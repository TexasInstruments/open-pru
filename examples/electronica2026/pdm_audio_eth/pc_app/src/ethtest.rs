// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Ethernet test (design 6.3): link, ping-pong round trip, throughput in
//! both directions, one pass/fail verdict per step.

use std::io;
use std::time::Duration;

use crate::link::{now_ns, Link};
use crate::proto::*;

/// Wire time of one 1514-byte frame at 1 Gbit/s (with CRC, preamble, IPG).
pub const TPUT_FRAMES_PER_S: f64 = 81_274.0;
/// Frame size of the throughput test (fixed on the board).
pub const TPUT_FRAME: usize = 1514;
/// Pass thresholds (design 6.3, Global Constraints).
pub const MIN_MBIT_S: f64 = 500.0;
pub const MAX_LOSS: f64 = 0.001;

/// Test parameters.
#[derive(Clone, Debug)]
pub struct TestConfig {
    pub pings: u32,
    pub ping_timeout: Duration,
    pub seconds: f64,
}

impl Default for TestConfig {
    fn default() -> Self {
        TestConfig { pings: 10_000, ping_timeout: Duration::from_millis(100), seconds: 3.0 }
    }
}

/// Ping-pong result. RTTs in microseconds on the PC clock; the board's
/// turnaround (TX IEP - RX IEP) in ns, as (min, max).
#[derive(Clone, Debug, PartialEq)]
pub struct PingResult {
    pub sent: u32,
    pub lost: u32,
    pub min_us: f64,
    pub mean_us: f64,
    pub p99_us: f64,
    pub max_us: f64,
    pub board_turnaround_ns: (u64, u64),
}

/// Direction of a throughput run.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Dir {
    ToBoard,
    FromBoard,
}

/// Throughput result; `frames` is what the receiver counted.
#[derive(Clone, Debug, PartialEq)]
pub struct TputResult {
    pub dir: Dir,
    pub frames: u64,
    pub bytes: u64,
    pub mbit_s: f64,
    pub sent: u64,
    pub seq_gaps: u64,
    pub pc_kernel_drops: u64,
}

impl TputResult {
    fn loss(&self) -> f64 {
        if self.sent == 0 { 1.0 } else { self.sent.saturating_sub(self.frames) as f64 / self.sent as f64 }
    }
}

/// Everything the test measured.
#[derive(Clone, Debug, PartialEq)]
pub struct Summary {
    pub link: (u32, bool, bool),
    pub ping: PingResult,
    pub to_board: TputResult,
    pub from_board: TputResult,
}

fn tput_verdict(t: &TputResult) -> (bool, String) {
    let lost = t.sent.saturating_sub(t.frames);
    let ok = t.mbit_s >= MIN_MBIT_S && t.loss() < MAX_LOSS;
    let mut text = format!(
        "{}/{} frames, {:.1} Mbit/s, {} sequence gaps",
        t.frames, t.sent, t.mbit_s, t.seq_gaps
    );
    if lost > 0 {
        if t.pc_kernel_drops > 0 {
            text += &format!(", PC dropped {} frames in the kernel (socket buffer), not the board", t.pc_kernel_drops);
        } else {
            match t.dir {
                Dir::ToBoard => text += &format!(", board lost {lost} frames"),
                Dir::FromBoard => text += &format!(", {lost} frames missing (board lost them or they never left)"),
            }
        }
    }
    (ok, text)
}

impl Summary {
    /// `(step, pass, text)` per step, in test order.
    pub fn verdicts(&self) -> Vec<(&'static str, bool, String)> {
        let (mbit, full, carrier) = self.link;
        let p = &self.ping;
        let to = tput_verdict(&self.to_board);
        let from = tput_verdict(&self.from_board);
        vec![
            ("link", mbit == 1000 && full && carrier,
             format!("{mbit}/{}{}", if full { "full" } else { "half" }, if carrier { "" } else { ", no carrier" })),
            ("ping", p.sent > 0 && p.lost == 0,
             format!("{}/{}, rtt min/mean/p99/max {:.0}/{:.0}/{:.0}/{:.0} us, board turnaround {:.2}-{:.2} us",
                     p.sent - p.lost, p.sent, p.min_us, p.mean_us, p.p99_us, p.max_us,
                     p.board_turnaround_ns.0 as f64 / 1000.0, p.board_turnaround_ns.1 as f64 / 1000.0)),
            ("to board", to.0, to.1),
            ("from board", from.0, from.1),
        ]
    }
}

