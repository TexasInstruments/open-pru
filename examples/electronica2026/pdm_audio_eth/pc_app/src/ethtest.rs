// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Ethernet test (design 6.3): link, ping-pong round trip, throughput in
//! both directions, one pass/fail verdict per step.

use std::io;
use std::time::Duration;

use crate::link::{now_ns, Link};
use crate::proto::*;

/// Frame size of the throughput test (fixed on the board).
pub const TPUT_FRAME: usize = 1514;
/// Frame size of the small-frame throughput test: 64 B on the wire with CRC.
pub const SMALL_FRAME: usize = 60;
/// Frames per sendmmsg call in the PC -> board run.
pub const SEND_BATCH: usize = 64;

/// Frames per second at 1 Gbit/s for a frame of `frame` bytes without CRC
/// (+ 4 CRC, 8 preamble, 12 IPG).
pub fn line_rate_fps(frame: usize) -> f64 {
    1e9 / ((frame + 24) as f64 * 8.0)
}
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
    /// Frame size without CRC (TPUT_FRAME or SMALL_FRAME).
    pub frame: usize,
    pub frames: u64,
    pub bytes: u64,
    pub mbit_s: f64,
    pub sent: u64,
    pub seq_gaps: u64,
    /// PC-side loss: socket buffer (kernel) and NIC overruns during the run.
    pub pc_kernel_drops: u64,
    pub pc_nic_drops: u64,
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
    pub to_board_64: TputResult,
    pub from_board_64: TputResult,
}

