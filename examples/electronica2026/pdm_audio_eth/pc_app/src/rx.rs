// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! AUDIO_OUT reconstruction by sample index (design 6.2), loss and
//! resync counters, board clock estimate, and WAV capture.

use std::collections::{BTreeMap, VecDeque};
use std::io;
use std::path::Path;

use crate::proto::{Status, AUDIO_OUT_FRAMES};

/// Out-of-order frames held back before a missing one is declared lost.
const REORDER: usize = 4;
/// An index jump of more than this (1 s at 16 kHz) is a resync, not a gap.
const RESYNC_FRAMES: u64 = 16_000;
/// Window of the board clock estimate.
const CLOCK_WINDOW_NS: u64 = 10_000_000_000;

/// Rebuilds the 8-channel PCM stream from AUDIO_OUT frames.
pub struct OutStream {
    next: Option<u64>,
    pending: BTreeMap<u64, [[i32; 8]; 32]>,
    out: Vec<(u64, [i32; 8])>,
    clock: VecDeque<(u64, u64)>,
    last: Option<Status>,
    /// AUDIO_OUT frames (32 PCM frames each) that never arrived.
    pub lost_frames: u64,
    /// AUDIO_OUT frames received twice.
    pub duplicates: u64,
    /// Index jumps (board reset or reload) after which the stream restarted.
    pub resyncs: u64,
}

impl Default for OutStream {
    fn default() -> Self {
        Self::new()
    }
}

impl OutStream {
    pub fn new() -> Self {
        OutStream {
            next: None,
            pending: BTreeMap::new(),
            out: Vec::new(),
            clock: VecDeque::new(),
            last: None,
            lost_frames: 0,
            duplicates: 0,
            resyncs: 0,
        }
    }

    fn restart(&mut self, at: u64) {
        self.pending.clear();
        self.next = Some(at - at % 64);
        self.clock.clear();
    }

    /// Add one AUDIO_OUT frame: its status, 32 PCM frames, the header `ts`
    /// (board IEP ns) and the PC receive time.
    pub fn push(&mut self, status: &Status, pcm: &[[i32; 8]; 32], tx_iep: u64, pc_rx_ns: u64) {
        let i = status.first_index as u64;
        self.last = Some(*status);
        match self.next {
            None => self.restart(i),
            Some(n) if i + (AUDIO_OUT_FRAMES as u64) <= n => {
                if n - i > RESYNC_FRAMES {
                    self.resyncs += 1;
                    self.restart(i);
                } else {
                    self.duplicates += 1;
                    return;
                }
            }
            Some(n) if i > n + RESYNC_FRAMES => {
                self.resyncs += 1;
                self.restart(i);
            }
            _ => {}
        }
        if self.pending.insert(i, *pcm).is_some() {
            self.duplicates += 1;
        }
        self.clock.push_back((pc_rx_ns, tx_iep));
        while self.clock.front().is_some_and(|(t, _)| pc_rx_ns.saturating_sub(*t) > CLOCK_WINDOW_NS) {
            self.clock.pop_front();
        }
        self.drain();
    }

    fn drain(&mut self) {
        let Some(mut n) = self.next else { return };
        loop {
            if let Some(b) = self.pending.remove(&n) {
                for (k, f) in b.iter().enumerate() {
                    self.out.push((n + k as u64, *f));
                }
                n += AUDIO_OUT_FRAMES as u64;
                continue;
            }
            if self.pending.len() < REORDER {
                break;
            }
            let k = *self.pending.keys().next().unwrap();
            let missing = k - n;
            self.lost_frames += missing.div_ceil(AUDIO_OUT_FRAMES as u64);
            for j in n..k {
                self.out.push((j, [0; 8]));
            }
            n = k;
        }
        self.next = Some(n);
    }

    /// PCM frames completed since the last call, contiguous by index;
    /// lost frames are filled with zeros.
    pub fn take_new(&mut self) -> Vec<(u64, [i32; 8])> {
        std::mem::take(&mut self.out)
    }

    /// Board IEP clock against the PC clock over the last 10 s, ppm
    /// (positive: board fast). `None` with less than 1 s of data.
    pub fn board_clock_ppm(&self) -> Option<f64> {
        // least-squares slope of board time over PC time, relative to the
        // first sample (robust against the jitter of single frames)
        let (p0, b0) = *self.clock.front()?;
        let (p1, _) = *self.clock.back()?;
        if p1.saturating_sub(p0) < 1_000_000_000 {
            return None;
        }
        let n = self.clock.len() as f64;
        let (mut sx, mut sy, mut sxx, mut sxy) = (0.0, 0.0, 0.0, 0.0);
        for (p, b) in &self.clock {
            let x = (p - p0) as f64;
            let y = b.wrapping_sub(b0) as i64 as f64 - x;
            sx += x;
            sy += y;
            sxx += x * x;
            sxy += x * y;
        }
        let d = n * sxx - sx * sx;
        if d <= 0.0 {
            return None;
        }
        Some((n * sxy - sx * sy) / d * 1e6)
    }

    /// The newest status block.
    pub fn last_status(&self) -> Option<&Status> {
        self.last.as_ref()
    }
}

/// Eight mono WAV files `<prefix>_ch0.wav` .. `<prefix>_ch7.wav`, 16 kHz.
pub struct WavCapture {
    w: Vec<hound::WavWriter<io::BufWriter<std::fs::File>>>,
    bits: u16,
}

fn herr(e: hound::Error) -> io::Error {
    io::Error::other(e.to_string())
}

impl WavCapture {
    /// 24-bit files (board output; the PCM is 24-bit saturated).
    pub fn create(dir: &Path, prefix: &str) -> io::Result<Self> {
        Self::create_bits(dir, prefix, 24)
    }

