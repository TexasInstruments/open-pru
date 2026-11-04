// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Processing seam for the later GPU projects (design 7): aligned 64-frame
//! blocks of input and output, to an in-process [`Stage`] or exported to
//! another process through a shared-memory ring.
//!
//! Shared-memory layout (`/dev/shm/<name>`, little endian), for a Python or
//! CUDA reader that maps the file:
//!
//! | Offset | Size | Field |
//! |-------:|-----:|-------|
//! | 0 | 8 | magic `PDMSHM01` |
//! | 8 | 4 | u32 block count N |
//! | 12 | 4 | u32 block size B = 3080 |
//! | 16 | 8 | u64 blocks written W (updated after each block) |
//! | 24 | 40 | zero |
//! | 64 + (w mod N) x B | B | block number w |
//!
//! Block: u64 board index of its first frame, then 64 x 8 x i16 input
//! (frame-interleaved), then 64 x 8 x i32 output of the same samples (the
//! output is taken `lag` frames earlier in PCM index, see
//! [`crate::latency::LatencyMeter::lag`]). The newest block is
//! `(W - 1) mod N`; a reader copies a block and checks W did not move past
//! it by N meanwhile.

use std::collections::BTreeMap;
use std::fs::File;
use std::io;
use std::os::unix::fs::FileExt;

/// One aligned block: board index of the first frame, 64 input frames,
/// the 64 output frames of the same samples.
pub type Block = (u64, Vec<[i16; 8]>, Vec<[i32; 8]>);

/// A processing stage, called with aligned 64-frame blocks.
pub trait Stage: Send {
    fn name(&self) -> &str;
    /// Result frames (any length) to show, or `None`.
    fn process(&mut self, index: u64, input: &[[i16; 8]], output: &[[i32; 8]]) -> Option<Vec<[f32; 8]>>;
}

/// Passes the output through (tests, and a template for real stages).
pub struct Identity;

impl Stage for Identity {
    fn name(&self) -> &str {
        "identity"
    }

    fn process(&mut self, _index: u64, _input: &[[i16; 8]], output: &[[i32; 8]]) -> Option<Vec<[f32; 8]>> {
        Some(output.iter().map(|f| f.map(|v| v as f32)).collect())
    }
}

/// Pairs input frames (board index) with output frames (PCM index k, which
/// carries input index k + lag) into 64-frame blocks.
#[derive(Default)]
pub struct Aligner {
    inp: BTreeMap<u64, [i16; 8]>,
    out: BTreeMap<u64, [i32; 8]>,
    lag: i64,
    next: Option<u64>,
}

impl Aligner {
    pub fn new() -> Self {
        Self::default()
    }

    /// Output index k carries input index k + lag. A change drops the
    /// output frames kept so far.
    pub fn set_lag(&mut self, lag: i64) {
        if lag != self.lag {
            self.out.clear();
            self.lag = lag;
        }
    }

    pub fn push_in(&mut self, frames: &[(u64, [i16; 8])]) {
        self.inp.extend(frames.iter().copied());
    }

    pub fn push_out(&mut self, frames: &[(u64, [i32; 8])]) {
        for (k, f) in frames {
            let i = *k as i64 + self.lag;
            if i >= 0 {
                self.out.insert(i as u64, *f);
            }
        }
    }

    /// Complete blocks, oldest first. A block that cannot complete while
    /// three later blocks could is skipped.
    pub fn take_blocks(&mut self) -> Vec<Block> {
        let mut v = Vec::new();
        while let (Some(&i_lo), Some(&o_lo)) = (self.inp.keys().next(), self.out.keys().next()) {
            let lo = i_lo.max(o_lo).div_ceil(64) * 64; // first block both may complete
            let start = self.next.map_or(lo, |n| n.max(lo));
            let complete = (start..start + 64).all(|i| self.inp.contains_key(&i) && self.out.contains_key(&i));
            if complete {
                let inp = (start..start + 64).map(|i| self.inp[&i]).collect();
                let out = (start..start + 64).map(|i| self.out[&i]).collect();
                v.push((start, inp, out));
                self.next = Some(start + 64);
            } else {
                let i_hi = *self.inp.keys().next_back().unwrap();
                let o_hi = *self.out.keys().next_back().unwrap();
                if i_hi.min(o_hi) >= start + 4 * 64 {
                    self.next = Some(start + 64); // skip a block with a gap
                } else {
                    self.next = Some(start);
                    break;
                }
            }
            let n = self.next.unwrap();
            self.inp = self.inp.split_off(&n);
            self.out = self.out.split_off(&n);
        }
        v
    }
}

/// Bytes per block in the shared-memory ring.
pub const BLOCK_BYTES: usize = 8 + 64 * 8 * 2 + 64 * 8 * 4;
const HEADER: u64 = 64;

