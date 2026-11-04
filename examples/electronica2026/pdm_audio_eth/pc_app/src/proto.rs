// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Frame protocol of pdm_audio_eth (design 6, plan "Protocol").

use std::fmt;

/// Every protocol constant, named exactly as in `eth_proto.inc` / `eth_proto.h`.
#[allow(missing_docs)]
pub mod layout {
    pub const ETH_ETHERTYPE: usize = 0x88B5;
    pub const ETH_VERSION: usize = 1;
    pub const ETH_L2_SIZE: usize = 14;
    pub const ETH_HDR_SIZE: usize = 16;
    pub const ETH_PAYLOAD_MAX: usize = 1500;
    pub const ETH_FRAME_MAX: usize = 1514;
    pub const ETH_FRAME_MIN: usize = 60;
    pub const ETH_BOARD_MAC_0: usize = 0x02;
    pub const ETH_BOARD_MAC_1: usize = 0x00;
    pub const ETH_BOARD_MAC_2: usize = 0x5D;
    pub const ETH_BOARD_MAC_3: usize = 0x4D;
    pub const ETH_BOARD_MAC_4: usize = 0x00;
    pub const ETH_BOARD_MAC_5: usize = 0x01;
    // common header, offsets from the payload start
    pub const ETH_HDR_VERSION: usize = 0;
    pub const ETH_HDR_TYPE: usize = 1;
    pub const ETH_HDR_LENGTH: usize = 2;
    pub const ETH_HDR_SEQ: usize = 4;
    pub const ETH_HDR_TS: usize = 8;
    // frame types
    pub const ETH_T_PING: usize = 1;
    pub const ETH_T_PONG: usize = 2;
    pub const ETH_T_TPUT_DATA: usize = 3;
    pub const ETH_T_TPUT_REPORT: usize = 4;
    pub const ETH_T_AUDIO_IN: usize = 5;
    pub const ETH_T_AUDIO_OUT: usize = 6;
    pub const ETH_T_CONTROL: usize = 7;
    // bodies, offsets from the body start (payload + ETH_HDR_SIZE)
    pub const ETH_PING_PC_NS: usize = 0;
    pub const ETH_PING_SIZE: usize = 8;
    pub const ETH_PONG_PING_SEQ: usize = 0;
    pub const ETH_PONG_PC_NS: usize = 8;
    pub const ETH_PONG_RX_IEP: usize = 16;
    pub const ETH_PONG_SIZE: usize = 24;
    pub const ETH_TR_FRAMES: usize = 0;
    pub const ETH_TR_SEQ_GAPS: usize = 4;
    pub const ETH_TR_BYTES: usize = 8;
    pub const ETH_TR_FIRST_IEP: usize = 16;
    pub const ETH_TR_LAST_IEP: usize = 24;
    pub const ETH_TR_SIZE: usize = 32;
    pub const ETH_AI_FIRST_INDEX: usize = 0;
    pub const ETH_AI_PCM: usize = 8;
    pub const ETH_AI_FRAMES: usize = 64;
    pub const ETH_AI_SIZE: usize = 1032;
    pub const ETH_AO_STATUS: usize = 0;
    pub const ETH_AO_PCM: usize = 64;
    pub const ETH_AO_FRAMES: usize = 32;
    pub const ETH_AO_SIZE: usize = 1088;
    pub const ETH_CTL_CMD: usize = 0;
    pub const ETH_CTL_ARG0: usize = 4;
    pub const ETH_CTL_ARG1: usize = 8;
    pub const ETH_CTL_SIZE: usize = 12;
    pub const ETH_CMD_RESET_COUNTERS: usize = 1;
    pub const ETH_CMD_TPUT_TX: usize = 2;
    pub const ETH_CMD_TPUT_REPORT_REQ: usize = 3;
    // AUDIO_OUT status block, offsets from the block start
    pub const ETH_ST_FIRST_INDEX: usize = 0;
    pub const ETH_ST_HALF_SEQ: usize = 4;
    pub const ETH_ST_RTU1_LOOPS: usize = 8;
    pub const ETH_ST_RTU1_FRAME_POS: usize = 12;
    pub const ETH_ST_CLIP_FRAMES: usize = 16;
    pub const ETH_ST_UNDERRUN_COUNT: usize = 20;
    pub const ETH_ST_SD_OVF_COUNT: usize = 24;
    pub const ETH_ST_BATCH_COUNT: usize = 28;
    pub const ETH_ST_RX_FRAMES: usize = 32;
    pub const ETH_ST_RX_AUDIO: usize = 36;
    pub const ETH_ST_RX_FOREIGN: usize = 40;
    pub const ETH_ST_RX_BAD: usize = 44;
    pub const ETH_ST_LAST_IN_END: usize = 48;
    pub const ETH_ST_TX_LATE: usize = 52;
    pub const ETH_ST_DETECT_IEP: usize = 56;
    pub const ETH_ST_SIZE: usize = 64;
    // ICSSG1 shared RAM (local 0x10000, global 0x30090000)
    pub const ETH1_SHARED_LOCAL: usize = 0x10000;
    pub const ETH1_SHARED_GLOBAL: usize = 0x3009_0000;
    pub const ETH1_MAGIC_VALUE: usize = 0x3148_5445;
    pub const ETH1_VERSION_VALUE: usize = 1;
    pub const ETH1_MAGIC: usize = 0x00;
    pub const ETH1_VERSION: usize = 0x04;
    pub const ETH1_PC_MAC: usize = 0x08;
    pub const ETH1_MAC_VALID: usize = 0x0E;
    pub const ETH1_RX_FRAMES: usize = 0x10;
    pub const ETH1_RX_AUDIO: usize = 0x14;
    pub const ETH1_RX_FOREIGN: usize = 0x18;
    pub const ETH1_RX_BAD: usize = 0x1C;
    pub const ETH1_LAST_IN_END: usize = 0x20;
    pub const ETH1_PING_REQ: usize = 0x24;
    pub const ETH1_PING_PC_NS: usize = 0x28;
    pub const ETH1_PING_RX_IEP: usize = 0x30;
    pub const ETH1_TPUT_TX_COUNT: usize = 0x38;
    pub const ETH1_TPUT_TX_LEN: usize = 0x3C;
    pub const ETH1_TPUT_RX_FRAMES: usize = 0x40;
    pub const ETH1_TPUT_RX_GAPS: usize = 0x44;
    pub const ETH1_TPUT_RX_BYTES: usize = 0x48;
    pub const ETH1_TPUT_RX_FIRST_IEP: usize = 0x50;
    pub const ETH1_TPUT_RX_LAST_IEP: usize = 0x58;
    pub const ETH1_REPORT_REQ: usize = 0x60;
    pub const ETH1_TX_AUDIO: usize = 0x64;
    pub const ETH1_TX_PONGS: usize = 0x68;
    pub const ETH1_TX_LATE: usize = 0x6C;
    pub const ETH1_CLIP_ADDR: usize = 0x70;
    pub const ETH1_CLIP_FRAMES: usize = 0x74;
    pub const ETH1_SIZE: usize = 0x78;
}

