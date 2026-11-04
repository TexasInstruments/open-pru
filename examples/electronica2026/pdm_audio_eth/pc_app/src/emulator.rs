// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! In-process board emulator (design 7.3): the same frames as the
//! LP-AM243 firmware, on wall-clock time, so the PC application can be
//! developed and tested without the board.

use std::collections::VecDeque;
use std::io;
use std::time::Duration;

use crate::link::{now_ns, Link, LinkStats, RxFrame};
use crate::proto::*;

/// Emulator settings.
#[derive(Clone, Debug)]
pub struct EmuConfig {
    /// Stream AUDIO_OUT (500 frames/s) once the PC was seen.
    pub audio_out: bool,
    /// Clip buffer (= PC input ring) size in frames.
    pub clip_frames: u32,
    /// PCM delay behind the RTU1 read position, frames.
    pub pipeline_frames: u32,
    /// Drop every n-th AUDIO_OUT frame (loss tests).
    pub drop_every_nth_out: Option<u32>,
    /// Drop every n-th PONG (loss tests).
    pub drop_every_nth_pong: Option<u32>,
    /// Count every n-th TPUT_DATA frame to the PC as a PC kernel drop.
    pub pc_drop_every_nth_tput: Option<u32>,
    /// Board time from PING receive to PONG send.
    pub turnaround_ns: u64,
}

impl Default for EmuConfig {
    fn default() -> Self {
        EmuConfig {
            audio_out: true,
            clip_frames: 16000,
            pipeline_frames: 40,
            drop_every_nth_out: None,
            drop_every_nth_pong: None,
            pc_drop_every_nth_tput: None,
            turnaround_ns: 1500,
        }
    }
}

/// The PC-side MAC the emulator pretends the PC has.
pub const EMU_PC_MAC: [u8; 6] = [0x02, 0x11, 0x22, 0x33, 0x44, 0x55];
const RATE: u64 = 16_000;
const TPUT_WIRE_NS: u64 = 12_304; // 1514 B + CRC + preamble + IPG at 1 Gbit/s

#[derive(Default)]
struct Counters {
    rx_frames: u32,
    rx_audio: u32,
    rx_foreign: u32,
    rx_bad: u32,
    last_in_end: u32,
    tx_late: u32,
    tput_frames: u32,
    tput_gaps: u32,
    tput_bytes: u64,
    tput_first: u64,
    tput_last: u64,
    tput_last_seq: u32,
}

/// The board emulator; implements [`Link`] as seen from the PC.
pub struct Emulator {
    cfg: EmuConfig,
    t0: u64,
    ring: Vec<[i16; 8]>,
    pc_mac: Option<[u8; 6]>,
    queue: VecDeque<Vec<u8>>,
    next_half: u64,
    out_seq: u32,
    pongs: u32,
    tput_left: u32,
    tput_seq: u32,
    tput_start: u64,
    c: Counters,
    stats: LinkStats,
}

impl Emulator {
    /// A fresh board: ring pre-filled with one tone per channel
    /// ((c + 1) x 500 Hz, 0.5 FS), like the booth clips.
    pub fn new(cfg: EmuConfig) -> Self {
        let n = cfg.clip_frames as usize;
        let ring = (0..n)
            .map(|f| {
                let mut fr = [0i16; 8];
                for (c, x) in fr.iter_mut().enumerate() {
                    let ph = 2.0 * std::f64::consts::PI * 500.0 * (c + 1) as f64 * f as f64 / RATE as f64;
                    *x = (16384.0 * ph.sin()).round() as i16;
                }
                fr
            })
            .collect();
        Emulator {
            cfg,
            t0: now_ns(),
            ring,
            pc_mac: None,
            queue: VecDeque::new(),
            next_half: 0,
            out_seq: 0,
            pongs: 0,
            tput_left: 0,
            tput_seq: 0,
            tput_start: 0,
            c: Counters::default(),
            stats: LinkStats::default(),
        }
    }

