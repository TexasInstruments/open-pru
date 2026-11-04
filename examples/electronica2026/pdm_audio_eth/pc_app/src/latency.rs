// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Board audio latency (design 7.2, user option A): from the time RTU1
//! takes an input sample out of the clip buffer to the time the PCM of that
//! sample leaves the board. Measured on the board's IEP clock: the input
//! index of an output sample comes from cross-correlating input and output
//! of one channel; RTU1's time for an index from the newest status
//! (detect_iep, rtu1_total); the send time is the AUDIO_OUT header ts.

use std::collections::VecDeque;

use crate::proto::Status;

const KEEP: usize = 48_000; // 3 s of samples
const WINDOW: usize = 8_000; // correlation window, 0.5 s
const LAG_MIN: i64 = -256;
const LAG_MAX: i64 = 1024;
const MIN_CORR: f64 = 0.5;

/// Measures the board audio latency of one channel.
#[derive(Default)]
pub struct LatencyMeter {
    inp: VecDeque<f32>,
    in_base: u64,
    out: VecDeque<f32>,
    out_base: u64,
    /// (youngest PCM index, frame ts, detect_iep, rtu1_total) per AUDIO_OUT
    frames: VecDeque<(u64, u64, u64, u64)>,
    status: Option<(u64, u64)>,
    lag: Option<i64>,
    lag_end: u64,
    lag_done: bool,
}

fn push_run(q: &mut VecDeque<f32>, base: &mut u64, index: u64, x: impl Iterator<Item = f32>) {
    if q.is_empty() || index != *base + q.len() as u64 {
        q.clear();
        *base = index;
    }
    q.extend(x);
    while q.len() > KEEP {
        q.pop_front();
        *base += 1;
    }
}

impl LatencyMeter {
    pub fn new() -> Self {
        Self::default()
    }

    /// Input block of the channel: board index of the first sample.
    pub fn push_in(&mut self, index: u64, x: &[i16]) {
        push_run(&mut self.inp, &mut self.in_base, index, x.iter().map(|v| *v as f32));
    }

    /// Status of the AUDIO_OUT frame whose PCM is pushed next.
    pub fn push_status(&mut self, s: &Status) {
        self.status = Some((s.detect_iep, s.rtu1_total()));
    }

    /// Output PCM of the channel from one AUDIO_OUT frame (`tx_iep`: its ts).
    pub fn push_out(&mut self, index: u64, y: &[i32], tx_iep: u64) {
        push_run(&mut self.out, &mut self.out_base, index, y.iter().map(|v| *v as f32));
        if let Some((detect, total)) = self.status {
            self.frames.push_back((index + y.len() as u64 - 1, tx_iep, detect, total));
            while self.frames.len() > 50 {
                self.frames.pop_front();
            }
        }
    }

    /// Lag L with output index k carrying input index k + L, if the
    /// normalised correlation over the last 0.5 s is at least 0.5.
    fn find_lag(&self) -> Option<i64> {
        if self.out.len() < WINDOW {
            return None;
        }
        let k1 = self.out_base + self.out.len() as u64;
        let k0 = k1 - WINDOW as u64;
        let y: Vec<f32> = self.out.range(self.out.len() - WINDOW..).copied().collect();
        let yy: f64 = y.iter().map(|v| (*v as f64).powi(2)).sum();
        let (mut best, mut best_l) = (0.0f64, None);
        for l in LAG_MIN..=LAG_MAX {
            let i0 = k0 as i64 + l;
            if i0 < self.in_base as i64 || i0 + WINDOW as i64 > (self.in_base + self.inp.len() as u64) as i64 {
                continue;
            }
            let off = (i0 - self.in_base as i64) as usize;
            let (mut xy, mut xx) = (0.0f64, 0.0f64);
            for (j, yv) in y.iter().enumerate() {
                let xv = self.inp[off + j] as f64;
                xy += xv * *yv as f64;
                xx += xv * xv;
            }
            if xx > 0.0 && yy > 0.0 {
                let c = xy / (xx * yy).sqrt();
                if c > best {
                    best = c;
                    best_l = Some(l);
                }
            }
        }
        if best >= MIN_CORR { best_l } else { None }
    }

    /// The lag L found by the last correlation (output index k carries
    /// input index k + L).
    pub fn lag(&self) -> Option<i64> {
        self.lag
    }