/// `(name, value)` of every constant in [`layout`], for the mirror test.
pub mod mirror {
    use super::layout::*;
    macro_rules! all { ($($n:ident),* $(,)?) => { &[$((stringify!($n), $n as u64)),*] } }
    /// All mirrored constants.
    pub const ALL: &[(&str, u64)] = all!(
        ETH_ETHERTYPE, ETH_VERSION, ETH_L2_SIZE, ETH_HDR_SIZE, ETH_PAYLOAD_MAX, ETH_FRAME_MAX,
        ETH_FRAME_MIN, ETH_BOARD_MAC_0, ETH_BOARD_MAC_1, ETH_BOARD_MAC_2, ETH_BOARD_MAC_3,
        ETH_BOARD_MAC_4, ETH_BOARD_MAC_5, ETH_HDR_VERSION, ETH_HDR_TYPE, ETH_HDR_LENGTH,
        ETH_HDR_SEQ, ETH_HDR_TS, ETH_T_PING, ETH_T_PONG, ETH_T_TPUT_DATA, ETH_T_TPUT_REPORT,
        ETH_T_AUDIO_IN, ETH_T_AUDIO_OUT, ETH_T_CONTROL, ETH_PING_PC_NS, ETH_PING_SIZE,
        ETH_PONG_PING_SEQ, ETH_PONG_PC_NS, ETH_PONG_RX_IEP, ETH_PONG_SIZE, ETH_TR_FRAMES,
        ETH_TR_SEQ_GAPS, ETH_TR_BYTES, ETH_TR_FIRST_IEP, ETH_TR_LAST_IEP, ETH_TR_SIZE,
        ETH_AI_FIRST_INDEX, ETH_AI_PCM, ETH_AI_FRAMES, ETH_AI_SIZE, ETH_AO_STATUS, ETH_AO_PCM,
        ETH_AO_FRAMES, ETH_AO_SIZE, ETH_CTL_CMD, ETH_CTL_ARG0, ETH_CTL_ARG1, ETH_CTL_SIZE,
        ETH_CMD_RESET_COUNTERS, ETH_CMD_TPUT_TX, ETH_CMD_TPUT_REPORT_REQ, ETH_ST_FIRST_INDEX,
        ETH_ST_HALF_SEQ, ETH_ST_RTU1_LOOPS, ETH_ST_RTU1_FRAME_POS, ETH_ST_CLIP_FRAMES,
        ETH_ST_UNDERRUN_COUNT, ETH_ST_SD_OVF_COUNT, ETH_ST_BATCH_COUNT, ETH_ST_RX_FRAMES,
        ETH_ST_RX_AUDIO, ETH_ST_RX_FOREIGN, ETH_ST_RX_BAD, ETH_ST_LAST_IN_END, ETH_ST_TX_LATE,
        ETH_ST_DETECT_IEP, ETH_ST_SIZE, ETH1_SHARED_LOCAL, ETH1_SHARED_GLOBAL, ETH1_MAGIC_VALUE,
        ETH1_VERSION_VALUE, ETH1_MAGIC, ETH1_VERSION, ETH1_PC_MAC, ETH1_MAC_VALID, ETH1_RX_FRAMES,
        ETH1_RX_AUDIO, ETH1_RX_FOREIGN, ETH1_RX_BAD, ETH1_LAST_IN_END, ETH1_PING_REQ,
        ETH1_PING_PC_NS, ETH1_PING_RX_IEP, ETH1_TPUT_TX_COUNT, ETH1_TPUT_TX_LEN,
        ETH1_TPUT_RX_FRAMES, ETH1_TPUT_RX_GAPS, ETH1_TPUT_RX_BYTES, ETH1_TPUT_RX_FIRST_IEP,
        ETH1_TPUT_RX_LAST_IEP, ETH1_REPORT_REQ, ETH1_TX_AUDIO, ETH1_TX_PONGS, ETH1_TX_LATE,
        ETH1_CLIP_ADDR, ETH1_CLIP_FRAMES, ETH1_SIZE,
    );
}