    /// Board reset (SW1): indices, HALF_SEQ and counters back to 0, PC MAC
    /// forgotten, queued frames dropped. The ring keeps its contents.
    pub fn reset_board(&mut self) {
        self.t0 = now_ns();
        self.pc_mac = None;
        self.queue.clear();
        self.next_half = 0;
        self.out_seq = 0;
        self.pongs = 0;
        self.tput_left = 0;
        self.c = Counters::default();
    }

    fn board_ns(&self, now: u64) -> u64 {
        now.saturating_sub(self.t0)
    }

    fn rtu1_total(&self, board_ns: u64) -> u64 {
        board_ns * RATE / 1_000_000_000
    }

    fn half_due_ns(&self, h: u64) -> u64 {
        (64 * (h + 1) + self.cfg.pipeline_frames as u64) * 1_000_000_000 / RATE
    }

    fn frame(&self, ftype: FrameType, seq: u32, ts: u64, body: &Body, pad: usize) -> Vec<u8> {
        let h = Header { version: VERSION, ftype, length: 0, seq, ts };
        encode(self.pc_mac.unwrap_or(EMU_PC_MAC), BOARD_MAC, &h, body, pad)
    }

    fn status(&self, h: u64, part: u32, board_ns: u64) -> Status {
        let total = self.rtu1_total(board_ns);
        let n = self.cfg.clip_frames as u64;
        Status {
            first_index: (h * 64) as u32 + part * 32,
            half_seq: (h + 1) as u32,
            rtu1_loops: (total / n) as u32,
            rtu1_frame_pos: (total % n) as u32,
            clip_frames: self.cfg.clip_frames,
            underrun_count: 0,
            sd_ovf_count: 0,
            batch_count: (board_ns * 3_072_000 / 28 / 1_000_000_000) as u32,
            rx_frames: self.c.rx_frames,
            rx_audio: self.c.rx_audio,
            rx_foreign: self.c.rx_foreign,
            rx_bad: self.c.rx_bad,
            last_in_end: self.c.last_in_end,
            tx_late: self.c.tx_late,
            detect_iep: self.half_due_ns(h),
        }
    }

    /// Queue everything that is due at `now`.
    fn produce(&mut self, now: u64) {
        let bns = self.board_ns(now);
        if self.cfg.audio_out {
            let total = self.rtu1_total(bns);
            let done = total.saturating_sub(self.cfg.pipeline_frames as u64) / 64; // halves complete
            if self.pc_mac.is_none() {
                self.next_half = done;
            } else if done > self.next_half {
                let newest = done - 1;
                if newest > self.next_half {
                    self.c.tx_late += 1;
                }
                for part in 0..2u32 {
                    let st = self.status(newest, part, bns);
                    let mut pcm = Box::new([[0i32; 8]; 32]);
                    for (k, row) in pcm.iter_mut().enumerate() {
                        let i = (newest * 64 + part as u64 * 32 + k as u64) % self.cfg.clip_frames as u64;
                        for (c, x) in row.iter_mut().enumerate() {
                            *x = (self.ring[i as usize][c] as i32) << 8;
                        }
                    }
                    self.out_seq += 1;
                    let drop = self.cfg.drop_every_nth_out.is_some_and(|n| self.out_seq % n == 0);
                    if !drop {
                        let ts = st.detect_iep + 1000 + part as u64 * 9000;
                        let f = self.frame(FrameType::AudioOut, self.out_seq - 1, ts,
                                           &Body::AudioOut { status: st, pcm }, 0);
                        self.queue.push_back(f);
                    }
                }
                self.next_half = done;
            }
        }
        while self.tput_left > 0 {
            let due = self.tput_start + self.tput_seq as u64 * TPUT_WIRE_NS;
            if now < due {
                break;
            }
            let seq = self.tput_seq;
            self.tput_seq += 1;
            self.tput_left -= 1;
            if self.cfg.pc_drop_every_nth_tput.is_some_and(|n| (seq + 1) % n == 0) {
                self.stats.rx_kernel_drops += 1;
                continue;
            }
            let f = self.frame(FrameType::TputData, seq, self.board_ns(due), &Body::TputData, 1514);
            self.queue.push_back(f);
        }
    }