    /// Board audio latency in ms (minimum over the recent frames), or
    /// `None` without correlated input and output.
    pub fn board_latency_ms(&mut self) -> Option<f64> {
        let end = self.out_base + self.out.len() as u64;
        // the correlation (about 10 M multiply-adds) runs at most once per
        // second of output, also when it found nothing
        let ready = self.lag.is_some() || self.out.len() >= WINDOW;
        if ready && (!self.lag_done || end >= self.lag_end + 16_000 || end < self.lag_end) {
            self.lag = self.find_lag();
            self.lag_end = end;
            self.lag_done = true;
        }
        let lag = self.lag?;
        self.frames
            .iter()
            .map(|(k, ts, detect, total)| {
                let i = *k as i64 + lag;
                let t_in = *detect as i64 + (i - *total as i64) * 1_000_000_000 / 16_000;
                (*ts as i64 - t_in) as f64 / 1e6
            })
            .reduce(f64::min)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::emulator::{EmuConfig, Emulator};
    use crate::link::{now_ns, Link};
    use crate::pacing::Pacer;
    use crate::proto::*;

    fn noise(seed: u32, n: usize) -> Vec<i16> {
        let mut s = seed;
        (0..n)
            .map(|_| {
                s = s.wrapping_mul(1_664_525).wrapping_add(1_013_904_223);
                ((s >> 16) as i16) / 2
            })
            .collect()
    }

    /// Stream `input` (channel 0) through the emulator for `secs`, feeding
    /// the meter like the application does.
    fn run(input: &[i16], replace_out: Option<Vec<i16>>, secs: f64) -> LatencyMeter {
        let mut e = Emulator::new(EmuConfig::default());
        let mut m = LatencyMeter::new();
        let mut p = Pacer::new(8000, 16384);
        let h = Header { version: VERSION, ftype: FrameType::Control, length: 0, seq: 0, ts: 0 };
        e.send(&encode(BOARD_MAC, e.mac(), &h, &Body::Control { cmd: CMD_RESET_COUNTERS, arg0: 0, arg1: 0 }, 60)).unwrap();
        let (end, mut pos) = (now_ns() + (secs * 1e9) as u64, 0usize);
        while now_ns() < end {
            if let Some(f) = e.recv(std::time::Duration::from_millis(1)).unwrap() {
                if let Ok((h, Body::AudioOut { status, pcm }, _)) = decode(&f.data) {
                    p.on_status(&status, f.t_ns);
                    m.push_status(&status);
                    let mut y: Vec<i32> = pcm.iter().map(|r| r[0]).collect();
                    if let Some(r) = &replace_out {
                        let k0 = status.first_index as usize;
                        y = (0..32).map(|k| (r[(k0 + k) % r.len()] as i32) << 8).collect();
                    }
                    m.push_out(status.first_index as u64, &y, h.ts);
                }
            }
            while let Some(first) = p.next_due(now_ns()) {
                let mut pcm = Box::new([[0i16; 8]; 64]);
                let mut x = [0i16; 64];
                for k in 0..64 {
                    x[k] = input[(pos + k) % input.len()];
                    pcm[k][0] = x[k];
                }
                let h = Header { version: VERSION, ftype: FrameType::AudioIn, length: 0, seq: 0, ts: 0 };
                e.send(&encode(BOARD_MAC, e.mac(), &h, &Body::AudioIn { first_index: first, pcm }, 0)).unwrap();
                m.push_in(first as u64, &x);
                pos += 64;
            }
        }
        m
    }

    #[test]
    fn latency_emulator_known_delay() {
        let mut m = run(&noise(1, 48_000), None, 2.0);
        let ms = m.board_latency_ms().expect("latency");
        assert!((ms - 2.5).abs() < 0.2, "{ms} ms");
    }

    #[test]
    fn latency_none_without_input() {
        let mut m = LatencyMeter::new();
        m.push_status(&Status { clip_frames: 16384, ..Status::default() });
        m.push_out(0, &[0; 32], 1000);
        assert_eq!(m.board_latency_ms(), None);
    }

    #[test]
    fn latency_none_when_uncorrelated() {
        let mut m = run(&noise(1, 48_000), Some(noise(99, 48_000)), 1.5);
        assert_eq!(m.board_latency_ms(), None);
    }
}
