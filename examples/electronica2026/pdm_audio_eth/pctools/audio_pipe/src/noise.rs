// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Speech-presence probability and noise statistics.
//! Stands in for the "ego-noise suppression" stage: with no proprioceptive state θ(t) available offline, the robot's
//! own noise (fan, servos) and the stationary room noise are tracked as a multichannel noise covariance Φn. Its
//! Cholesky factor whitens the array: the multichannel speech-presence probability (Souden et al.) is computed on the
//! whitened frame, localization votes in the whitened domain (so directional ego-noise stops winning the DOA), and
//! the MVDR nulls Φn.
use crate::common::*;

pub struct NoiseTracker {
    pub p: Vec<f32>,        // speech presence probability, per bin (current frame)
    pub phi_acc: Vec<Mat8>, // Σ γⁿ (1-p) x xᴴ
    pub phi_w: Vec<f32>,    // Σ γⁿ (1-p)
    pub chol: Vec<Mat8>,    // lower Cholesky factor of loaded Φn, per bin
    pub snr: Vec<f32>,      // whitened a-posteriori SNR (per bin, current frame)
    pbar: Vec<f32>,
    t: usize,
    init_frames: usize,
    m_eff: f32,
    xi_h1: f32,
    /// relative diagonal loading of Φn before whitening: small = null every noise direction (also weak ones that
    /// share a talker's direction), larger = only strong directional noise (ego noise) is whitened away
    pub load: f32,
    /// fixed ego-noise covariance Φego(f), learned offline from a robot-only take (HARK-style pre-recorded noise)
    pub ego: Option<Vec<Mat8>>,
    pub ego_mode: EgoMode,
    /// pseudo-frames of Φego mixed into Φn (prior mode); the adaptive part has ≈ 1/(1-γ) = 100 frames of memory
    pub ego_weight: f32,
    /// Cholesky factor used to whiten the DOA search (= chol, or Φego's factor in Doa mode)
    pub chol_doa: Vec<Mat8>,
}

#[derive(Clone, Copy, PartialEq, Debug, clap::ValueEnum)]
pub enum EgoMode {
    /// adaptive Φn only
    Off,
    /// Φn = (adaptive accumulator + w·Φego) / (weight + w): ego noise stays nulled everywhere
    Prior,
    /// DOA whitened by Φego only (GEVD-MUSIC with a fixed noise matrix); SPP, masks and MVDR stay adaptive
    Doa,
    /// Φn = adaptive + g·Φego everywhere: ego directions are always over-represented, ambient noise is kept
    Add,
    /// adaptive + g·Φego for the DOA whitening only
    AddDoa,
}

const GAMMA_PHI: f32 = 0.99; // noise covariance forgetting (~0.8 s of noise-only frames)

impl NoiseTracker {
    pub fn new(m_eff: f32, load: f32) -> Self {
        let mut eye = ZM;
        for i in 0..M {
            eye[i][i] = C::new(1.0, 0.0);
        }
        Self {
            p: vec![0.0; F],
            phi_acc: vec![ZM; F],
            phi_w: vec![0.0; F],
            chol: vec![eye; F], // identity until the first Φn factorisation
            snr: vec![0.0; F],
            pbar: vec![0.5; F],
            t: 0,
            init_frames: 16, // ~130 ms: assume the array starts without speech (ego noise is always on)
            m_eff,
            xi_h1: 10f32.powf(15.0 / 10.0) * m_eff, // fixed a-priori SNR under H1, multichannel (≈ M × 15 dB)
            load,
            ego: None,
            ego_mode: EgoMode::Off,
            ego_weight: 100.0,
            chol_doa: vec![eye; F],
        }
    }

    /// State bytes: per bin Φn accumulator + Cholesky factor (8×8 complex each) and a few scalars.
    pub fn mem_bytes(&self) -> usize {
        vbytes(&self.p) + vbytes(&self.phi_acc) + vbytes(&self.phi_w) + vbytes(&self.chol) + vbytes(&self.snr) + vbytes(&self.pbar)
    }

    /// Install a fixed ego-noise covariance.
    pub fn set_ego(&mut self, ego: Vec<Mat8>, mode: EgoMode, weight: f32) {
        self.ego = Some(ego);
        self.ego_mode = mode;
        self.ego_weight = weight;
        if mode == EgoMode::Doa {
            let e = self.ego.as_ref().unwrap();
            for f in 0..F {
                let ld = self.load * trace_re(&e[f]) / self.m_eff + 1e-20;
                if let Some(l) = chol(&e[f], ld) {
                    self.chol_doa[f] = l;
                }
            }
        }
    }