    fn next_due(&self, now: u64) -> Option<u64> {
        let mut due: Option<u64> = None;
        if self.cfg.audio_out && self.pc_mac.is_some() {
            due = Some(self.t0 + self.half_due_ns(self.next_half));
        }
        if self.tput_left > 0 {
            let t = self.tput_start + self.tput_seq as u64 * TPUT_WIRE_NS;
            due = Some(due.map_or(t, |d| d.min(t)));
        }
        due.map(|d| d.max(now))
    }

    fn receive(&mut self, frame: &[u8], now: u64) {
        let to_us = frame.len() >= 6
            && (frame[0..6] == BOARD_MAC || frame[0..6] == [0xFF; 6]);
        let (h, body, src) = match decode(frame) {
            Ok(x) if to_us => x,
            Err(DecodeError::BadVersion) | Err(DecodeError::BadType) if to_us => {
                self.c.rx_bad += 1;
                return;
            }
            Ok(_) | Err(DecodeError::ForeignEtherType) | Err(_) => {
                self.c.rx_foreign += 1;
                return;
            }
        };
        self.c.rx_frames += 1;
        self.pc_mac = Some(src);
        let bns = self.board_ns(now);
        match body {
            Body::Ping { pc_ns } => {
                self.pongs += 1;
                if self.cfg.drop_every_nth_pong.is_some_and(|n| self.pongs % n == 0) {
                    return;
                }
                let b = Body::Pong { ping_seq: h.seq, pc_ns, rx_iep: bns };
                let f = self.frame(FrameType::Pong, self.pongs - 1, bns + self.cfg.turnaround_ns, &b, 64);
                self.queue.push_back(f);
            }
            Body::TputData => {
                let c = &mut self.c;
                if c.tput_frames == 0 {
                    c.tput_first = bns;
                } else if h.seq != c.tput_last_seq.wrapping_add(1) {
                    c.tput_gaps += 1;
                }
                c.tput_frames += 1;
                c.tput_bytes += h.length as u64;
                c.tput_last = bns;
                c.tput_last_seq = h.seq;
            }
            Body::AudioIn { first_index, pcm } => {
                if first_index % 64 != 0 {
                    self.c.rx_bad += 1;
                    return;
                }
                self.c.rx_audio += 1;
                for (k, row) in pcm.iter().enumerate() {
                    let i = (first_index as u64 + k as u64) % self.cfg.clip_frames as u64;
                    self.ring[i as usize] = *row;
                }
                self.c.last_in_end = first_index + 64;
            }
            Body::Control { cmd, arg0, .. } => match cmd {
                CMD_RESET_COUNTERS => {
                    let last = self.c.last_in_end;
                    self.c = Counters { last_in_end: last, ..Counters::default() };
                }
                CMD_TPUT_TX => {
                    self.tput_left = arg0;
                    self.tput_seq = 0;
                    self.tput_start = now;
                }
                CMD_TPUT_REPORT_REQ => {
                    let c = &self.c;
                    let b = Body::TputReport {
                        frames: c.tput_frames,
                        seq_gaps: c.tput_gaps,
                        bytes: c.tput_bytes,
                        first_iep: c.tput_first,
                        last_iep: c.tput_last,
                    };
                    let f = self.frame(FrameType::TputReport, 0, bns, &b, 64);
                    self.queue.push_back(f);
                }
                _ => self.c.rx_bad += 1,
            },
            Body::Pong { .. } | Body::TputReport { .. } | Body::AudioOut { .. } => {}
        }
    }
}

impl Link for Emulator {
    fn send(&mut self, frame: &[u8]) -> io::Result<()> {
        let now = now_ns();
        self.produce(now);
        self.receive(frame, now);
        Ok(())
    }

    fn recv(&mut self, timeout: Duration) -> io::Result<Option<RxFrame>> {
        let deadline = now_ns() + timeout.as_nanos() as u64;
        loop {
            let now = now_ns();
            self.produce(now);
            if let Some(data) = self.queue.pop_front() {
                return Ok(Some(RxFrame { data, t_ns: now_ns() }));
            }
            if now >= deadline {
                return Ok(None);
            }
            let wake = self.next_due(now).map_or(deadline, |d| d.min(deadline));
            std::thread::sleep(Duration::from_nanos(wake.saturating_sub(now).max(1)));
        }
    }