fn send_frame(link: &mut dyn Link, ftype: FrameType, seq: u32, ts: u64, b: &Body, pad: usize) -> io::Result<()> {
    let h = Header { version: VERSION, ftype, length: 0, seq, ts };
    let mac = link.mac();
    link.send(&encode(BOARD_MAC, mac, &h, b, pad))
}

/// Make the board learn the PC MAC and zero its counters.
pub fn hello(link: &mut dyn Link) -> io::Result<()> {
    send_frame(link, FrameType::Control, 0, now_ns(),
               &Body::Control { cmd: CMD_RESET_COUNTERS, arg0: 0, arg1: 0 }, 60)
}

/// Wait for a frame of `want` that `accept` takes, until `timeout`.
fn wait_for<F>(link: &mut dyn Link, timeout: Duration, mut accept: F) -> io::Result<Option<(Header, Body, u64)>>
where
    F: FnMut(&Header, &Body) -> bool,
{
    let deadline = now_ns() + timeout.as_nanos() as u64;
    loop {
        let left = deadline.saturating_sub(now_ns());
        if left == 0 {
            return Ok(None);
        }
        let Some(f) = link.recv(Duration::from_nanos(left))? else { return Ok(None) };
        if let Ok((h, b, _)) = decode(&f.data) {
            if accept(&h, &b) {
                return Ok(Some((h, b, f.t_ns)));
            }
        }
    }
}

/// Ping-pong, one outstanding ping at a time.
pub fn ping(link: &mut dyn Link, cfg: &TestConfig) -> io::Result<PingResult> {
    let mut rtts = Vec::with_capacity(cfg.pings as usize);
    let mut turn = (u64::MAX, 0u64);
    for i in 0..cfg.pings {
        let t = now_ns();
        send_frame(link, FrameType::Ping, i, t, &Body::Ping { pc_ns: t }, 60)?;
        let got = wait_for(link, cfg.ping_timeout, |h, b| {
            h.ftype == FrameType::Pong && matches!(b, Body::Pong { ping_seq, .. } if *ping_seq == i)
        })?;
        if let Some((h, Body::Pong { pc_ns, rx_iep, .. }, t_rx)) = got {
            rtts.push(t_rx.saturating_sub(pc_ns) as f64 / 1000.0);
            let d = h.ts.saturating_sub(rx_iep);
            turn = (turn.0.min(d), turn.1.max(d));
        }
    }
    let lost = cfg.pings - rtts.len() as u32;
    rtts.sort_by(|a, b| a.partial_cmp(b).unwrap());
    let n = rtts.len();
    let pick = |q: f64| if n == 0 { 0.0 } else { rtts[((q * n as f64).ceil() as usize).clamp(1, n) - 1] };
    Ok(PingResult {
        sent: cfg.pings,
        lost,
        min_us: pick(0.0),
        mean_us: if n == 0 { 0.0 } else { rtts.iter().sum::<f64>() / n as f64 },
        p99_us: pick(0.99),
        max_us: pick(1.0),
        board_turnaround_ns: if n == 0 { (0, 0) } else { turn },
    })
}

