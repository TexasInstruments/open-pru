// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! The command-line sessions `capture` and `stream`: receive (and send)
//! audio until done, keep the board talking after a reset.

use std::io;
use std::path::Path;
use std::time::Duration;

use crate::ethtest::hello;
use crate::latency::LatencyMeter;
use crate::link::{now_ns, Link};
use crate::pacing::Pacer;
use crate::proto::{decode, encode, Body, FrameType, Header, Status, BOARD_MAC, VERSION};
use crate::rx::{OutStream, WavCapture};
use crate::seam::{Aligner, ShmExport};
use crate::wavin::InputSet;

/// Re-send hello after this long without AUDIO_OUT (a reset board forgets
/// the PC MAC and sends nothing until it sees a frame).
const HELLO_AFTER_NS: u64 = 1_000_000_000;
/// `stream` gives up after this long without AUDIO_OUT.
const NO_AUDIO_NS: u64 = 3_000_000_000;
/// Shared-memory ring of `stream --export-shm`: 4096 blocks = 16 s. Larger
/// than the GUI's, because the frames sent before the lag is known (about
/// 1.5 s) are exported in one go once it is.
pub const STREAM_SHM_BLOCKS: usize = 4096;
/// Frames kept for the export while the lag is not known yet (30 s).
const BACKLOG: usize = 480_000;

/// Says hello at the start and again while the board is silent.
struct Keepalive {
    last_audio: u64,
    last_hello: u64,
}

impl Keepalive {
    fn new() -> Self {
        Keepalive { last_audio: now_ns(), last_hello: 0 }
    }

    fn tick(&mut self, link: &mut dyn Link) -> io::Result<()> {
        let now = now_ns();
        if self.last_hello == 0
            || (now - self.last_audio > HELLO_AFTER_NS && now - self.last_hello > HELLO_AFTER_NS)
        {
            hello(link)?;
            self.last_hello = now;
        }
        Ok(())
    }

    fn audio(&mut self) {
        self.last_audio = now_ns();
    }

    fn silent_ns(&self) -> u64 {
        now_ns() - self.last_audio
    }
}

/// What `capture` saw.
#[derive(Clone, Debug, Default)]
pub struct CaptureReport {
    pub written: u64,
    pub lost: u64,
    pub duplicates: u64,
    pub resyncs: u64,
    pub status: Status,
    pub clock_ppm: Option<f64>,
}

/// Receive the board output for `seconds`, optionally into WAV files.
pub fn capture(link: &mut dyn Link, seconds: f64, mut wav: Option<&mut WavCapture>) -> io::Result<CaptureReport> {
    let mut s = OutStream::new();
    let mut k = Keepalive::new();
    let (mut written, end) = (0u64, now_ns() + (seconds * 1e9) as u64);
    while now_ns() < end {
        k.tick(link)?;
        let Some(f) = link.recv(Duration::from_millis(20))? else { continue };
        if let Ok((h, Body::AudioOut { status, pcm }, _)) = decode(&f.data) {
            k.audio();
            s.push(&status, &pcm, h.ts, f.t_ns);
            let v = s.take_new();
            written += v.len() as u64;
            if let Some(w) = wav.as_mut() {
                w.write(&v)?;
            }
        }
    }
    Ok(CaptureReport {
        written,
        lost: s.lost_frames,
        duplicates: s.duplicates,
        resyncs: s.resyncs,
        status: s.last_status().copied().unwrap_or_default(),
        clock_ppm: s.board_clock_ppm(),
    })
}

/// What `stream` did.
#[derive(Clone, Debug, Default)]
pub struct StreamReport {
    pub sent_blocks: u64,
    pub sent_frames: u64,
    pub underruns: u64,
    pub overwrite_blocked: u64,
    pub restarts: u64,
    pub lost: u64,
    pub duplicates: u64,
    pub resyncs: u64,
    pub status: Status,
    /// Output index k carries input index k + lag (board indices), if found.
    pub lag: Option<i64>,
    /// Board audio latency (as in the GUI), if found.
    pub latency_ms: Option<f64>,
    /// Aligned 64-frame blocks written to the shared-memory export.
    pub exported_blocks: u64,
    /// Board index of the first sent frame (file sample 0).
    pub first_index: Option<u64>,
}

