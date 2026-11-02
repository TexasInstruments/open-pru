// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Polling loop: one PicoScope 7 request at a time, start-to-start interval.

use crate::cli::Args;
use crate::mcp::McpClient;
use crate::stats::Stats;
use crate::tracker::{Conn, Snapshot, Tracker};
use std::sync::{Arc, Mutex};
use std::thread::{self, JoinHandle};
use std::time::{Duration, Instant};

/// HTTP timeout for one PicoScope 7 request (design.md, Behaviour 7).
pub const HTTP_TIMEOUT: Duration = Duration::from_secs(3);
/// Retry period while PicoScope 7 is not reachable.
const RETRY: Duration = Duration::from_secs(1);

/// Wait before the next poll. Waiting for PicoScope: 1 s. Otherwise fill
/// the interval; a reply slower than the interval is followed at once.
pub fn next_delay(conn: Conn, interval: Duration, elapsed: Duration) -> Duration {
    match conn {
        Conn::Waiting => RETRY,
        _ => interval.saturating_sub(elapsed),
    }
}

/// Poll forever. Stores each new snapshot in `shared` and calls `on_change`
/// when the LED value or the connection state changed.
pub fn run(args: &Args, shared: &Mutex<Snapshot>, on_change: &dyn Fn(&Snapshot)) -> ! {
    let mut client = McpClient::new(&args.url, HTTP_TIMEOUT);
    let mut tracker = Tracker::new();
    let interval = Duration::from_millis(args.interval_ms);
    let mut stats = Stats::new(Instant::now());
    let mut prev = tracker.snapshot();
    loop {
        let start = Instant::now();
        let r = tracker.poll_once(&mut client, start);
        let elapsed = start.elapsed();
        let snap = tracker.snapshot();
        if snap != prev {
            *shared.lock().unwrap() = snap.clone();
            on_change(&snap);
            prev = snap;
        }
        stats.record(elapsed, r);
        if args.stats
            && let Some(line) = stats.report_due(Instant::now())
        {
            println!("{line}");
        }
        thread::sleep(next_delay(prev.conn, interval, start.elapsed()));
    }
}

/// Run [`run`] on its own thread.
pub fn spawn(args: &Args, shared: Arc<Mutex<Snapshot>>, on_change: Box<dyn Fn() + Send>) -> JoinHandle<()> {
    let args = args.clone();
    thread::spawn(move || run(&args, &shared, &|_| on_change()))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn ms(v: u64) -> Duration {
        Duration::from_millis(v)
    }

    #[test]
    fn retry_delay_is_one_second_when_waiting() {
        assert_eq!(next_delay(Conn::Waiting, ms(50), ms(2)), ms(1000));
    }

    #[test]
    fn delay_fills_interval() {
        assert_eq!(next_delay(Conn::Live, ms(50), ms(12)), ms(38));
        assert_eq!(next_delay(Conn::NoFrames, ms(50), ms(12)), ms(38));
    }

    #[test]
    fn slow_reply_polls_immediately() {
        assert_eq!(next_delay(Conn::Live, ms(50), ms(800)), Duration::ZERO);
    }
}