use layout::*;

/// EtherType of every frame of this protocol.
pub const ETHERTYPE: u16 = ETH_ETHERTYPE as u16;
/// Protocol version in the common header.
pub const VERSION: u8 = ETH_VERSION as u8;
/// The board's fixed, locally administered MAC address.
pub const BOARD_MAC: [u8; 6] = [
    ETH_BOARD_MAC_0 as u8, ETH_BOARD_MAC_1 as u8, ETH_BOARD_MAC_2 as u8,
    ETH_BOARD_MAC_3 as u8, ETH_BOARD_MAC_4 as u8, ETH_BOARD_MAC_5 as u8,
];
/// CONTROL: zero the board's RX and throughput counters.
pub const CMD_RESET_COUNTERS: u32 = ETH_CMD_RESET_COUNTERS as u32;
/// CONTROL: board sends `arg0` TPUT_DATA frames of `arg1` payload bytes.
pub const CMD_TPUT_TX: u32 = ETH_CMD_TPUT_TX as u32;
/// CONTROL: board sends one TPUT_REPORT.
pub const CMD_TPUT_REPORT_REQ: u32 = ETH_CMD_TPUT_REPORT_REQ as u32;
/// PCM frames in one AUDIO_IN frame.
pub const AUDIO_IN_FRAMES: usize = ETH_AI_FRAMES;
/// PCM frames in one AUDIO_OUT frame.
pub const AUDIO_OUT_FRAMES: usize = ETH_AO_FRAMES;