/// The channel the lag is measured on: the one with the most energy.
fn loudest_channel(set: &InputSet) -> usize {
    (0..8).max_by_key(|&c| set.ch[c].iter().map(|v| (*v as i64).pow(2)).sum::<i64>()).unwrap_or(0)
}

/// Feeds the shared-memory export: keeps what was sent and received until
/// the lag is known, then aligns from the first sent frame on.
struct Export<'a> {
    shm: &'a mut ShmExport,
    aligner: Aligner,
    inp: Vec<(u64, [i16; 8])>,
    out: Vec<(u64, [i32; 8])>,
    blocks: u64,
}

impl<'a> Export<'a> {
    fn new(shm: &'a mut ShmExport) -> Self {
        Export { shm, aligner: Aligner::new(), inp: Vec::new(), out: Vec::new(), blocks: 0 }
    }

    fn push(&mut self, inp: &[(u64, [i16; 8])], out: &[(u64, [i32; 8])]) {
        self.inp.extend_from_slice(inp);
        self.out.extend_from_slice(out);
        if self.inp.len() > BACKLOG {
            self.inp.drain(..self.inp.len() - BACKLOG);
        }
        if self.out.len() > BACKLOG {
            self.out.drain(..self.out.len() - BACKLOG);
        }
    }

    fn flush(&mut self, lag: Option<i64>) -> io::Result<()> {
        let Some(l) = lag else { return Ok(()) };
        self.aligner.set_lag(l);
        self.aligner.push_in(&std::mem::take(&mut self.inp));
        self.aligner.push_out(&std::mem::take(&mut self.out));
        for (i, a, b) in self.aligner.take_blocks() {
            self.shm.write(i, &a, &b)?;
            self.blocks += 1;
        }
        Ok(())
    }
}

fn send_block(link: &mut dyn Link, set: &InputSet, pos: usize, first: u32, seq: u32) -> io::Result<[[i16; 8]; 64]> {
    let mut pcm = Box::new([[0i16; 8]; 64]);
    for (k, row) in pcm.iter_mut().enumerate() {
        *row = set.frame(pos + k);
    }
    let block = *pcm;
    let h = Header { version: VERSION, ftype: FrameType::AudioIn, length: 0, seq, ts: now_ns() };
    let mac = link.mac();
    link.send(&encode(BOARD_MAC, mac, &h, &Body::AudioIn { first_index: first, pcm }, 0))?;
    Ok(block)
}

