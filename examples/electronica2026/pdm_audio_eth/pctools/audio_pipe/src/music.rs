// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Local SEVD-MUSIC refinement of each talker's azimuth (after HARK's LocalizeMUSIC).
//! The whitened vote histogram + tracker decides *whether* there is a talker; this decides *where* exactly.
//! R(f) = recursive x̂ x̂ᴴ average over ≈ 50 frames, bins 500-2800 Hz. Every `every` frames the signal subspace (top Ns
//! eigenvectors) of each R(f) is refreshed by warm-started orthogonal iteration, and for a talker near θ0 the broadband
//! pseudo-spectrum  P(θ) = Σ_f sqrt(λ1(f)) ‖d‖² / (‖d‖² − Σ_i |e_iᴴd|²)  is searched over θ0 ± `span` (1° steps).
use crate::common::*;
use crate::doa::Geometry;

pub struct MusicRefiner {
    bins: Vec<usize>,
    r: Vec<Mat8>,
    q: Vec<Vec<Vec8>>, // per band bin: Ns signal-subspace vectors (warm start)
    lam1: Vec<f32>,
    gamma: f32,
    pub ns: usize,
    pub span: f32,
    pub every: usize,
    pub alpha: f32, // smoothing of the per-stream offset
}

impl MusicRefiner {
    pub fn new(ns: usize, span: f32, every: usize, alpha: f32, win_frames: f32) -> Self {
        let bins: Vec<usize> = (0..F).filter(|&f| { let hz = f as f32 * FS as f32 / N as f32; (500.0..=2800.0).contains(&hz) }).collect();
        let q = bins
            .iter()
            .map(|_| (0..ns).map(|i| core::array::from_fn(|m| if m == i { C::new(1.0, 0.0) } else { ZERO })).collect())
            .collect();
        Self { r: vec![ZM; bins.len()], lam1: vec![0.0; bins.len()], bins, q, gamma: 1.0 - 1.0 / win_frames.max(2.0), ns, span, every, alpha }
    }

    pub fn update(&mut self, x: &Frame) {
        for (k, &f) in self.bins.iter().enumerate() {
            scale_mat(&mut self.r[k], self.gamma);
            rank1_acc(&mut self.r[k], &x[f], 1.0 - self.gamma);
        }
    }

    /// Refresh the signal subspaces (a few orthogonal-iteration sweeps from the previous estimate).
    pub fn refresh(&mut self) {
        let ns = self.ns;
        for k in 0..self.bins.len() {
            let r = &self.r[k];
            let q = &mut self.q[k];
            for _ in 0..3 {
                let mut z: Vec<Vec8> = q.iter().map(|v| core::array::from_fn(|i| (0..M).map(|j| r[i][j] * v[j]).fold(ZERO, |a, b| a + b))).collect();
                for a in 0..ns {
                    for b in 0..a {
                        let p = dot_h(&z[b], &z[a]);
                        let zb = z[b];
                        for m in 0..M {
                            z[a][m] -= zb[m] * p;
                        }
                    }
                    let n = z[a].iter().map(|c| c.norm_sqr()).sum::<f32>().sqrt();
                    if n > 1e-30 {
                        for m in 0..M {
                            z[a][m] /= n;
                        }
                    }
                }
                *q = z;
            }
            self.lam1[k] = quad(r, &q[0]).max(0.0);
        }
    }

    fn p_at(&self, geo: &Geometry, az: f32) -> f32 {
        let mut p = 0.0;
        for (k, &f) in self.bins.iter().enumerate() {
            let d = geo.steer(az, f);
            let dn: f32 = d.iter().map(|c| c.norm_sqr()).sum();
            let proj: f32 = self.q[k].iter().map(|e| dot_h(e, &d).norm_sqr()).sum();
            p += self.lam1[k].sqrt() * dn / (dn - proj).max(1e-6 * dn);
        }
        p
    }

    /// Azimuth of the MUSIC peak within az0 ± span (parabolic interpolation), or None if the peak sits on the edge
    /// or is not at least `min_peak` × the mean of the searched span.
    pub fn refine(&self, geo: &Geometry, az0: f32) -> Option<f32> {
        self.refine_gated(geo, az0, 0.0)
    }

    pub fn refine_gated(&self, geo: &Geometry, az0: f32, min_peak: f32) -> Option<f32> {
        let n = (2.0 * self.span) as i32;
        let ps: Vec<f32> = (0..=n).map(|i| self.p_at(geo, wrap_deg(az0 - self.span + i as f32))).collect();
        let (bi, _) = ps.iter().enumerate().max_by(|a, b| a.1.total_cmp(b.1))?;
        if bi == 0 || bi == n as usize {
            return None;
        }
        let mean = ps.iter().sum::<f32>() / ps.len() as f32;
        if ps[bi] < min_peak * mean {
            return None;
        }
        let (a, b, c) = (ps[bi - 1], ps[bi], ps[bi + 1]);
        let den = a - 2.0 * b + c;
        let off = if den.abs() > 1e-20 { 0.5 * (a - c) / den } else { 0.0 };
        Some(wrap_deg(az0 - self.span + bi as f32 + off.clamp(-0.5, 0.5)))
    }

    pub fn mem_bytes(&self) -> usize {
        vbytes(&self.r) + self.q.iter().map(|v| vbytes(v)).sum::<usize>() + vbytes(&self.lam1) + vbytes(&self.bins)
    }
}
