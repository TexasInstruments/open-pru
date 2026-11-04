// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! AUDIO_IN scheduling (design 6.4): keep the next 64-frame block a target
//! lead ahead of RTU1's read position in the clip buffer.

use crate::proto::Status;

const RATE: u64 = 16_000;

/// Decides when the next AUDIO_IN block is due and which board index it
/// gets. Board indices are RTU1's frame count (`Status::rtu1_total`).
#[derive(Clone, Debug)]
pub struct Pacer {
    lead: u32,
    clip: u32,
    obs: Option<(u64, u64)>,
    next: Option<u64>,
    in_underrun: bool,
    finished: bool,
    /// RTU1 reached audio the PC had not sent yet (then the lead restarts).
    pub underruns: u64,
    /// A block was held back because it would overwrite unread audio.
    pub overwrite_blocked: u64,
    /// RTU1's position jumped back (board reset or reload).
    pub restarts: u64,
}

impl Pacer {
    /// `lead_frames`: target distance ahead of RTU1 (8000 = 0.5 s);
    /// `clip_frames`: ring size until the first status says otherwise.
    pub fn new(lead_frames: u32, clip_frames: u32) -> Self {
        Pacer { lead: lead_frames, clip: clip_frames, obs: None, next: None, in_underrun: false, finished: false,
                underruns: 0, overwrite_blocked: 0, restarts: 0 }
    }

    /// Feed the status block of every AUDIO_OUT frame.
    pub fn on_status(&mut self, s: &Status, pc_ns: u64) {
        if s.clip_frames != 0 {
            self.clip = s.clip_frames;
        }
        let total = s.rtu1_total();
        if let Some((last, _)) = self.obs
            && total + 1000 < last {
                self.restarts += 1;
                self.next = None;
                self.in_underrun = false;
            }
        self.obs = Some((total, pc_ns));
        if self.finished {
            return;
        }
        match self.next {
            Some(n) if total >= n => {
                if !self.in_underrun {
                    self.underruns += 1;
                    self.in_underrun = true;
                }
                self.next = None; // start again one lead ahead
            }
            Some(_) => self.in_underrun = false,
            None => {}
        }
    }

    fn est(&self, pc_ns: u64) -> Option<u64> {
        let (total, t) = self.obs?;
        Some(total + pc_ns.saturating_sub(t) * RATE / 1_000_000_000)
    }

    /// The input has ended: no more blocks, and RTU1 passing the last one
    /// is not an underrun.
    pub fn finish(&mut self) {
        self.finished = true;
    }

    /// The first index of the next block to send now, if one is due.
    pub fn next_due(&mut self, pc_ns: u64) -> Option<u32> {
        if self.finished {
            return None;
        }
        let est = self.est(pc_ns)?;
        let ring_end = est + self.clip as u64; // never write at or past this
        let n = match self.next {
            Some(n) => {
                if n >= est + self.lead as u64 {
                    return None;
                }
                n
            }
            None => (est + self.lead.min(self.clip.saturating_sub(128)) as u64).div_ceil(64) * 64,
        };
        if n + 64 > ring_end {
            self.overwrite_blocked += 1;
            self.next = Some(n);
            return None;
        }
        self.next = Some(n + 64);
        Some(n as u32)
    }