/// Frame type in the common header.
#[repr(u8)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum FrameType {
    Ping = 1,
    Pong = 2,
    TputData = 3,
    TputReport = 4,
    AudioIn = 5,
    AudioOut = 6,
    Control = 7,
}

impl FrameType {
    fn from_u8(v: u8) -> Option<Self> {
        Some(match v {
            1 => Self::Ping,
            2 => Self::Pong,
            3 => Self::TputData,
            4 => Self::TputReport,
            5 => Self::AudioIn,
            6 => Self::AudioOut,
            7 => Self::Control,
            _ => return None,
        })
    }

    fn min_body(self) -> usize {
        match self {
            Self::Ping => ETH_PING_SIZE,
            Self::Pong => ETH_PONG_SIZE,
            Self::TputData => 0,
            Self::TputReport => ETH_TR_SIZE,
            Self::AudioIn => ETH_AI_SIZE,
            Self::AudioOut => ETH_AO_SIZE,
            Self::Control => ETH_CTL_SIZE,
        }
    }
}

/// Common header. `length` is the payload byte count after the header;
/// `encode` computes it, the value passed in is ignored.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Header {
    pub version: u8,
    pub ftype: FrameType,
    pub length: u16,
    pub seq: u32,
    pub ts: u64,
}

/// AUDIO_OUT status block.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct Status {
    pub first_index: u32,
    pub half_seq: u32,
    pub rtu1_loops: u32,
    pub rtu1_frame_pos: u32,
    pub clip_frames: u32,
    pub underrun_count: u32,
    pub sd_ovf_count: u32,
    pub batch_count: u32,
    pub rx_frames: u32,
    pub rx_audio: u32,
    pub rx_foreign: u32,
    pub rx_bad: u32,
    pub last_in_end: u32,
    pub tx_late: u32,
    pub detect_iep: u64,
}

impl Status {
    /// Frames RTU1 has taken from the clip buffer since it started.
    pub fn rtu1_total(&self) -> u64 {
        self.rtu1_loops as u64 * self.clip_frames as u64 + self.rtu1_frame_pos as u64
    }
}

/// Frame body after the common header.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Body {
    Ping { pc_ns: u64 },
    Pong { ping_seq: u32, pc_ns: u64, rx_iep: u64 },
    TputData,
    TputReport { frames: u32, seq_gaps: u32, bytes: u64, first_iep: u64, last_iep: u64 },
    AudioIn { first_index: u32, pcm: Box<[[i16; 8]; 64]> },
    AudioOut { status: Status, pcm: Box<[[i32; 8]; 32]> },
    Control { cmd: u32, arg0: u32, arg1: u32 },
}

/// Why a frame was not decoded.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DecodeError {
    Short,
    ForeignEtherType,
    BadVersion,
    BadType,
    BadLength,
}

impl fmt::Display for DecodeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{self:?}")
    }
}

impl std::error::Error for DecodeError {}

fn put32(v: &mut Vec<u8>, x: u32) { v.extend_from_slice(&x.to_le_bytes()); }
fn put64(v: &mut Vec<u8>, x: u64) { v.extend_from_slice(&x.to_le_bytes()); }
fn get32(b: &[u8], o: usize) -> u32 { u32::from_le_bytes(b[o..o + 4].try_into().unwrap()) }
fn get64(b: &[u8], o: usize) -> u64 { u64::from_le_bytes(b[o..o + 8].try_into().unwrap()) }

fn put_status(v: &mut Vec<u8>, s: &Status) {
    for x in [
        s.first_index, s.half_seq, s.rtu1_loops, s.rtu1_frame_pos, s.clip_frames,
        s.underrun_count, s.sd_ovf_count, s.batch_count, s.rx_frames, s.rx_audio,
        s.rx_foreign, s.rx_bad, s.last_in_end, s.tx_late,
    ] {
        put32(v, x);
    }
    put64(v, s.detect_iep);
}