/// Stream `set` into the board once, paced `lead` frames ahead of RTU1;
/// with `capture`, write in_ch*.wav (as sent) and out_ch*.wav there; with
/// `export`, write aligned input/output blocks to the shared-memory ring
/// (from the first sent frame, also those sent before the lag was found).
/// Fails after 3 s without AUDIO_OUT.
pub fn stream(link: &mut dyn Link, set: &InputSet, capture: Option<&Path>, lead: u32,
              export: Option<&mut ShmExport>) -> io::Result<StreamReport> {
    let mut export = export.map(Export::new);
    let mut meter = LatencyMeter::new();
    let mc = loudest_channel(set);
    let mut first_index = None;
    let mut out = OutStream::new();
    let mut pacer = Pacer::new(lead, 16384);
    let mut k = Keepalive::new();
    let mut caps = match capture {
        Some(d) => Some((WavCapture::create_bits(d, "in", 16)?, WavCapture::create(d, "out")?)),
        None => None,
    };
    let (mut pos, mut seq, mut done_at) = (0usize, 0u32, None::<u64>);
    let total = set.frames();
    let tail_ns = (lead as u64 + 4000) * 1_000_000_000 / 16_000; // drain the lead and the pipeline
    loop {
        let now = now_ns();
        if done_at.is_some_and(|t| now > t + tail_ns) {
            break;
        }
        if k.silent_ns() > NO_AUDIO_NS {
            return Err(io::Error::other(
                "no AUDIO_OUT from the board for 3 s: is it running, and is the cable on its ICSSG1 port?",
            ));
        }
        k.tick(link)?;
        if let Some(f) = link.recv(Duration::from_millis(1))?
            && let Ok((h, Body::AudioOut { status, pcm }, _)) = decode(&f.data) {
                k.audio();
                out.push(&status, &pcm, h.ts, f.t_ns);
                pacer.on_status(&status, f.t_ns);
                meter.push_status(&status);
                let y: Vec<i32> = pcm.iter().map(|r| r[mc]).collect();
                meter.push_out(status.first_index as u64, &y, h.ts);
                let v = out.take_new();
                if let Some((_, o)) = &mut caps {
                    o.write(&v)?;
                }
                if let Some(x) = &mut export {
                    x.push(&[], &v);
                }
            }
        while done_at.is_none() {
            let Some(first) = pacer.next_due(now_ns()) else { break };
            let block = send_block(link, set, pos, first, seq)?;
            first_index.get_or_insert(first as u64);
            let x: Vec<i16> = block.iter().map(|r| r[mc]).collect();
            meter.push_in(first as u64, &x);
            if let Some(x) = &mut export {
                let v: Vec<(u64, [i16; 8])> = block.iter().enumerate().map(|(k, f)| (first as u64 + k as u64, *f)).collect();
                x.push(&v, &[]);
            }
            if let Some((i, _)) = &mut caps {
                let v: Vec<(u64, [i32; 8])> =
                    block.iter().enumerate().map(|(k, f)| ((pos + k) as u64, f.map(|x| x as i32))).collect();
                i.write(&v)?;
            }
            seq += 1;
            pos += 64;
            if pos >= total {
                done_at = Some(now_ns());
                pacer.finish();
            }
        }
        let _ = meter.board_latency_ms(); // finds the lag (at most once per second of output)
        if let Some(x) = &mut export {
            x.flush(meter.lag())?;
        }
    }
    let latency_ms = meter.board_latency_ms();
    let exported_blocks = match &mut export {
        Some(x) => {
            x.flush(meter.lag())?;
            x.blocks
        }
        None => 0,
    };
    if let Some((i, o)) = caps {
        i.finish()?;
        o.finish()?;
    }
    Ok(StreamReport {
        sent_blocks: seq as u64,
        sent_frames: pos as u64,
        underruns: pacer.underruns,
        overwrite_blocked: pacer.overwrite_blocked,
        restarts: pacer.restarts,
        lost: out.lost_frames,
        duplicates: out.duplicates,
        resyncs: out.resyncs,
        status: out.last_status().copied().unwrap_or_default(),
        lag: meter.lag(),
        latency_ms,
        exported_blocks,
        first_index,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::emulator::{EmuConfig, Emulator};

    #[test]
    fn capture_survives_board_reset() {
        // the board ran 1.5 s, then resets: its IEP restarts, it forgets the
        // PC and sends nothing until the PC says hello again
        let mut e = Emulator::new(EmuConfig { reset_after_ms: Some(1500), ..EmuConfig::default() });
        let r = capture(&mut e, 4.0, None).unwrap();
        assert!(r.resyncs >= 1, "{r:?}");
        assert_eq!(r.lost, 0, "{r:?}");
        assert!(r.written > 40_000, "only {} frames: no audio after the reset", r.written);
    }

    #[test]
    fn stream_times_out_without_board() {
        let mut e = Emulator::new(EmuConfig { audio_out: false, ..EmuConfig::default() });
        let mut set = InputSet::default();
        set.ch[0] = vec![100; 16000];
        let t = std::time::Instant::now();
        let err = stream(&mut e, &set, None, 8000, None).unwrap_err().to_string();
        assert!(err.contains("no AUDIO_OUT"), "{err}");
        assert!(t.elapsed() < Duration::from_secs(5));
    }

    #[test]
    fn stream_exports_aligned_blocks_from_the_first_frame() {
        // 3 s of 8 different noise-like channels (the lag search needs a
        // signal that correlates at one lag only)
        let mut set = InputSet::default();
        let mut r = 12345u32;
        for c in 0..8 {
            set.ch[c] = (0..48_000)
                .map(|_| {
                    r = r.wrapping_mul(1_103_515_245).wrapping_add(12345);
                    ((r >> 16) as i16 >> 2) / (c as i16 + 1)
                })
                .collect();
        }
        let name = format!("pdm_eth_test_{}", std::process::id());
        let mut shm = ShmExport::create(&name, STREAM_SHM_BLOCKS).unwrap();
        let mut e = Emulator::new(EmuConfig::default());
        let rep = stream(&mut e, &set, None, 8000, Some(&mut shm)).unwrap();
        let bytes = std::fs::read(std::path::Path::new("/dev/shm").join(&name)).unwrap();
        drop(shm);
        assert!(rep.lag.is_some(), "{rep:?}");
        let first = rep.first_index.unwrap();
        let w = u64::from_le_bytes(bytes[16..24].try_into().unwrap());
        assert_eq!(w, rep.exported_blocks);
        assert_eq!(w, 48_000 / 64, "every sent frame exported once: {rep:?}");
        let bb = crate::seam::BLOCK_BYTES;
        // the emulator skips a half when the PC loop stalls > 4 ms (the lag
        // search); rx counts those frames as lost and fills them with silence
        let mut zero_filled = 0u64;
        for b in 0..w as usize {
            let blk = &bytes[64 + b * bb..64 + (b + 1) * bb];
            let idx = u64::from_le_bytes(blk[0..8].try_into().unwrap());
            assert_eq!(idx, first + 64 * b as u64, "block {b}");
            for k in 0..64 {
                let pos = (idx - first) as usize + k;
                let mut row = [(0i16, 0i32); 8];
                for (c, r) in row.iter_mut().enumerate() {
                    let o = 8 + (k * 8 + c) * 2;
                    r.0 = i16::from_le_bytes(blk[o..o + 2].try_into().unwrap());
                    let o = 8 + 64 * 8 * 2 + (k * 8 + c) * 4;
                    r.1 = i32::from_le_bytes(blk[o..o + 4].try_into().unwrap());
                    assert_eq!(r.0, set.ch[c][pos], "input, block {b} frame {k} ch {c}");
                }
                if row.iter().all(|r| r.1 == 0) && row.iter().any(|r| r.0 != 0) {
                    zero_filled += 1;
                    continue;
                }
                for (c, r) in row.iter().enumerate() {
                    assert_eq!(r.1, (r.0 as i32) << 8, "output, block {b} frame {k} ch {c}: {rep:?}");
                }
            }
        }
        assert!(zero_filled <= rep.lost * 32, "{zero_filled} silent output frames, {} AUDIO_OUT frames (32 each) counted lost", rep.lost);
        assert!(zero_filled <= 4 * 64, "{zero_filled} silent output frames");
    }

    #[test]
    fn stream_survives_board_reset() {
        let mut e = Emulator::new(EmuConfig { reset_after_ms: Some(400), ..EmuConfig::default() });
        let mut set = InputSet::default();
        set.ch[0] = vec![100; 40_000];
        let r = stream(&mut e, &set, None, 8000, None).unwrap();
        assert!(r.restarts >= 1, "{r:?}");
        assert_eq!(r.sent_frames as usize, 40_000usize.div_ceil(64) * 64);
    }
}