    /// Frames between the next block and RTU1's estimated position.
    pub fn ahead_frames(&self, pc_ns: u64) -> Option<i64> {
        Some(self.next? as i64 - self.est(pc_ns)? as i64)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const N: u32 = 16384;
    const LEAD: u32 = 8000;
    const MS: u64 = 1_000_000;

    fn status_at(t_ns: u64, t0: u64) -> Status {
        let total = (t_ns - t0) * 16_000 / 1_000_000_000;
        Status {
            clip_frames: N,
            rtu1_loops: (total / N as u64) as u32,
            rtu1_frame_pos: (total % N as u64) as u32,
            ..Status::default()
        }
    }

    /// Board ticks every 2 ms (one AUDIO_OUT status); the PC sends what is
    /// due unless `paused(t)`. Returns (t, block index, rtu1_total) of every send.
    fn simulate(p: &mut Pacer, ms: u64, paused: impl Fn(u64) -> bool) -> Vec<(u64, u32, u64)> {
        let t0 = 1_000 * MS;
        let mut sent = vec![];
        for k in 0..ms / 2 {
            let t = t0 + k * 2 * MS;
            let st = status_at(t, t0);
            p.on_status(&st, t);
            if paused(t - t0) {
                continue;
            }
            while let Some(i) = p.next_due(t) {
                sent.push((t - t0, i, st.rtu1_total()));
            }
        }
        sent
    }

    #[test]
    fn pacing_starts_lead_ahead() {
        let mut p = Pacer::new(LEAD, N);
        assert_eq!(p.next_due(0), None, "no status yet");
        p.on_status(&Status { clip_frames: N, rtu1_frame_pos: 100, ..Status::default() }, 0);
        assert_eq!(p.next_due(0), Some(8128)); // round_up(100 + 8000, 64)
    }

    #[test]
    fn pacing_holds_lead_at_16khz() {
        let mut p = Pacer::new(LEAD, N);
        let sent = simulate(&mut p, 10_000, |_| false);
        let late: Vec<_> = sent.iter().filter(|(t, _, _)| *t > 100 * MS).collect();
        assert!(late.len() > 2000);
        for (_, i, total) in late {
            let lead = *i as i64 - *total as i64;
            assert!((LEAD as i64 - 128..=LEAD as i64 + 128).contains(&lead), "lead {lead}");
        }
        assert!(sent.windows(2).all(|w| w[1].1 == w[0].1 + 64), "contiguous blocks");
        assert_eq!((p.underruns, p.overwrite_blocked, p.restarts), (0, 0, 0));
    }

    #[test]
    fn pacing_counts_underrun_and_recovers() {
        let mut p = Pacer::new(LEAD, N);
        let sent = simulate(&mut p, 3_000, |t| (1_000 * MS..1_600 * MS).contains(&t));
        assert!(p.underruns >= 1);
        let after: Vec<_> = sent.iter().filter(|(t, _, _)| *t > 2_600 * MS).collect();
        assert!(!after.is_empty());
        for (_, i, total) in after {
            let lead = *i as i64 - *total as i64;
            assert!((LEAD as i64 - 128..=LEAD as i64 + 128).contains(&lead), "lead {lead}");
        }
    }

    #[test]
    fn pacing_never_writes_past_read_position() {
        // a lead larger than the ring must be refused, not wrap onto unread audio
        let mut p = Pacer::new(20_000, N);
        let sent = simulate(&mut p, 2_000, |_| false);
        for (_, i, total) in &sent {
            assert!((*i as u64 + 64) - total <= N as u64, "block {i} at total {total}");
        }
        assert!(p.overwrite_blocked > 0);
    }

    #[test]
    fn pacing_end_of_stream_is_not_underrun() {
        let mut p = Pacer::new(LEAD, N);
        let t0 = 1_000 * MS;
        for k in 0..2000u64 {
            let t = t0 + k * 2 * MS;
            p.on_status(&status_at(t, t0), t);
            if k == 100 {
                p.finish(); // the input ended: RTU1 may now pass the last block
            }
            if k < 100 {
                while p.next_due(t).is_some() {}
            }
        }
        assert_eq!(p.underruns, 0);
        assert_eq!(p.next_due(t0 + 4_000 * MS), None, "nothing is due after finish");
    }

    #[test]
    fn pacing_restarts_after_board_reset() {
        let mut p = Pacer::new(LEAD, N);
        p.on_status(&Status { clip_frames: N, rtu1_loops: 3, rtu1_frame_pos: 500, ..Status::default() }, 0);
        while p.next_due(0).is_some() {}
        p.on_status(&Status { clip_frames: N, rtu1_frame_pos: 0, ..Status::default() }, MS);
        assert_eq!(p.restarts, 1);
        assert_eq!(p.next_due(MS), Some(8000)); // round_up(0 + 8000, 64)
    }
}