/// Shared-memory ring `/dev/shm/<name>` (layout in the module docs). The
/// file is removed when the export is dropped.
pub struct ShmExport {
    f: File,
    path: std::path::PathBuf,
    blocks: u64,
    written: u64,
}

impl ShmExport {
    pub fn create(name: &str, blocks: usize) -> io::Result<Self> {
        let path = std::path::Path::new("/dev/shm").join(name);
        let f = File::options().read(true).write(true).create(true).truncate(true).open(&path)?;
        f.set_len(HEADER + (blocks * BLOCK_BYTES) as u64)?;
        let mut h = [0u8; HEADER as usize];
        h[0..8].copy_from_slice(b"PDMSHM01");
        h[8..12].copy_from_slice(&(blocks as u32).to_le_bytes());
        h[12..16].copy_from_slice(&(BLOCK_BYTES as u32).to_le_bytes());
        f.write_all_at(&h, 0)?;
        Ok(ShmExport { f, path, blocks: blocks as u64, written: 0 })
    }

    /// Append one aligned block.
    pub fn write(&mut self, index: u64, input: &[[i16; 8]], output: &[[i32; 8]]) -> io::Result<()> {
        let mut b = Vec::with_capacity(BLOCK_BYTES);
        b.extend_from_slice(&index.to_le_bytes());
        for f in input.iter().take(64) {
            for x in f {
                b.extend_from_slice(&x.to_le_bytes());
            }
        }
        for f in output.iter().take(64) {
            for x in f {
                b.extend_from_slice(&x.to_le_bytes());
            }
        }
        b.resize(BLOCK_BYTES, 0);
        let slot = self.written % self.blocks;
        self.f.write_all_at(&b, HEADER + slot * BLOCK_BYTES as u64)?;
        self.written += 1;
        self.f.write_all_at(&self.written.to_le_bytes(), 16)
    }
}

impl Drop for ShmExport {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.path);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn frames_in(i0: u64, n: usize) -> Vec<(u64, [i16; 8])> {
        (0..n as u64).map(|k| (i0 + k, [((i0 + k) % 1000) as i16; 8])).collect()
    }

    fn frames_out(k0: u64, n: usize, lag: i64) -> Vec<(u64, [i32; 8])> {
        // output index k carries input index k + lag
        (0..n as u64).map(|k| (k0 + k, [(((k0 + k) as i64 + lag) % 1000) as i32 * 256; 8])).collect()
    }

    #[test]
    fn identity_stage_sees_aligned_blocks() {
        let mut a = Aligner::new();
        a.set_lag(10);
        a.push_in(&frames_in(6400, 640));
        a.push_out(&frames_out(6390 - 100, 900, 10));
        let blocks = a.take_blocks();
        assert!(blocks.len() >= 9, "{}", blocks.len());
        let mut st = Identity;
        for (index, inp, out) in &blocks {
            assert_eq!(index % 64, 0);
            for (fi, fo) in inp.iter().zip(out.iter()) {
                assert_eq!(fi[0] as i32 * 256, fo[0], "block {index}");
            }
            let r = st.process(*index, inp, out).unwrap();
            assert_eq!(r[5][3], out[5][3] as f32);
        }
        assert_eq!(st.name(), "identity");
    }

    #[test]
    fn shm_ring_wraps_and_counts() {
        let name = format!("pdm_eth_test_{}", std::process::id());
        let mut x = ShmExport::create(&name, 4).unwrap();
        let inp = [[1i16; 8]; 64];
        let out = [[2i32; 8]; 64];
        for w in 0..7u64 {
            x.write(w * 64, &inp, &out).unwrap();
        }
        let b = std::fs::read(format!("/dev/shm/{name}")).unwrap();
        assert_eq!(&b[0..8], b"PDMSHM01");
        assert_eq!(u32::from_le_bytes(b[8..12].try_into().unwrap()), 4);
        assert_eq!(u32::from_le_bytes(b[12..16].try_into().unwrap()), BLOCK_BYTES as u32);
        assert_eq!(u64::from_le_bytes(b[16..24].try_into().unwrap()), 7);
        assert_eq!(b.len(), 64 + 4 * BLOCK_BYTES);
        // block 6 went to slot 2, block 4 to slot 0
        let slot = |s: usize| u64::from_le_bytes(b[64 + s * BLOCK_BYTES..72 + s * BLOCK_BYTES].try_into().unwrap());
        assert_eq!((slot(0), slot(2), slot(3)), (4 * 64, 6 * 64, 3 * 64));
        drop(x);
        assert!(!std::path::Path::new(&format!("/dev/shm/{name}")).exists());
    }
}