    /// Files with `bits` per sample (16 for the PC input).
    pub fn create_bits(dir: &Path, prefix: &str, bits: u16) -> io::Result<Self> {
        std::fs::create_dir_all(dir)?;
        let spec = hound::WavSpec {
            channels: 1,
            sample_rate: 16_000,
            bits_per_sample: bits,
            sample_format: hound::SampleFormat::Int,
        };
        let w = (0..8)
            .map(|c| hound::WavWriter::create(dir.join(format!("{prefix}_ch{c}.wav")), spec).map_err(herr))
            .collect::<io::Result<Vec<_>>>()?;
        Ok(WavCapture { w, bits })
    }

    /// Append frames (values clamped to the sample width).
    pub fn write(&mut self, frames: &[(u64, [i32; 8])]) -> io::Result<()> {
        let max = (1i32 << (self.bits - 1)) - 1;
        for (_, f) in frames {
            for (c, w) in self.w.iter_mut().enumerate() {
                w.write_sample(f[c].clamp(-max - 1, max)).map_err(herr)?;
            }
        }
        Ok(())
    }

    /// Close the files (writes the WAV headers).
    pub fn finish(self) -> io::Result<()> {
        for w in self.w {
            w.finalize().map_err(herr)?;
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn st(first_index: u32) -> Status {
        Status { first_index, clip_frames: 16000, ..Status::default() }
    }

    fn block(first: u32) -> [[i32; 8]; 32] {
        let mut b = [[0i32; 8]; 32];
        for (k, row) in b.iter_mut().enumerate() {
            for (c, x) in row.iter_mut().enumerate() {
                *x = (first as i32 + k as i32) * 10 + c as i32 + 1;
            }
        }
        b
    }

    fn push(o: &mut OutStream, i: u32) {
        o.push(&st(i), &block(i), i as u64 * 62_500, i as u64 * 62_500);
    }

    fn indices(v: &[(u64, [i32; 8])]) -> Vec<u64> {
        v.iter().map(|(i, _)| *i).collect()
    }

    #[test]
    fn rx_places_by_index() {
        let mut o = OutStream::new();
        push(&mut o, 32);
        push(&mut o, 0);
        let v = o.take_new();
        assert_eq!(indices(&v), (0..64).collect::<Vec<u64>>());
        assert_eq!(v[5].1, block(0)[5]);
        assert_eq!(v[40].1, block(32)[8]);
        assert_eq!(o.lost_frames, 0);
    }

    #[test]
    fn rx_counts_gap_and_fills_silence() {
        let mut o = OutStream::new();
        for i in [0u32, 32, 96, 128, 160, 192] {
            push(&mut o, i);
        }
        let v = o.take_new();
        assert_eq!(indices(&v), (0..224).collect::<Vec<u64>>());
        assert_eq!(o.lost_frames, 1);
        assert!(v[64..96].iter().all(|(_, f)| *f == [0; 8]));
        assert_eq!(v[96].1, block(96)[0]);
    }

    #[test]
    fn rx_ignores_duplicate() {
        let mut o = OutStream::new();
        for i in [0u32, 32, 32, 0] {
            push(&mut o, i);
        }
        assert_eq!(indices(&o.take_new()), (0..64).collect::<Vec<u64>>());
        assert_eq!(o.duplicates, 2);
    }

    #[test]
    fn rx_resync_on_index_jump_back() {
        let mut o = OutStream::new();
        push(&mut o, 999_936);
        push(&mut o, 999_968);
        assert_eq!(o.take_new().len(), 64);
        push(&mut o, 0); // board reset
        push(&mut o, 32);
        let v = o.take_new();
        assert_eq!(indices(&v), (0..64).collect::<Vec<u64>>());
        assert_eq!(o.resyncs, 1);
        assert_eq!(o.lost_frames, 0);
    }

    #[test]
    fn rx_board_clock_ppm() {
        let mut o = OutStream::new();
        assert_eq!(o.board_clock_ppm(), None);
        // board IEP runs 50 ppm fast against the PC clock, 2 s of frames
        for k in 0..1000u64 {
            let pc = k * 2_000_000;
            let iep = pc + pc / 20_000;
            o.push(&st((k * 32) as u32), &block(0), iep, pc);
        }
        let ppm = o.board_clock_ppm().unwrap();
        assert!((ppm - 50.0).abs() < 0.5, "{ppm}");
        assert_eq!(o.last_status().unwrap().first_index, 999 * 32);
    }

    #[test]
    fn capture_writes_eight_24bit_files() {
        let dir = tempfile::tempdir().unwrap();
        let mut c = WavCapture::create(dir.path(), "out").unwrap();
        let frames: Vec<(u64, [i32; 8])> =
            (0..100u64).map(|i| (i, [i as i32 * 1000 - 50_000, 1 << 23, -(1 << 23) - 5, 0, 1, 2, 3, 4])).collect();
        c.write(&frames).unwrap();
        c.finish().unwrap();
        for ch in 0..8 {
            let r = hound::WavReader::open(dir.path().join(format!("out_ch{ch}.wav"))).unwrap();
            let sp = r.spec();
            assert_eq!((sp.channels, sp.sample_rate, sp.bits_per_sample), (1, 16000, 24));
            let s: Vec<i32> = r.into_samples::<i32>().map(|x| x.unwrap()).collect();
            assert_eq!(s.len(), 100);
            let want: Vec<i32> = frames.iter().map(|(_, f)| f[ch].clamp(-(1 << 23), (1 << 23) - 1)).collect();
            assert_eq!(s, want, "ch {ch}");
        }
    }
}
