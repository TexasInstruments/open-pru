// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Online WPE dereverberation, recursive (RLS) inverse-covariance form (impl. note rev B, sec. 5).
//! Per bin, regressor ỹ = [y_{t-Δ} .. y_{t-Δ-K+1}] (MK), x̂ = y - Gᴴỹ,
//! g = P ỹ, k = g / (αλ + ỹᴴg), G += k x̂ᴴ, P = (P - k gᴴ)/α.  Bins are independent -> rayon.
use crate::common::*;
use rayon::prelude::*;

pub struct WpeParams {
    pub taps: usize,  // K
    pub delay: usize, // Δ
    pub alpha: f32,   // forgetting factor
    pub enabled: bool,
}

struct Bin {
    p: Vec<C>,       // MK x MK row-major, Hermitian
    g: Vec<C>,       // MK x M row-major
    hist: Vec<Vec8>, // ring of the last Δ+K frames of y for this bin
    head: usize,
    pavg: f32, // slow average power, for the λ floor
    // scratch
    reg: Vec<C>,
    gk: Vec<C>,
}

pub struct Wpe {
    prm: WpeParams,
    bins: Vec<Bin>,
    mk: usize,
    t: usize,
}

impl Wpe {
    pub fn new(prm: WpeParams) -> Self {
        let mk = M * prm.taps;
        let len = prm.delay + prm.taps;
        let bins = (0..F)
            .map(|_| {
                let mut p = vec![ZERO; mk * mk];
                for i in 0..mk {
                    p[i * mk + i] = C::new(1.0, 0.0); // δ⁻¹ I with δ = 1; λ-normalised regressors make this scale-free
                }
                Bin { p, g: vec![ZERO; mk * M], hist: vec![ZV; len], head: 0, pavg: 0.0, reg: vec![ZERO; mk], gk: vec![ZERO; mk] }
            })
            .collect();
        Self { prm, bins, mk, t: 0 }
    }

    /// State bytes: per bin P (MK×MK), G (MK×M), Δ+K frame ring, scratch.
    pub fn mem_bytes(&self) -> usize {
        self.bins.iter().map(|b| vbytes(&b.p) + vbytes(&b.g) + vbytes(&b.hist) + vbytes(&b.reg) + vbytes(&b.gk) + 16).sum()
    }

    /// In-place: y (STFT frame) -> x̂ (dereverberated frame).
    pub fn process(&mut self, frame: &mut Frame) {
        if !self.prm.enabled {
            return;
        }
        let (k, d, alpha, mk) = (self.prm.taps, self.prm.delay, self.prm.alpha, self.mk);
        let t = self.t;
        let warm = t >= d + k;
        frame.par_iter_mut().zip(self.bins.par_iter_mut()).enumerate().for_each(|(f, (y, b))| {
            let len = b.hist.len();
            let pw: f32 = y.iter().map(|v| v.norm_sqr()).sum::<f32>() / M as f32;
            b.pavg = if t == 0 { pw } else { 0.995 * b.pavg + 0.005 * pw };
            if warm && f > 0 {
                // regressor: frames t-Δ ... t-Δ-K+1 ; hist[head-1] is t-1
                for tap in 0..k {
                    let idx = (b.head + len - d - tap) % len;
                    b.reg[tap * M..(tap + 1) * M].copy_from_slice(&b.hist[idx]);
                }
                // x̂ = y - Gᴴ ỹ
                let mut xh = *y;
                for i in 0..mk {
                    let r = b.reg[i];
                    let row = &b.g[i * M..(i + 1) * M];
                    for m in 0..M {
                        xh[m] -= row[m].conj() * r;
                    }
                }
                // λ from the observed power (as nara_wpe online). Using the a-priori error |x̂|² instead lets an
                // over-fitted predictor shrink λ, up-weight those frames and run away (it added energy at HF).
                let lam = pw.max(1e-4 * b.pavg).max(1e-12);
                // g = P ỹ
                let mut den = 0.0f32;
                for i in 0..mk {
                    let row = &b.p[i * mk..(i + 1) * mk];
                    let mut s = ZERO;
                    for j in 0..mk {
                        s += row[j] * b.reg[j];
                    }
                    b.gk[i] = s;
                    den += (b.reg[i].conj() * s).re;
                }
                let den = alpha * lam + den.max(0.0);
                let inv = 1.0 / den;
                let ia = 1.0 / alpha;
                // G += k x̂ᴴ   (k = g/den)
                for i in 0..mk {
                    let ki = b.gk[i] * inv;
                    let row = &mut b.g[i * M..(i + 1) * M];
                    for m in 0..M {
                        row[m] += ki * xh[m].conj();
                    }
                }
                // P = (P - g gᴴ/den)/α
                for i in 0..mk {
                    let gi = b.gk[i] * inv;
                    let row = &mut b.p[i * mk..(i + 1) * mk];
                    for j in 0..mk {
                        row[j] = (row[j] - gi * b.gk[j].conj()) * ia;
                    }
                }
                // re-symmetrise occasionally against f32 drift
                if t % 64 == 0 {
                    for i in 0..mk {
                        for j in i + 1..mk {
                            let a = (b.p[i * mk + j] + b.p[j * mk + i].conj()) * 0.5;
                            b.p[i * mk + j] = a;
                            b.p[j * mk + i] = a.conj();
                        }
                        b.p[i * mk + i].im = 0.0;
                    }
                }
                b.hist[b.head] = *y;
                *y = xh;
            } else {
                b.hist[b.head] = *y;
            }
            b.head = (b.head + 1) % len;
        });
        self.t += 1;
    }
}
