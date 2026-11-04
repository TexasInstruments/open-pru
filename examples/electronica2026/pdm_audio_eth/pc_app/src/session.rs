// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! The command-line sessions `capture` and `stream`: receive (and send)
//! audio until done, keep the board talking after a reset.

use std::io;
use std::path::Path;
use std::time::Duration;

use crate::ethtest::hello;
use crate::link::{now_ns, Link};
use crate::pacing::Pacer;
use crate::proto::{decode, encode, Body, FrameType, Header, Status, BOARD_MAC, VERSION};
use crate::rx::{OutStream, WavCapture};
use crate::wavin::InputSet;

/// Re-send hello after this long without AUDIO_OUT (a reset board forgets
/// the PC MAC and sends nothing until it sees a frame).
const HELLO_AFTER_NS: u64 = 1_000_000_000;
/// `stream` gives up after this long without AUDIO_OUT.
const NO_AUDIO_NS: u64 = 3_000_000_000;

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
/// with `capture`, write in_ch*.wav (as sent) and out_ch*.wav there. Fails
/// after 3 s without AUDIO_OUT.
pub fn stream(link: &mut dyn Link, set: &InputSet, capture: Option<&Path>, lead: u32) -> io::Result<StreamReport> {
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
                let v = out.take_new();
                if let Some((_, o)) = &mut caps {
                    o.write(&v)?;
                }
            }
        while done_at.is_none() {
            let Some(first) = pacer.next_due(now_ns()) else { break };
            let block = send_block(link, set, pos, first, seq)?;
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
    }
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
        let err = stream(&mut e, &set, None, 8000).unwrap_err().to_string();
        assert!(err.contains("no AUDIO_OUT"), "{err}");
        assert!(t.elapsed() < Duration::from_secs(5));
    }

    #[test]
    fn stream_survives_board_reset() {
        let mut e = Emulator::new(EmuConfig { reset_after_ms: Some(400), ..EmuConfig::default() });
        let mut set = InputSet::default();
        set.ch[0] = vec![100; 40_000];
        let r = stream(&mut e, &set, None, 8000).unwrap();
        assert!(r.restarts >= 1, "{r:?}");
        assert_eq!(r.sent_frames as usize, 40_000usize.div_ceil(64) * 64);
    }
}
