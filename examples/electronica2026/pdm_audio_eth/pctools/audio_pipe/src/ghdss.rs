// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! GHDSS separation (HARK's "Geometric High-order Decorrelation-based Source Separation", adaptive step size),
//! as an alternative to mask-MVDR. Per bin, the separation matrix W (C talkers × M mics) is updated every frame:
//!   W ← W − μ_SS J'_SS − μ_GC J'_GC
//!   E_SS = φ(y)yᴴ − diag(φ(y)yᴴ),  J'_SS = 2 E_SS φ̃(y) xᴴ        (higher-order decorrelation of the outputs)
//!   E_GC = W D − I,                J'_GC = 2 E_GC Dᴴ              (geometric constraint: unit gain to each talker)
//!   μ_SS = ‖E_SS‖² / (8‖J'_SS‖²),  μ_GC = ‖E_GC‖² / (8‖J'_GC‖²)    (adaptive step sizes, as in HARK)
//!   φ(y) = tanh(η|y|) e^{j∠y},  φ̃(y) = φ(y) + |y| ∂φ/∂|y|
//! It runs in the noise-whitened domain (x_w = L⁻¹x, D_w = L⁻¹D, so η = 1 is on a noise-normalised scale) and the
//! result is mapped back (W_o = W_w L⁻¹) into each stream's weights, so the post-filter and synthesis are unchanged.
//! `--ghdss-domain raw` instead runs it on x scaled by the noise level, closer to HARK (noise is left to the post-filter).
use crate::common::*;
use crate::noise::NoiseTracker;
use crate::sep::Stream;

#[derive(Clone, Copy, PartialEq, Debug, clap::ValueEnum)]
pub enum SepMode {
    /// mask-based MVDR (default)
    Mvdr,
    /// GHDSS adaptive separation (HARK)
    Ghdss,
}

#[derive(Clone, Copy, PartialEq, Debug, clap::ValueEnum)]
pub enum GhdssDomain {
    White,
    Raw,
}

#[derive(Clone)]
pub struct GhdssParams {
    pub eta: f32,
    pub domain: GhdssDomain,
    /// scale on the adaptive steps (1 = HARK's 1/8 rule)
    pub mu: f32,
    /// start a new talker's row from its mask-MVDR solution (else delay-and-sum, as HARK)
    pub init_mvdr: bool,
}

/// Per-stream GHDSS state: W_o rows (y = w_o · x, no conjugate), per bin.
pub struct GhdssRows {
    pub rows: Vec<Vec<Vec8>>, // [stream][f]
}

impl GhdssRows {
    pub fn new() -> Self {
        Self { rows: Vec::new() }
    }

    /// New talker: delay-and-sum row d^H / (d^H d).
    pub fn add_stream(&mut self, st: &Stream) {
        let r = (0..F)
            .map(|f| {
                let d = &st.steer[f];
                let n: f32 = d.iter().map(|c| c.norm_sqr()).sum::<f32>().max(1e-12);
                core::array::from_fn(|m| d[m].conj() / n)
            })
            .collect();
        self.rows.push(r);
    }

    /// New talker, row taken from the stream's current MVDR weights (s = wᴴx -> w_o = conj(w)).
    pub fn add_stream_from_w(&mut self, st: &Stream) {
        self.rows.push((0..F).map(|f| core::array::from_fn(|m| st.w[f][m].conj())).collect());
    }

