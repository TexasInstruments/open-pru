// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Start-up channel health check. On a 5 cm ring, adjacent mics (3.8 cm apart) are highly coherent below 2 kHz for
//! any sound field (diffuse coherence sinc(kd) > 0.7), so a channel that is incoherent with BOTH neighbours while the
//! rest of the ring is coherent is dead, mis-wired, or from another recording. It is excluded from the array.
use crate::common::*;
use realfft::RealFftPlanner;

pub struct Health {
    pub adj_msc: Vec<f32>, // MSC(m, m+1), m = 0..M-1 (wraps)
    pub bad: Vec<usize>,
}

pub fn check(chans: &[Vec<f32>], ms: usize) -> Health {
    let len = (ms * FS / 1000).min(chans[0].len());
    let win = crate::stft::sqrt_hann();
    let fft = RealFftPlanner::<f32>::new().plan_fft_forward(N);
    let mut buf = vec![0f32; N];
    let mut spec = vec![ZERO; F];
    let lo = (200 * N) / FS;
    let hi = (2000 * N) / FS;
    let nb = hi - lo + 1;
    // per-channel STFT over the window
    let mut s: Vec<Vec<Vec<C>>> = vec![Vec::new(); M];
    let mut t = 0;
    while t + N <= len {
        for m in 0..M {
            for n in 0..N {
                buf[n] = chans[m][t + n] * win[n] * win[n];
            }
            fft.process(&mut buf, &mut spec).unwrap();
            s[m].push(spec[lo..=hi].to_vec());
        }
        t += H;
    }
    let msc = |a: usize, b: usize| -> f32 {
        let mut tot = 0.0;
        for k in 0..nb {
            let (mut sab, mut saa, mut sbb) = (ZERO, 0.0f32, 0.0f32);
            for fr in 0..s[a].len() {
                sab += s[a][fr][k] * s[b][fr][k].conj();
                saa += s[a][fr][k].norm_sqr();
                sbb += s[b][fr][k].norm_sqr();
            }
            tot += sab.norm_sqr() / (saa * sbb).max(1e-30);
        }
        tot / nb as f32
    };
    let adj: Vec<f32> = (0..M).map(|m| msc(m, (m + 1) % M)).collect();
    let mut sorted = adj.clone();
    sorted.sort_by(|a, b| a.total_cmp(b));
    let median = sorted[M / 2];
    let mut bad = Vec::new();
    for m in 0..M {
        let left = adj[(m + M - 1) % M];
        let right = adj[m];
        let silent = chans[m][..len].iter().all(|&v| v == 0.0);
        if silent || (left < 0.5 * median && right < 0.5 * median && median > 0.4) {
            bad.push(m);
        }
    }
    Health { adj_msc: adj, bad }
}
