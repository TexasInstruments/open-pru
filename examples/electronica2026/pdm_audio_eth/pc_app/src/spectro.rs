// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Spectrogram columns as in the pru-simulator Audio view: 256-point Hann,
//! hop 128 (8 ms at 16 kHz), -90 to 0 dB, a full-scale sine reads 0 dB.

use std::collections::VecDeque;
use std::sync::Arc;

use rustfft::num_complex::Complex32;
use rustfft::{Fft, FftPlanner};

/// Columns kept: 10 s at hop 128 / 16 kHz.
pub const COLUMNS: usize = 1250;

/// One channel's spectrogram.
pub struct Spectrogram {
    fft_size: usize,
    hop: usize,
    min_db: f32,
    max_db: f32,
    scale: f32,
    window: Vec<f32>,
    fft: Arc<dyn Fft<f32>>,
    buf: Vec<f32>,
    work: Vec<Complex32>,
    cols: VecDeque<Vec<f32>>,
    /// Columns added since the last `take_added` (for texture updates).
    added: usize,
}

impl Spectrogram {
    /// `full_scale`: the sample value of a 0 dB sine amplitude.
    pub fn new(fft_size: usize, hop: usize, min_db: f32, max_db: f32, full_scale: f32) -> Self {
        let window = (0..fft_size)
            .map(|i| 0.5 - 0.5 * (2.0 * std::f32::consts::PI * i as f32 / fft_size as f32).cos())
            .collect();
        Spectrogram {
            fft_size,
            hop,
            min_db,
            max_db,
            scale: 1.0 / (full_scale * fft_size as f32 / 4.0),
            window,
            fft: FftPlanner::new().plan_fft_forward(fft_size),
            buf: Vec::with_capacity(fft_size * 4),
            work: vec![Complex32::new(0.0, 0.0); fft_size],
            cols: VecDeque::with_capacity(COLUMNS),
            added: 0,
        }
    }

    /// Append samples; every `hop` samples (once `fft_size` are there) one
    /// column of `fft_size / 2 + 1` dB values is added.
    pub fn push(&mut self, samples: &[f32]) {
        self.buf.extend_from_slice(samples);
        let mut start = 0;
        while self.buf.len() - start >= self.fft_size {
            for (i, w) in self.work.iter_mut().enumerate() {
                *w = Complex32::new(self.buf[start + i] * self.window[i], 0.0);
            }
            self.fft.process(&mut self.work);
            let col: Vec<f32> = self.work[..self.fft_size / 2 + 1]
                .iter()
                .map(|c| (20.0 * (c.norm() * self.scale).max(1e-12).log10()).clamp(self.min_db, self.max_db))
                .collect();
            if self.cols.len() == COLUMNS {
                self.cols.pop_front();
            }
            self.cols.push_back(col);
            self.added += 1;
            start += self.hop;
        }
        self.buf.drain(..start);
    }

    /// The kept columns, oldest first.
    pub fn columns(&self) -> &VecDeque<Vec<f32>> {
        &self.cols
    }

    /// Columns added since the last call (at most `COLUMNS`).
    pub fn take_added(&mut self) -> usize {
        std::mem::take(&mut self.added).min(COLUMNS)
    }

    /// dB range, for colour mapping.
    pub fn range(&self) -> (f32, f32) {
        (self.min_db, self.max_db)
    }
}

/// Per-bin dB difference `a - b` (input minus output).
pub fn diff_column(a: &[f32], b: &[f32]) -> Vec<f32> {
    a.iter().zip(b).map(|(x, y)| x - y).collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tone(f: f64, amp: f32, n: usize) -> Vec<f32> {
        (0..n).map(|i| amp * (2.0 * std::f64::consts::PI * f * i as f64 / 16000.0).sin() as f32).collect()
    }

    fn s() -> Spectrogram {
        Spectrogram::new(256, 128, -90.0, 0.0, 1.0)
    }

    #[test]
    fn spectro_tone_peak_bin() {
        let mut sp = s();
        sp.push(&tone(1000.0, 0.5, 4000));
        let col = sp.columns().back().unwrap();
        assert_eq!(col.len(), 129);
        let peak = col.iter().enumerate().max_by(|a, b| a.1.partial_cmp(b.1).unwrap()).unwrap();
        assert_eq!(peak.0, 16);
        assert!((peak.1 - -6.02).abs() < 0.5, "0.5 FS sine at {} dB", peak.1);
    }

    #[test]
    fn spectro_hop_columns() {
        let mut sp = s();
        sp.push(&tone(500.0, 0.1, 10_000));
        sp.push(&tone(500.0, 0.1, 6_000)); // in two pieces
        assert_eq!(sp.columns().len(), 124);
    }

    #[test]
    fn spectro_keeps_ten_seconds() {
        let mut sp = s();
        sp.push(&vec![0.0; 16000 * 12]);
        assert_eq!(sp.columns().len(), COLUMNS);
    }

    #[test]
    fn spectro_db_clamped() {
        let mut sp = s();
        sp.push(&vec![0.0; 512]);
        assert!(sp.columns().iter().flatten().all(|v| *v == -90.0));
        let mut sp = s();
        sp.push(&tone(2000.0, 4.0, 512)); // 12 dB over full scale
        assert!(sp.columns().iter().flatten().all(|v| (-90.0..=0.0).contains(v)));
    }

    #[test]
    fn diff_column_zero_for_equal() {
        let a = vec![-10.0, -50.0, -90.0];
        assert_eq!(diff_column(&a, &a), vec![0.0, 0.0, 0.0]);
        assert_eq!(diff_column(&a, &[-20.0, -40.0, -90.0]), vec![10.0, -10.0, 0.0]);
    }

    #[test]
    fn spectro_8ch_realtime_budget() {
        if cfg!(debug_assertions) {
            return; // timing only means something in --release
        }
        let x = tone(1234.0, 0.3, 16000);
        let mut sps: Vec<Spectrogram> = (0..8).map(|_| s()).collect();
        let t = std::time::Instant::now();
        for sp in &mut sps {
            sp.push(&x);
        }
        assert!(t.elapsed() < std::time::Duration::from_millis(50), "{:?}", t.elapsed());
    }
}