    /// One adaptation step on frame x (and its whitened version xw); writes MVDR-convention weights into the streams.
    pub fn step(&mut self, x: &Frame, xw: &Frame, noise: &NoiseTracker, streams: &mut [Stream], prm: &GhdssParams, update: Option<&[bool]>) {
        let c = streams.len();
        if c == 0 {
            return;
        }
        for f in 1..F {
            // domain: x_d = T x with T = L⁻¹ (white) or 1/σ (raw); D_d = T D; W_d = W_o T⁻¹
            let l = &noise.chol[f];
            let (xd, sig) = match prm.domain {
                GhdssDomain::White => (xw[f], 1.0),
                GhdssDomain::Raw => {
                    let s = (trace_re(&noise.phi_n(f)) / M as f32).max(1e-20).sqrt();
                    (core::array::from_fn(|m| x[f][m] / s), s)
                }
            };
            let mut wd: Vec<Vec8> = Vec::with_capacity(c);
            let mut dd: Vec<Vec8> = Vec::with_capacity(c);
            for (k, st) in streams.iter().enumerate() {
                let wo = &self.rows[k][f];
                wd.push(match prm.domain {
                    // w_d[j] = Σ_i w_o[i] L[i][j]   (W_o L)
                    GhdssDomain::White => core::array::from_fn(|j| (j..M).map(|i| wo[i] * l[i][j]).fold(ZERO, |a, b| a + b)),
                    GhdssDomain::Raw => core::array::from_fn(|j| wo[j] * sig),
                });
                dd.push(match prm.domain {
                    GhdssDomain::White => st.steer_w[f],
                    GhdssDomain::Raw => core::array::from_fn(|m| st.steer[f][m] / sig),
                });
            }
            // outputs and nonlinearity
            let y: Vec<C> = wd.iter().map(|w| (0..M).map(|j| w[j] * xd[j]).fold(ZERO, |a, b| a + b)).collect();
            let phi: Vec<C> = y.iter().map(|v| { let a = v.norm(); if a > 1e-20 { v * ((prm.eta * a).tanh() / a) } else { ZERO } }).collect();
            let phit: Vec<C> = y
                .iter()
                .map(|v| {
                    let a = v.norm();
                    if a > 1e-20 {
                        let t = (prm.eta * a).tanh();
                        v * ((t + prm.eta * a * (1.0 - t * t)) / a)
                    } else {
                        ZERO
                    }
                })
                .collect();
            // E_SS φ̃(y) (C-vector) and ‖E_SS‖²
            let mut ess_n = 0f32;
            let mut ep = vec![ZERO; c];
            for a in 0..c {
                for b in 0..c {
                    if a != b {
                        let e = phi[a] * y[b].conj();
                        ess_n += e.norm_sqr();
                        ep[a] += e * phit[b];
                    }
                }
            }
            let xn: f32 = xd.iter().map(|v| v.norm_sqr()).sum();
            // J_SS[a][m] = 2 ep[a] conj(x[m]);  ‖J_SS‖² = 4 Σ|ep|² ‖x‖²
            let jss_n: f32 = 4.0 * ep.iter().map(|v| v.norm_sqr()).sum::<f32>() * xn;
            let mu_ss = if jss_n > 0.0 { prm.mu * ess_n / (8.0 * jss_n) } else { 0.0 };
            // E_GC = W D − I ; J_GC = 2 E_GC Dᴴ
            let mut egc = vec![vec![ZERO; c]; c];
            let mut egc_n = 0f32;
            for a in 0..c {
                for k in 0..c {
                    let mut s = (0..M).map(|j| wd[a][j] * dd[k][j]).fold(ZERO, |p, q| p + q);
                    if a == k {
                        s -= C::new(1.0, 0.0);
                    }
                    egc_n += s.norm_sqr();
                    egc[a][k] = s;
                }
            }
            let mut jgc = vec![ZV; c];
            let mut jgc_n = 0f32;
            for a in 0..c {
                for m in 0..M {
                    let mut s = ZERO;
                    for k in 0..c {
                        s += egc[a][k] * dd[k][m].conj();
                    }
                    jgc[a][m] = s * 2.0;
                    jgc_n += jgc[a][m].norm_sqr();
                }
            }
            let mu_gc = if jgc_n > 0.0 { prm.mu * egc_n / (8.0 * jgc_n) } else { 0.0 };
            for a in 0..c {
                if let Some(u) = update {
                    if !u[a] {
                        continue;
                    }
                }
                let mut w = wd[a];
                for m in 0..M {
                    w[m] -= ep[a] * xd[m].conj() * (2.0 * mu_ss) + jgc[a][m] * mu_gc;
                }
                if !w.iter().all(|v| v.re.is_finite() && v.im.is_finite()) {
                    continue;
                }
                // back to the original domain: W_o = W_d T
                let wo: Vec8 = match prm.domain {
                    // w_o L = w_d  ->  Lᵀ w_oᵀ = w_dᵀ (back substitution, plain transpose)
                    GhdssDomain::White => {
                        let mut z = ZV;
                        for i in (0..M).rev() {
                            let mut s = w[i];
                            for k in i + 1..M {
                                s -= l[k][i] * z[k];
                            }
                            z[i] = s / l[i][i].re;
                        }
                        z
                    }
                    GhdssDomain::Raw => core::array::from_fn(|m| w[m] / sig),
                };
                self.rows[a][f] = wo;
            }
        }
        for (k, st) in streams.iter_mut().enumerate() {
            for f in 1..F {
                st.w[f] = core::array::from_fn(|m| self.rows[k][f][m].conj()); // MVDR convention: s = wᴴx
            }
            st.have_w = true;
        }
    }
}