    fn mac(&self) -> [u8; 6] {
        EMU_PC_MAC
    }

    fn stats(&mut self) -> LinkStats {
        self.stats
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::{Duration, Instant};

    const T: Duration = Duration::from_millis(50);

    fn emu(audio_out: bool) -> Emulator {
        Emulator::new(EmuConfig { audio_out, ..EmuConfig::default() })
    }

    fn send(e: &mut Emulator, ftype: FrameType, seq: u32, b: Body) {
        let h = Header { version: VERSION, ftype, length: 0, seq, ts: 0 };
        let mac = e.mac();
        e.send(&encode(BOARD_MAC, mac, &h, &b, 60)).unwrap();
    }

    fn next(e: &mut Emulator, want: FrameType, within: Duration) -> Option<(Header, Body)> {
        let end = Instant::now() + within;
        while Instant::now() < end {
            if let Some(f) = e.recv(T).unwrap() {
                let (h, b, src) = decode(&f.data).unwrap();
                assert_eq!(src, BOARD_MAC);
                assert_eq!(&f.data[0..6], &e.mac());
                if h.ftype == want {
                    return Some((h, b));
                }
            }
        }
        None
    }

    #[test]
    fn emulator_is_silent_until_it_saw_a_frame() {
        let mut e = emu(true);
        assert!(e.recv(Duration::from_millis(30)).unwrap().is_none());
    }

    #[test]
    fn emulator_answers_ping() {
        let mut e = emu(false);
        send(&mut e, FrameType::Ping, 5, Body::Ping { pc_ns: 123 });
        let (_, b) = next(&mut e, FrameType::Pong, Duration::from_millis(200)).expect("pong");
        match b {
            Body::Pong { ping_seq, pc_ns, .. } => {
                assert_eq!(ping_seq, 5);
                assert_eq!(pc_ns, 123);
            }
            _ => unreachable!(),
        }
    }

    #[test]
    fn emulator_tput_tx_sends_count_frames() {
        let mut e = emu(false);
        send(&mut e, FrameType::Control, 0, Body::Control { cmd: CMD_TPUT_TX, arg0: 100, arg1: 1500 });
        let mut seqs = vec![];
        let end = Instant::now() + Duration::from_millis(500);
        while seqs.len() < 100 && Instant::now() < end {
            if let Some(f) = e.recv(T).unwrap() {
                assert_eq!(f.data.len(), 1514);
                let (h, _, _) = decode(&f.data).unwrap();
                assert_eq!(h.ftype, FrameType::TputData);
                seqs.push(h.seq);
            }
        }
        assert_eq!(seqs, (0..100).collect::<Vec<u32>>());
    }

    #[test]
    fn emulator_counts_pc_tput_and_reports() {
        let mut e = emu(false);
        for s in [0u32, 1, 2, 4] {
            send(&mut e, FrameType::TputData, s, Body::TputData);
        }
        send(&mut e, FrameType::Control, 0, Body::Control { cmd: CMD_TPUT_REPORT_REQ, arg0: 0, arg1: 0 });
        let (_, b) = next(&mut e, FrameType::TputReport, Duration::from_millis(200)).expect("report");
        match b {
            Body::TputReport { frames, seq_gaps, bytes, .. } => {
                assert_eq!((frames, seq_gaps), (4, 1));
                assert_eq!(bytes, 4 * (60 - 30));
            }
            _ => unreachable!(),
        }
    }

    #[test]
    fn emulator_audio_out_500_per_second() {
        let mut e = emu(true);
        send(&mut e, FrameType::Control, 0, Body::Control { cmd: CMD_RESET_COUNTERS, arg0: 0, arg1: 0 });
        let start = Instant::now();
        let mut idx = vec![];
        while start.elapsed() < Duration::from_secs(1) {
            if let Some(f) = e.recv(T).unwrap() {
                if let (_, Body::AudioOut { status, .. }, _) = decode(&f.data).unwrap() {
                    idx.push(status.first_index);
                }
            }
        }
        assert!((495..=505).contains(&idx.len()), "{} AUDIO_OUT frames", idx.len());
        assert!(idx.windows(2).all(|w| w[1] == w[0] + 32), "{idx:?}");
    }

    #[test]
    fn emulator_ignores_foreign_ethertype() {
        let mut e = emu(true);
        let mac = e.mac();
        let h = Header { version: VERSION, ftype: FrameType::Ping, length: 0, seq: 0, ts: 0 };
        let mut f = encode(BOARD_MAC, mac, &h, &Body::Ping { pc_ns: 0 }, 60);
        f[12] = 0x86;
        f[13] = 0xDD;
        e.send(&f).unwrap();
        // no reply and no learning from the foreign frame
        assert!(e.recv(Duration::from_millis(30)).unwrap().is_none());
        send(&mut e, FrameType::Control, 0, Body::Control { cmd: 99, arg0: 0, arg1: 0 });
        let (_, b) = next(&mut e, FrameType::AudioOut, Duration::from_millis(200)).expect("audio");
        let Body::AudioOut { status, .. } = b else { unreachable!() };
        assert_eq!(status.rx_foreign, 1);
        assert_eq!(status.rx_frames, 1);
        assert_eq!(status.rx_bad, 1); // unknown CONTROL command
    }

    #[test]
    fn emulator_audio_in_lands_in_ring() {
        let mut e = emu(true);
        send(&mut e, FrameType::Control, 0, Body::Control { cmd: CMD_RESET_COUNTERS, arg0: 0, arg1: 0 });
        let (_, b) = next(&mut e, FrameType::AudioOut, Duration::from_millis(200)).expect("audio");
        let Body::AudioOut { status, .. } = b else { unreachable!() };
        let target = (status.rtu1_total() as u32 + 640).div_ceil(64) * 64;
        let mut pcm = Box::new([[0i16; 8]; 64]);
        for (f, row) in pcm.iter_mut().enumerate() {
            for (c, x) in row.iter_mut().enumerate() {
                *x = (f * 8 + c) as i16 * 10 - 2000;
            }
        }
        send(&mut e, FrameType::AudioIn, 0, Body::AudioIn { first_index: target, pcm: pcm.clone() });
        let end = Instant::now() + Duration::from_millis(500);
        let mut seen = 0;
        while seen < 2 && Instant::now() < end {
            let Some(f) = e.recv(T).unwrap() else { continue };
            let (_, Body::AudioOut { status, pcm: out }, _) = decode(&f.data).unwrap() else { continue };
            for k in 0..32u32 {
                let i = status.first_index + k;
                if (target..target + 64).contains(&i) {
                    let fr = (i - target) as usize;
                    for c in 0..8 {
                        assert_eq!(out[k as usize][c], (pcm[fr][c] as i32) << 8, "index {i} ch {c}");
                    }
                    if k == 31 {
                        seen += 1;
                    }
                }
            }
            if status.first_index >= target {
                assert!(status.last_in_end >= target + 64);
            }
        }
        assert_eq!(seen, 2, "both AUDIO_OUT frames covering the input block");
    }

    #[test]
    fn emulator_reset_board_restarts_indices() {
        let mut e = emu(true);
        send(&mut e, FrameType::Control, 0, Body::Control { cmd: CMD_RESET_COUNTERS, arg0: 0, arg1: 0 });
        std::thread::sleep(Duration::from_millis(100));
        while e.recv(Duration::ZERO).unwrap().is_some() {}
        e.reset_board();
        send(&mut e, FrameType::Control, 0, Body::Control { cmd: CMD_RESET_COUNTERS, arg0: 0, arg1: 0 });
        let (_, b) = next(&mut e, FrameType::AudioOut, Duration::from_millis(200)).expect("audio");
        let Body::AudioOut { status, .. } = b else { unreachable!() };
        assert_eq!(status.first_index, 0);
    }
}