/// PC -> board: TPUT_DATA as fast as the PC sends for `seconds`, then the
/// board's TPUT_REPORT. The rate is the board's (first to last IEP time).
pub fn tput_to_board(link: &mut dyn Link, cfg: &TestConfig) -> io::Result<TputResult> {
    hello(link)?; // zero the board's TPUT counters
    let h = Header { version: VERSION, ftype: FrameType::TputData, length: 0, seq: 0, ts: 0 };
    let mac = link.mac();
    let mut f = encode(BOARD_MAC, mac, &h, &Body::TputData, TPUT_FRAME);
    let end = now_ns() + (cfg.seconds * 1e9) as u64;
    let mut sent = 0u32;
    while now_ns() < end {
        f[14 + 4..14 + 8].copy_from_slice(&sent.to_le_bytes());
        link.send(&f)?;
        sent += 1;
    }
    std::thread::sleep(Duration::from_millis(20));
    send_frame(link, FrameType::Control, 0, now_ns(),
               &Body::Control { cmd: CMD_TPUT_REPORT_REQ, arg0: 0, arg1: 0 }, 60)?;
    let rep = wait_for(link, Duration::from_millis(500), |h, _| h.ftype == FrameType::TputReport)?;
    let (frames, gaps, first, last) = match rep {
        Some((_, Body::TputReport { frames, seq_gaps, first_iep, last_iep, .. }, _)) => {
            (frames as u64, seq_gaps as u64, first_iep, last_iep)
        }
        _ => (0, 0, 0, 0),
    };
    let dt = last.saturating_sub(first) as f64;
    let mbit_s = if frames > 1 && dt > 0.0 { (frames - 1) as f64 * TPUT_FRAME as f64 * 8.0 / dt * 1e3 } else { 0.0 };
    Ok(TputResult { dir: Dir::ToBoard, frames, bytes: frames * TPUT_FRAME as u64, mbit_s,
                    sent: sent as u64, seq_gaps: gaps, pc_kernel_drops: 0 })
}

/// Board -> PC: the board sends `seconds` worth of 1514-byte frames back to
/// back; the PC counts them. The rate is the PC's (first to last receive).
pub fn tput_from_board(link: &mut dyn Link, cfg: &TestConfig) -> io::Result<TputResult> {
    let count = (cfg.seconds * TPUT_FRAMES_PER_S).round() as u32;
    let drops0 = link.stats().rx_kernel_drops;
    send_frame(link, FrameType::Control, 0, now_ns(),
               &Body::Control { cmd: CMD_TPUT_TX, arg0: count, arg1: 1500 }, 60)?;
    let (mut frames, mut gaps, mut last_seq) = (0u64, 0u64, None::<u32>);
    let (mut t_first, mut t_last) = (0u64, 0u64);
    while frames < count as u64 {
        let Some(f) = link.recv(Duration::from_millis(500))? else { break };
        let Ok((h, _, _)) = decode(&f.data) else { continue };
        if h.ftype != FrameType::TputData {
            continue;
        }
        if frames == 0 {
            t_first = f.t_ns;
        } else if Some(h.seq) != last_seq.map(|s| s.wrapping_add(1)) {
            gaps += 1;
        }
        t_last = f.t_ns;
        last_seq = Some(h.seq);
        frames += 1;
        if h.seq + 1 == count {
            break;
        }
    }
    let dt = t_last.saturating_sub(t_first) as f64;
    let mbit_s = if frames > 1 && dt > 0.0 { (frames - 1) as f64 * TPUT_FRAME as f64 * 8.0 / dt * 1e3 } else { 0.0 };
    Ok(TputResult { dir: Dir::FromBoard, frames, bytes: frames * TPUT_FRAME as u64, mbit_s,
                    sent: count as u64, seq_gaps: gaps,
                    pc_kernel_drops: link.stats().rx_kernel_drops - drops0 })
}