fn tput_verdict(t: &TputResult) -> (bool, String) {
    let lost = t.sent.saturating_sub(t.frames);
    // the rate floor is for large frames; small frames report what they get
    let ok = (t.frame != TPUT_FRAME || t.mbit_s >= MIN_MBIT_S) && t.loss() < MAX_LOSS;
    let mut text = if t.frame == TPUT_FRAME {
        format!("{}/{} frames, {:.1} Mbit/s, {} sequence gaps", t.frames, t.sent, t.mbit_s, t.seq_gaps)
    } else {
        let fps = t.mbit_s * 1e6 / (t.frame as f64 * 8.0);
        format!("{}/{} frames, {:.3} Mframes/s ({:.0} % of line rate), {:.1} Mbit/s, {} sequence gaps",
                t.frames, t.sent, fps / 1e6, 100.0 * fps / line_rate_fps(t.frame), t.mbit_s, t.seq_gaps)
    };
    if lost > 0 {
        let pc = t.pc_kernel_drops + t.pc_nic_drops;
        if pc > 0 {
            text += &format!(", PC dropped {} in the kernel and {} in the NIC", t.pc_kernel_drops, t.pc_nic_drops);
            let rest = lost.saturating_sub(pc);
            if rest == 0 {
                text += ", not the board";
            } else {
                text += &format!(", {rest} unexplained (board or wire)");
            }
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
        let to64 = tput_verdict(&self.to_board_64);
        let from64 = tput_verdict(&self.from_board_64);
        vec![
            ("link", mbit == 1000 && full && carrier,
             format!("{mbit}/{}{}", if full { "full" } else { "half" }, if carrier { "" } else { ", no carrier" })),
            ("ping", p.sent > 0 && p.lost == 0,
             format!("{}/{}, rtt min/mean/p99/max {:.0}/{:.0}/{:.0}/{:.0} us, board turnaround {:.2}-{:.2} us",
                     p.sent - p.lost, p.sent, p.min_us, p.mean_us, p.p99_us, p.max_us,
                     p.board_turnaround_ns.0 as f64 / 1000.0, p.board_turnaround_ns.1 as f64 / 1000.0)),
            ("to board", to.0, to.1),
            ("from board", from.0, from.1),
            ("to board 64 B", to64.0, to64.1),
            ("from board 64 B", from64.0, from64.1),
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
        if let Ok((h, b, _)) = decode(&f.data)
            && accept(&h, &b) {
                return Ok(Some((h, b, f.t_ns)));
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
pub fn tput_to_board(link: &mut dyn Link, cfg: &TestConfig, frame: usize) -> io::Result<TputResult> {
    hello(link)?; // zero the board's TPUT counters
    let h = Header { version: VERSION, ftype: FrameType::TputData, length: 0, seq: 0, ts: 0 };
    let mac = link.mac();
    let f = encode(BOARD_MAC, mac, &h, &Body::TputData, frame);
    let mut batch = vec![f; SEND_BATCH];
    let end = now_ns() + (cfg.seconds * 1e9) as u64;
    let mut sent = 0u32;
    while now_ns() < end {
        // frames the link refused (NIC queue full) go again with their seq
        for (k, b) in batch.iter_mut().enumerate() {
            b[14 + 4..14 + 8].copy_from_slice(&(sent + k as u32).to_le_bytes());
        }
        sent += link.send_batch(&batch)? as u32;
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
    let mbit_s = if frames > 1 && dt > 0.0 { (frames - 1) as f64 * frame as f64 * 8.0 / dt * 1e3 } else { 0.0 };
    Ok(TputResult { dir: Dir::ToBoard, frame, frames, bytes: frames * frame as u64, mbit_s,
                    sent: sent as u64, seq_gaps: gaps, pc_kernel_drops: 0, pc_nic_drops: 0 })
}

/// Board -> PC: the board sends `seconds` worth of frames back to back; the
/// PC counts them. The rate is the board's: first to last header time stamp
/// (PRU0's IEP time when it built the frame). The PC receive times are not
/// used: frames that wait in the socket queue before the reader gets to them
/// carry late receive times, which made the rate look higher than line rate.
pub fn tput_from_board(link: &mut dyn Link, cfg: &TestConfig, frame: usize) -> io::Result<TputResult> {
    let count = (cfg.seconds * line_rate_fps(frame)).round() as u32;
    let st0 = link.stats();
    send_frame(link, FrameType::Control, 0, now_ns(),
               &Body::Control { cmd: CMD_TPUT_TX, arg0: count, arg1: frame as u32 }, 60)?;
    let (mut frames, mut gaps, mut last_seq) = (0u64, 0u64, None::<u32>);
    let (mut t_first, mut t_last) = (0u64, 0u64);
    while frames < count as u64 {
        let Some(f) = link.recv(Duration::from_millis(500))? else { break };
        let Ok((h, _, _)) = decode(&f.data) else { continue };
        if h.ftype != FrameType::TputData {
            continue;
        }
        if frames == 0 {
            t_first = h.ts;
        } else if Some(h.seq) != last_seq.map(|s| s.wrapping_add(1)) {
            gaps += 1;
        }
        t_last = h.ts;
        last_seq = Some(h.seq);
        frames += 1;
        if h.seq + 1 == count {
            break;
        }
    }
    let dt = t_last.saturating_sub(t_first) as f64;
    let mbit_s = if frames > 1 && dt > 0.0 { (frames - 1) as f64 * frame as f64 * 8.0 / dt * 1e3 } else { 0.0 };
    Ok(TputResult { dir: Dir::FromBoard, frame, frames, bytes: frames * frame as u64, mbit_s,
                    sent: count as u64, seq_gaps: gaps,
                    pc_kernel_drops: link.stats().rx_kernel_drops - st0.rx_kernel_drops,
                    pc_nic_drops: link.stats().rx_nic_drops - st0.rx_nic_drops })
}

/// The whole test: ping, then throughput both ways.
pub fn run(link: &mut dyn Link, cfg: &TestConfig, link_info: (u32, bool, bool)) -> io::Result<Summary> {
    hello(link)?;
    let ping = ping(link, cfg)?;
    let to_board = tput_to_board(link, cfg, TPUT_FRAME)?;
    let from_board = tput_from_board(link, cfg, TPUT_FRAME)?;
    let to_board_64 = tput_to_board(link, cfg, SMALL_FRAME)?;
    let from_board_64 = tput_from_board(link, cfg, SMALL_FRAME)?;
    Ok(Summary { link: link_info, ping, to_board, from_board, to_board_64, from_board_64 })
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
        TputResult { dir, frame: TPUT_FRAME, frames, bytes: frames * 1514, mbit_s, sent: frames + lost, seq_gaps: lost.min(1),
                     pc_kernel_drops: pc_drops, pc_nic_drops: 0 }
    }

    #[test]
    fn tput_verdict_splits_kernel_nic_and_unexplained_loss() {
        let mut from = tput(Dir::FromBoard, 900.0, 9_980, 20, 10);
        from.pc_nic_drops = 5;
        let s = summary(tput(Dir::ToBoard, 900.0, 10_000, 0, 0), from);
        let (ok, text) = pass(&s, "from board");
        assert!(!ok);
        assert!(text.contains("PC dropped 10 in the kernel and 5 in the NIC"), "{text}");
        assert!(text.contains("5 unexplained"), "{text}");
        let mut all_pc = tput(Dir::FromBoard, 900.0, 9_985, 15, 10);
        all_pc.pc_nic_drops = 5;
        let s = summary(tput(Dir::ToBoard, 900.0, 10_000, 0, 0), all_pc);
        let (_, text) = pass(&s, "from board");
        assert!(text.contains("not the board"), "{text}");
        assert!(!text.contains("unexplained"), "{text}");
    }

    fn summary(to: TputResult, from: TputResult) -> Summary {
        Summary {
            link: (1000, true, true),
            ping: PingResult { sent: 10, lost: 0, min_us: 1.0, mean_us: 2.0, p99_us: 3.0, max_us: 4.0, board_turnaround_ns: (1, 2) },
            to_board: to.clone(),
            from_board: from.clone(),
            to_board_64: TputResult { frame: SMALL_FRAME, ..to },
            from_board_64: TputResult { frame: SMALL_FRAME, ..from },
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
        let to = tput_to_board(&mut e, &cfg, TPUT_FRAME).unwrap();
        assert_eq!(to.frames, to.sent, "board saw every PC frame");
        assert_eq!(to.seq_gaps, 0);
        let from = tput_from_board(&mut e, &cfg, TPUT_FRAME).unwrap();
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
        let v = s.verdicts();
        for (name, ok, text) in &v {
            assert!(ok, "{name}: {text}");
        }
        let names: Vec<&str> = v.iter().map(|x| x.0).collect();
        assert_eq!(names, ["link", "ping", "to board", "from board", "to board 64 B", "from board 64 B"]);
        assert_eq!(s.from_board_64.frame, SMALL_FRAME);
        assert!(s.from_board_64.frames > 1000, "{:?}", s.from_board_64);
    }

    /// Accepts at most `take` frames per batch; records batch sizes and seqs.
    struct BatchLink {
        take: usize,
        batches: Vec<usize>,
        seqs: Vec<u32>,
    }

    impl Link for BatchLink {
        fn send(&mut self, _frame: &[u8]) -> io::Result<()> {
            Ok(())
        }
        fn send_batch(&mut self, frames: &[Vec<u8>]) -> io::Result<usize> {
            self.batches.push(frames.len());
            let n = frames.len().min(self.take);
            for f in &frames[..n] {
                if let Ok((h, Body::TputData, _)) = decode(f) {
                    self.seqs.push(h.seq);
                }
            }
            Ok(n)
        }
        fn recv(&mut self, _timeout: Duration) -> io::Result<Option<crate::link::RxFrame>> {
            Ok(None)
        }
        fn mac(&self) -> [u8; 6] {
            [2, 0, 0, 0, 0, 9]
        }
        fn stats(&mut self) -> crate::link::LinkStats {
            crate::link::LinkStats::default()
        }
    }

    #[test]
    fn to_board_sends_in_batches_and_counts_only_accepted() {
        let mut l = BatchLink { take: 40, batches: vec![], seqs: vec![] };
        let cfg = TestConfig { pings: 0, ping_timeout: Duration::from_millis(1), seconds: 0.02 };
        let r = tput_to_board(&mut l, &cfg, SMALL_FRAME).unwrap();
        assert!(l.batches.iter().all(|b| *b <= SEND_BATCH), "{:?}", &l.batches[..4]);
        assert!(l.batches.contains(&SEND_BATCH), "full batches used");
        assert_eq!(r.sent, l.seqs.len() as u64);
        let want: Vec<u32> = (0..l.seqs.len() as u32).collect();
        assert_eq!(l.seqs, want, "the frames the link refused are sent again, in order");
    }

    /// Answers TPUT_TX with `count` TPUT_DATA frames whose board time stamps
    /// are at line rate, but which all reach the PC at the same moment (as when
    /// they waited in the socket queue before the reader got to them).
    struct QueuedLink {
        rx: std::collections::VecDeque<crate::link::RxFrame>,
    }

    impl Link for QueuedLink {
        fn send(&mut self, frame: &[u8]) -> io::Result<()> {
            if let Ok((_, Body::Control { cmd: CMD_TPUT_TX, arg0, arg1 }, _)) = decode(frame) {
                let frame = arg1 as usize;
                let wire_ns = (frame as u64 + 24) * 8;
                for seq in 0..arg0 {
                    let h = Header { version: crate::proto::layout::ETH_VERSION as u8, ftype: FrameType::TputData,
                                     length: 0, seq, ts: 1_000_000 + seq as u64 * wire_ns };
                    let data = encode([2, 0, 0, 0, 0, 9], [2, 0, 0x5d, 0x4d, 0, 1], &h, &Body::TputData, frame);
                    self.rx.push_back(crate::link::RxFrame { data, t_ns: 5_000_000_000 });
                }
            }
            Ok(())
        }
        fn recv(&mut self, _timeout: Duration) -> io::Result<Option<crate::link::RxFrame>> {
            Ok(self.rx.pop_front())
        }
        fn mac(&self) -> [u8; 6] {
            [2, 0, 0, 0, 0, 9]
        }
        fn stats(&mut self) -> crate::link::LinkStats {
            crate::link::LinkStats::default()
        }
    }

    #[test]
    fn from_board_rate_uses_board_time_stamps() {
        let mut l = QueuedLink { rx: Default::default() };
        let cfg = TestConfig { pings: 0, ping_timeout: Duration::from_millis(1), seconds: 0.01 };
        let r = tput_from_board(&mut l, &cfg, SMALL_FRAME).unwrap();
        let line = line_rate_fps(SMALL_FRAME) * SMALL_FRAME as f64 * 8.0 / 1e6;
        assert!(r.frames > 1000, "{r:?}");
        assert!((r.mbit_s - line).abs() < line * 0.001, "{} Mbit/s, line rate {line}", r.mbit_s);
    }

    #[test]
    fn line_rate_for_64_and_1514_byte_frames() {
        assert!((line_rate_fps(TPUT_FRAME) - 81_274.0).abs() < 1.0);
        assert!((line_rate_fps(SMALL_FRAME) - 1_488_095.2).abs() < 1.0);
    }

    #[test]
    fn small_frame_verdict_has_no_rate_floor() {
        let small = |dir, frames, lost| {
            let mut t = tput(dir, 100.0, frames, lost, 0);
            t.frame = SMALL_FRAME;
            t
        };
        let mut s = summary(tput(Dir::ToBoard, 900.0, 10_000, 0, 0), tput(Dir::FromBoard, 900.0, 10_000, 0, 0));
        s.to_board_64 = small(Dir::ToBoard, 10_000, 0);
        s.from_board_64 = small(Dir::FromBoard, 9_980, 20);
        let (ok, text) = pass(&s, "to board 64 B");
        assert!(ok, "{text}");
        assert!(text.contains("Mframes/s") && text.contains("% of line rate"), "{text}");
        assert!(!pass(&s, "from board 64 B").0);
    }
}