fn get_status(b: &[u8]) -> Status {
    Status {
        first_index: get32(b, ETH_ST_FIRST_INDEX),
        half_seq: get32(b, ETH_ST_HALF_SEQ),
        rtu1_loops: get32(b, ETH_ST_RTU1_LOOPS),
        rtu1_frame_pos: get32(b, ETH_ST_RTU1_FRAME_POS),
        clip_frames: get32(b, ETH_ST_CLIP_FRAMES),
        underrun_count: get32(b, ETH_ST_UNDERRUN_COUNT),
        sd_ovf_count: get32(b, ETH_ST_SD_OVF_COUNT),
        batch_count: get32(b, ETH_ST_BATCH_COUNT),
        rx_frames: get32(b, ETH_ST_RX_FRAMES),
        rx_audio: get32(b, ETH_ST_RX_AUDIO),
        rx_foreign: get32(b, ETH_ST_RX_FOREIGN),
        rx_bad: get32(b, ETH_ST_RX_BAD),
        last_in_end: get32(b, ETH_ST_LAST_IN_END),
        tx_late: get32(b, ETH_ST_TX_LATE),
        detect_iep: get64(b, ETH_ST_DETECT_IEP),
    }
}

/// Build one Ethernet frame (without CRC). The payload is zero-padded so
/// that the frame is at least `pad_to` bytes long.
pub fn encode(dst: [u8; 6], src: [u8; 6], h: &Header, b: &Body, pad_to: usize) -> Vec<u8> {
    let mut body = Vec::with_capacity(ETH_AO_SIZE);
    match b {
        Body::Ping { pc_ns } => put64(&mut body, *pc_ns),
        Body::Pong { ping_seq, pc_ns, rx_iep } => {
            put32(&mut body, *ping_seq);
            put32(&mut body, 0);
            put64(&mut body, *pc_ns);
            put64(&mut body, *rx_iep);
        }
        Body::TputData => {}
        Body::TputReport { frames, seq_gaps, bytes, first_iep, last_iep } => {
            put32(&mut body, *frames);
            put32(&mut body, *seq_gaps);
            put64(&mut body, *bytes);
            put64(&mut body, *first_iep);
            put64(&mut body, *last_iep);
        }
        Body::AudioIn { first_index, pcm } => {
            put32(&mut body, *first_index);
            put32(&mut body, 0);
            for f in pcm.iter() {
                for x in f {
                    body.extend_from_slice(&x.to_le_bytes());
                }
            }
        }
        Body::AudioOut { status, pcm } => {
            put_status(&mut body, status);
            for f in pcm.iter() {
                for x in f {
                    body.extend_from_slice(&x.to_le_bytes());
                }
            }
        }
        Body::Control { cmd, arg0, arg1 } => {
            put32(&mut body, *cmd);
            put32(&mut body, *arg0);
            put32(&mut body, *arg1);
        }
    }
    let total = (ETH_L2_SIZE + ETH_HDR_SIZE + body.len()).max(pad_to);
    body.resize(total - ETH_L2_SIZE - ETH_HDR_SIZE, 0);
    let mut v = Vec::with_capacity(total);
    v.extend_from_slice(&dst);
    v.extend_from_slice(&src);
    v.extend_from_slice(&ETHERTYPE.to_be_bytes());
    v.push(VERSION);
    v.push(h.ftype as u8);
    v.extend_from_slice(&(body.len() as u16).to_le_bytes());
    put32(&mut v, h.seq);
    put64(&mut v, h.ts);
    v.extend_from_slice(&body);
    v
}

