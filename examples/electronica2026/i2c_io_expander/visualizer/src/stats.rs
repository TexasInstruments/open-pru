// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! `--stats`: reply times, value changes and skipped steps per second.

use crate::tracker::PollResult;
use std::time::{Duration, Instant};

const WINDOW: Duration = Duration::from_secs(1);

pub struct Stats {
    window_start: Instant,
    reply_ms: Vec<f64>,
    changes: u32,
    skips: u32,
}

impl Stats {
    pub fn new(now: Instant) -> Self {
        Stats { window_start: now, reply_ms: Vec::new(), changes: 0, skips: 0 }
    }

    pub fn record(&mut self, reply_time: Duration, r: PollResult) {
        self.reply_ms.push(reply_time.as_secs_f64() * 1000.0);
        self.changes += u32::from(r.changed);
        self.skips += u32::from(r.skipped);
    }

    /// Report line once per second, then start a new window.
    pub fn report_due(&mut self, now: Instant) -> Option<String> {
        if now.duration_since(self.window_start) < WINDOW {
            return None;
        }
        let mut ms = std::mem::take(&mut self.reply_ms);
        ms.sort_by(f64::total_cmp);
        let at = |q: f64| ms.get(((ms.len() as f64 - 1.0) * q).round() as usize).copied().unwrap_or(0.0);
        let line = format!(
            "replies {} | reply ms med {:.1} p95 {:.1} max {:.0} | changes {} | skips {}",
            ms.len(),
            at(0.5),
            at(0.95),
            ms.last().copied().unwrap_or(0.0),
            self.changes,
            self.skips
        );
        *self = Stats::new(now);
        Some(line)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ms(v: u64) -> Duration {
        Duration::from_millis(v)
    }

    #[test]
    fn stats_report_format() {
        let t0 = Instant::now();
        let mut s = Stats::new(t0);
        s.record(ms(2), PollResult { changed: true, skipped: false });
        s.record(ms(3), PollResult::default());
        s.record(ms(400), PollResult { changed: true, skipped: true });
        assert_eq!(s.report_due(t0 + ms(500)), None);
        let line = s.report_due(t0 + ms(1000)).unwrap();
        assert!(line.contains("replies 3"), "{line}");
        assert!(line.contains("max 400"), "{line}");
        assert!(line.contains("changes 2"), "{line}");
        assert!(line.contains("skips 1"), "{line}");
    }

    #[test]
    fn report_starts_new_window() {
        let t0 = Instant::now();
        let mut s = Stats::new(t0);
        s.record(ms(2), PollResult::default());
        s.report_due(t0 + ms(1000)).unwrap();
        let line = s.report_due(t0 + ms(2000)).unwrap();
        assert!(line.contains("replies 0"), "{line}");
    }
}