/// The whole test: ping, then throughput both ways.
pub fn run(link: &mut dyn Link, cfg: &TestConfig, link_info: (u32, bool, bool)) -> io::Result<Summary> {
    hello(link)?;
    let ping = ping(link, cfg)?;
    let to_board = tput_to_board(link, cfg)?;
    let from_board = tput_from_board(link, cfg)?;
    Ok(Summary { link: link_info, ping, to_board, from_board })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::emulator::{EmuConfig, Emulator};

    fn quick() -> TestConfig {
        TestConfig { pings: 1000, ping_timeout: Duration::from_millis(5), seconds: 0.2 }
    }

    #[test]
    fn ping_stats_against_emulator() {
        let mut e = Emulator::new(EmuConfig::default());
        let p = ping(&mut e, &quick()).unwrap();
        assert_eq!((p.sent, p.lost), (1000, 0));
        assert!(p.min_us <= p.mean_us && p.mean_us <= p.p99_us && p.p99_us <= p.max_us, "{p:?}");
        assert_eq!(p.board_turnaround_ns, (1500, 1500));
    }

    #[test]
    fn ping_counts_lost() {
        let mut e = Emulator::new(EmuConfig { drop_every_nth_pong: Some(10), ..EmuConfig::default() });
        let p = ping(&mut e, &quick()).unwrap();
        assert_eq!((p.sent, p.lost), (1000, 100));
    }

    fn tput(dir: Dir, mbit_s: f64, frames: u64, lost: u64, pc_drops: u64) -> TputResult {
        TputResult { dir, frames, bytes: frames * 1514, mbit_s, sent: frames + lost, seq_gaps: lost.min(1), pc_kernel_drops: pc_drops }
    }

    fn summary(to: TputResult, from: TputResult) -> Summary {
        Summary {
            link: (1000, true, true),
            ping: PingResult { sent: 10, lost: 0, min_us: 1.0, mean_us: 2.0, p99_us: 3.0, max_us: 4.0, board_turnaround_ns: (1, 2) },
            to_board: to,
            from_board: from,
        }
    }

    fn pass(s: &Summary, name: &str) -> (bool, String) {
        let v = s.verdicts();
        let (_, ok, text) = v.iter().find(|(n, _, _)| *n == name).unwrap();
        (*ok, text.clone())
    }

    #[test]
    fn verdicts_apply_thresholds() {
        let s = summary(tput(Dir::ToBoard, 499.0, 10_000, 0, 0), tput(Dir::FromBoard, 500.0, 10_000, 0, 0));
        assert!(!pass(&s, "to board").0);
        assert!(pass(&s, "from board").0);
        assert!(pass(&s, "link").0);
        assert!(pass(&s, "ping").0);
        // 0.1 % loss fails, just under passes
        let s = summary(tput(Dir::ToBoard, 900.0, 9_990, 10, 0), tput(Dir::FromBoard, 900.0, 9_991, 9, 0));
        assert!(!pass(&s, "to board").0);
        assert!(pass(&s, "from board").0);
        let mut s = summary(tput(Dir::ToBoard, 900.0, 1, 0, 0), tput(Dir::FromBoard, 900.0, 1, 0, 0));
        s.link = (100, true, true);
        assert!(!pass(&s, "link").0);
        s.ping.lost = 1;
        assert!(!pass(&s, "ping").0);
    }

    #[test]
    fn tput_report_splits_pc_and_board_loss() {
        let mut e = Emulator::new(EmuConfig { pc_drop_every_nth_tput: Some(100), audio_out: false, ..EmuConfig::default() });
        let cfg = quick();
        hello(&mut e).unwrap();
        let to = tput_to_board(&mut e, &cfg).unwrap();
        assert_eq!(to.frames, to.sent, "board saw every PC frame");
        assert_eq!(to.seq_gaps, 0);
        let from = tput_from_board(&mut e, &cfg).unwrap();
        assert!(from.seq_gaps > 0 && from.pc_kernel_drops > 0, "{from:?}");
        let s = summary(to, from);
        let (ok, text) = pass(&s, "from board");
        assert!(!ok);
        assert!(text.contains("PC dropped"), "{text}");
        assert!(!text.contains("board lost"), "{text}");
    }

    #[test]
    fn run_against_emulator_passes() {
        let mut e = Emulator::new(EmuConfig::default());
        let s = run(&mut e, &quick(), (1000, true, true)).unwrap();
        for (name, ok, text) in s.verdicts() {
            assert!(ok, "{name}: {text}");
        }
    }
}