/// Parse one Ethernet frame (without CRC; trailing padding is allowed).
/// Returns the header, the body and the source MAC.
pub fn decode(frame: &[u8]) -> Result<(Header, Body, [u8; 6]), DecodeError> {
    if frame.len() < ETH_L2_SIZE + ETH_HDR_SIZE {
        return Err(DecodeError::Short);
    }
    if u16::from_be_bytes([frame[12], frame[13]]) != ETHERTYPE {
        return Err(DecodeError::ForeignEtherType);
    }
    let p = &frame[ETH_L2_SIZE..];
    if p[ETH_HDR_VERSION] != VERSION {
        return Err(DecodeError::BadVersion);
    }
    let ftype = FrameType::from_u8(p[ETH_HDR_TYPE]).ok_or(DecodeError::BadType)?;
    let length = u16::from_le_bytes([p[ETH_HDR_LENGTH], p[ETH_HDR_LENGTH + 1]]);
    let l = length as usize;
    if l > p.len() - ETH_HDR_SIZE || l < ftype.min_body() {
        return Err(DecodeError::BadLength);
    }
    let h = Header { version: VERSION, ftype, length, seq: get32(p, ETH_HDR_SEQ), ts: get64(p, ETH_HDR_TS) };
    let b = &p[ETH_HDR_SIZE..ETH_HDR_SIZE + l];
    let body = match ftype {
        FrameType::Ping => Body::Ping { pc_ns: get64(b, ETH_PING_PC_NS) },
        FrameType::Pong => Body::Pong {
            ping_seq: get32(b, ETH_PONG_PING_SEQ),
            pc_ns: get64(b, ETH_PONG_PC_NS),
            rx_iep: get64(b, ETH_PONG_RX_IEP),
        },
        FrameType::TputData => Body::TputData,
        FrameType::TputReport => Body::TputReport {
            frames: get32(b, ETH_TR_FRAMES),
            seq_gaps: get32(b, ETH_TR_SEQ_GAPS),
            bytes: get64(b, ETH_TR_BYTES),
            first_iep: get64(b, ETH_TR_FIRST_IEP),
            last_iep: get64(b, ETH_TR_LAST_IEP),
        },
        FrameType::AudioIn => {
            let mut pcm = Box::new([[0i16; 8]; 64]);
            for (i, x) in pcm.iter_mut().flatten().enumerate() {
                let o = ETH_AI_PCM + 2 * i;
                *x = i16::from_le_bytes([b[o], b[o + 1]]);
            }
            Body::AudioIn { first_index: get32(b, ETH_AI_FIRST_INDEX), pcm }
        }
        FrameType::AudioOut => {
            let mut pcm = Box::new([[0i32; 8]; 32]);
            for (i, x) in pcm.iter_mut().flatten().enumerate() {
                *x = get32(b, ETH_AO_PCM + 4 * i) as i32;
            }
            Body::AudioOut { status: get_status(&b[ETH_AO_STATUS..]), pcm }
        }
        FrameType::Control => Body::Control {
            cmd: get32(b, ETH_CTL_CMD),
            arg0: get32(b, ETH_CTL_ARG0),
            arg1: get32(b, ETH_CTL_ARG1),
        },
    };
    let src: [u8; 6] = frame[6..12].try_into().unwrap();
    Ok((h, body, src))
}

#[cfg(test)]
mod tests {
    use super::*;

    const PC: [u8; 6] = [0x02, 0x11, 0x22, 0x33, 0x44, 0x55];

    fn hdr(ftype: FrameType, seq: u32, ts: u64) -> Header {
        Header { version: VERSION, ftype, length: 0, seq, ts }
    }

    fn status() -> Status {
        Status {
            first_index: 64, half_seq: 2, rtu1_loops: 3, rtu1_frame_pos: 5, clip_frames: 16000,
            underrun_count: 6, sd_ovf_count: 7, batch_count: 8, rx_frames: 9, rx_audio: 10,
            rx_foreign: 11, rx_bad: 12, last_in_end: 128, tx_late: 13, detect_iep: 0x1122_3344_5566_7788,
        }
    }