    pub fn whiten_doa(&self, f: usize, x: &Vec8) -> Vec8 {
        fwd_sub(&self.chol_doa[f], x)
    }

    /// Re-factor Φn (call every few frames).
    pub fn refresh(&mut self) {
        for f in 0..F {
            let a = self.phi_n(f);
            let ld = self.load * trace_re(&a) / self.m_eff + 1e-20;
            self.chol[f] = chol(&a, ld).unwrap_or_else(|| {
                let mut l = ZM;
                for i in 0..M {
                    l[i][i] = C::new((trace_re(&a) / self.m_eff).max(1e-20).sqrt(), 0.0);
                }
                l
            });
        }
        match (self.ego_mode, &self.ego) {
            (EgoMode::Doa, Some(_)) => {}
            (EgoMode::AddDoa, Some(e)) => {
                for f in 0..F {
                    let mut a = self.phi_n(f);
                    for i in 0..M {
                        for j in 0..M {
                            a[i][j] += e[f][i][j] * self.ego_weight;
                        }
                    }
                    let ld = self.load * trace_re(&a) / self.m_eff + 1e-20;
                    if let Some(l) = chol(&a, ld) {
                        self.chol_doa[f] = l;
                    }
                }
            }
            _ => self.chol_doa.clone_from(&self.chol),
        }
    }

    pub fn whiten(&self, f: usize, x: &Vec8) -> Vec8 {
        fwd_sub(&self.chol[f], x)
    }

    pub fn update(&mut self, x: &Frame) {
        let t = self.t;
        if t < self.init_frames {
            for f in 0..F {
                rank1_acc(&mut self.phi_acc[f], &x[f], 1.0);
                self.phi_w[f] += 1.0;
                self.p[f] = 0.0;
            }
            self.t += 1;
            if self.t == self.init_frames {
                self.refresh();
            }
            return;
        }
        let xi = self.xi_h1;
        for f in 0..F {
            let xw = self.whiten(f, &x[f]);
            // multichannel a-posteriori "SNR": xᴴ Φn⁻¹ x (expected ≈ M under noise only)
            let q: f32 = xw.iter().map(|v| v.norm_sqr()).sum();
            self.snr[f] = q / self.m_eff;
            // Souden et al. multichannel SPP, fixed ξ: ln Λ = ξ/(1+ξ)·π - ln(1+ξ), where π = |dᴴΦn⁻¹x|²/(dᴴΦn⁻¹d)
            // is the whitened power along the talker; without d, π ≈ q - (M-1) (the other M-1 whitened
            // dimensions hold unit-variance noise).
            let proj = (q - (self.m_eff - 1.0)).max(0.0);
            let lnl = proj * xi / (1.0 + xi) - (1.0 + xi).ln();
            let mut p = 1.0 / (1.0 + (-lnl).exp());
            self.pbar[f] = 0.9 * self.pbar[f] + 0.1 * p;
            if self.pbar[f] > 0.99 {
                p = p.min(0.99); // avoid stagnation
            }
            self.p[f] = p;
            let w = 1.0 - p;
            scale_mat(&mut self.phi_acc[f], GAMMA_PHI);
            rank1_acc(&mut self.phi_acc[f], &x[f], w);
            self.phi_w[f] = GAMMA_PHI * self.phi_w[f] + w;
        }
        self.t += 1;
    }

    /// Normalised noise covariance Φn(f).
    pub fn phi_n(&self, f: usize) -> Mat8 {
        let mut a = self.phi_acc[f];
        let mut w = self.phi_w[f];
        if let (Some(e), EgoMode::Add) = (&self.ego, self.ego_mode) {
            scale_mat(&mut a, 1.0 / w.max(1e-6));
            for i in 0..M {
                for j in 0..M {
                    a[i][j] += e[f][i][j] * self.ego_weight;
                }
            }
            return a;
        }
        if let (Some(e), EgoMode::Prior) = (&self.ego, self.ego_mode) {
            for i in 0..M {
                for j in 0..M {
                    a[i][j] += e[f][i][j] * self.ego_weight;
                }
            }
            w += self.ego_weight;
        }
        scale_mat(&mut a, 1.0 / w.max(1e-6));
        a
    }
}
