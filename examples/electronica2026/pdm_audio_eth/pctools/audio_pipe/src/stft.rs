// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Streaming STFT analysis (8 channels) and per-stream synthesis.
//! sqrt-Hann analysis + synthesis, N = 512, H = 128: overlap-add gain 2.0, so OLA is scaled by 1/2.
use crate::common::*;
use realfft::{ComplexToReal, RealFftPlanner, RealToComplex};
use std::sync::Arc;

pub fn sqrt_hann() -> Vec<f32> {
    (0..N).map(|n| (0.5 - 0.5 * (2.0 * std::f32::consts::PI * n as f32 / N as f32).cos()).sqrt()).collect()
}

pub struct Analyzer {
    win: Vec<f32>,
    fft: Arc<dyn RealToComplex<f32>>,
    buf: Vec<Vec<f32>>, // M x N, last N input samples per channel
    inbuf: Vec<f32>,
    spec: Vec<C>,
    scratch: Vec<C>,
}

impl Analyzer {
    pub fn new() -> Self {
        let fft = RealFftPlanner::<f32>::new().plan_fft_forward(N);
        let scratch = fft.make_scratch_vec();
        Self { win: sqrt_hann(), inbuf: vec![0.0; N], spec: vec![ZERO; F], scratch, fft, buf: vec![vec![0.0; N]; M] }
    }

    /// State bytes (window, M input rings, FFT buffers; FFT twiddles ≈ N/2 complex).
    pub fn mem_bytes(&self) -> usize {
        vbytes(&self.win) + self.buf.iter().map(|b| vbytes(b)).sum::<usize>() + vbytes(&self.inbuf) + vbytes(&self.spec) + vbytes(&self.scratch) + N / 2 * 8
    }

    /// Push one hop (H samples per channel) and write the frame into `out` (len F).
    pub fn push(&mut self, hop: &[&[f32]], out: &mut Frame) {
        for m in 0..M {
            let b = &mut self.buf[m];
            b.copy_within(H.., 0);
            b[N - H..].copy_from_slice(hop[m]);
            for n in 0..N {
                self.inbuf[n] = b[n] * self.win[n];
            }
            self.fft.process_with_scratch(&mut self.inbuf, &mut self.spec, &mut self.scratch).unwrap();
            for f in 0..F {
                out[f][m] = self.spec[f];
            }
        }
    }
}

pub struct Synth {
    win: Vec<f32>,
    ifft: Arc<dyn ComplexToReal<f32>>,
    ola: Vec<f32>,
    spec: Vec<C>,
    out: Vec<f32>,
    scratch: Vec<C>,
}

impl Synth {
    pub fn new() -> Self {
        let ifft = RealFftPlanner::<f32>::new().plan_fft_inverse(N);
        let scratch = ifft.make_scratch_vec();
        Self { win: sqrt_hann(), ola: vec![0.0; N], spec: vec![ZERO; F], out: vec![0.0; N], scratch, ifft }
    }

    /// State bytes (window, OLA ring, FFT buffers, twiddles).
    pub fn mem_bytes(&self) -> usize {
        vbytes(&self.win) + vbytes(&self.ola) + vbytes(&self.spec) + vbytes(&self.out) + vbytes(&self.scratch) + N / 2 * 8
    }

    /// Push one spectrum; returns the H finished samples. The first finished sample of the frame that
    /// ended at input sample (t+1)H is input sample (t+1)H - N (algorithmic delay N - H).
    pub fn push(&mut self, spec: &[C], dst: &mut Vec<f32>) {
        self.spec.copy_from_slice(spec);
        self.spec[0].im = 0.0;
        self.spec[F - 1].im = 0.0;
        self.ifft.process_with_scratch(&mut self.spec, &mut self.out, &mut self.scratch).unwrap();
        let g = 0.5 / N as f32; // 1/N for the unnormalised IFFT, 1/2 for the OLA gain
        for n in 0..N {
            self.ola[n] += self.out[n] * self.win[n] * g;
        }
        dst.extend_from_slice(&self.ola[..H]);
        self.ola.copy_within(H.., 0);
        self.ola[N - H..].fill(0.0);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn perfect_reconstruction() {
        let len = 16 * H;
        let x: Vec<f32> = (0..len).map(|n| ((n * 37 % 101) as f32 / 50.0 - 1.0) * 0.5).collect();
        let mut a = Analyzer::new();
        let mut s = Synth::new();
        let mut fr: Frame = vec![ZV; F];
        let mut y = Vec::new();
        for t in 0..len / H {
            let hop = &x[t * H..(t + 1) * H];
            let hops: Vec<&[f32]> = (0..M).map(|_| hop).collect();
            a.push(&hops, &mut fr);
            let ch0: Vec<C> = fr.iter().map(|v| v[0]).collect();
            s.push(&ch0, &mut y);
        }
        // y[k] corresponds to x[k - (N - H)]
        let d = N - H;
        for k in N..len {
            assert!((y[k] - x[k - d]).abs() < 1e-4, "k={k} {} vs {}", y[k], x[k - d]);
        }
    }
}