    fn bodies() -> Vec<(FrameType, Body)> {
        let mut pin = Box::new([[0i16; 8]; 64]);
        let mut pout = Box::new([[0i32; 8]; 32]);
        for f in 0..64 { for c in 0..8 { pin[f][c] = (f * 8 + c) as i16 - 300; } }
        for f in 0..32 { for c in 0..8 { pout[f][c] = -((f * 8 + c) as i32) * 70_000; } }
        vec![
            (FrameType::Ping, Body::Ping { pc_ns: 123_456_789 }),
            (FrameType::Pong, Body::Pong { ping_seq: 77, pc_ns: 5, rx_iep: 6 }),
            (FrameType::TputData, Body::TputData),
            (FrameType::TputReport, Body::TputReport { frames: 1, seq_gaps: 2, bytes: 3, first_iep: 4, last_iep: 5 }),
            (FrameType::AudioIn, Body::AudioIn { first_index: 6400, pcm: pin }),
            (FrameType::AudioOut, Body::AudioOut { status: status(), pcm: pout }),
            (FrameType::Control, Body::Control { cmd: CMD_TPUT_TX, arg0: 1000, arg1: 1500 }),
        ]
    }

    #[test]
    fn roundtrip_every_type() {
        for (t, b) in bodies() {
            let f = encode(BOARD_MAC, PC, &hdr(t, 42, 99), &b, 0);
            let (h, b2, src) = decode(&f).expect("decode");
            assert_eq!(h.ftype, t);
            assert_eq!(h.seq, 42);
            assert_eq!(h.ts, 99);
            assert_eq!(src, PC);
            assert_eq!(b2, b, "{t:?}");
        }
    }

    #[test]
    fn audio_out_frame_is_1118_bytes() {
        let (t, b) = bodies().remove(5);
        assert_eq!(encode(PC, BOARD_MAC, &hdr(t, 0, 0), &b, 0).len(), 14 + 16 + 64 + 1024);
    }

    #[test]
    fn audio_in_frame_is_1062_bytes() {
        let (t, b) = bodies().remove(4);
        assert_eq!(encode(BOARD_MAC, PC, &hdr(t, 0, 0), &b, 0).len(), 14 + 16 + 8 + 1024);
    }

    #[test]
    fn small_frames_pad_to_requested_size() {
        let f = encode(BOARD_MAC, PC, &hdr(FrameType::Ping, 0, 0), &Body::Ping { pc_ns: 1 }, 1514);
        assert_eq!(f.len(), 1514);
        let (h, b, _) = decode(&f).unwrap();
        assert_eq!(h.length as usize, 1514 - 14 - 16);
        assert_eq!(b, Body::Ping { pc_ns: 1 });
    }

    #[test]
    fn decode_rejects_foreign_ethertype() {
        let mut f = encode(BOARD_MAC, PC, &hdr(FrameType::Ping, 0, 0), &Body::Ping { pc_ns: 1 }, 0);
        f[12] = 0x86;
        f[13] = 0xDD;
        assert_eq!(decode(&f).unwrap_err(), DecodeError::ForeignEtherType);
    }

    #[test]
    fn decode_rejects_short_and_bad_length() {
        let f = encode(BOARD_MAC, PC, &hdr(FrameType::Pong, 0, 0), &Body::Pong { ping_seq: 1, pc_ns: 2, rx_iep: 3 }, 0);
        assert_eq!(decode(&f[..20]).unwrap_err(), DecodeError::Short);
        let mut g = f.clone();
        g[14 + 2] = 0xFF; // length far beyond the frame
        assert_eq!(decode(&g).unwrap_err(), DecodeError::BadLength);
        let mut v = f.clone();
        v[14] = 9;
        assert_eq!(decode(&v).unwrap_err(), DecodeError::BadVersion);
        let mut t = f;
        t[15] = 200;
        assert_eq!(decode(&t).unwrap_err(), DecodeError::BadType);
    }

    #[test]
    fn ethertype_is_big_endian_fields_little_endian() {
        let f = encode(BOARD_MAC, PC, &hdr(FrameType::Ping, 0x0102_0304, 0), &Body::Ping { pc_ns: 0 }, 0);
        assert_eq!(&f[12..14], &[0x88, 0xB5]);
        assert_eq!(&f[14 + 4..14 + 8], &[0x04, 0x03, 0x02, 0x01]);
    }

    #[test]
    fn status_rtu1_total() {
        assert_eq!(status().rtu1_total(), 48005);
    }
}
