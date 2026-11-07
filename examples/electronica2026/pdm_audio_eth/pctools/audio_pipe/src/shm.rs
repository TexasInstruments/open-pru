// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Reader for the `PDMSHM01` ring that `pdm-eth gui --export-shm NAME`
//! writes to `/dev/shm/NAME` (layout: `pc_app/src/seam.rs`).

use std::fs::File;
use std::io;
use std::os::unix::fs::{FileExt, MetadataExt};
use std::path::{Path, PathBuf};

/// Bytes per block: u64 board index, 64 x 8 i16 input, 64 x 8 i32 output.
pub const BLOCK_BYTES: usize = 8 + 64 * 8 * 2 + 64 * 8 * 4;
const HEADER: u64 = 64;
const OUT_OFF: usize = 8 + 64 * 8 * 2;

/// One block of the board's output: board index of its first frame, 64 frames x 8 channels.
#[derive(Clone, Debug)]
pub struct Block {
    pub index: u64,
    pub out: [[i32; 8]; 64],
}

#[derive(Debug)]
pub enum Read {
    Block(Block),
    /// nothing new yet
    Empty,
    /// the writer lapped the reader; `skipped` blocks are lost, reading resumes at the oldest valid block
    Overrun { skipped: u64 },
    /// the writer restarted (new file or W went back); reading resumes at its newest block
    Reattached,
}

pub struct ShmReader {
    path: PathBuf,
    f: File,
    ino: u64,
    n: u64,
    next: u64,
    overruns: u64,
    buf: Vec<u8>,
}

fn header(f: &File, path: &Path) -> io::Result<(u64, u64)> {
    let mut h = [0u8; 24];
    f.read_exact_at(&mut h, 0)?;
    if &h[0..8] != b"PDMSHM01" {
        return Err(io::Error::new(io::ErrorKind::InvalidData, format!("{}: not a PDMSHM01 ring", path.display())));
    }
    let n = u32::from_le_bytes(h[8..12].try_into().unwrap()) as u64;
    let b = u32::from_le_bytes(h[12..16].try_into().unwrap()) as usize;
    if n == 0 || b != BLOCK_BYTES {
        return Err(io::Error::new(io::ErrorKind::InvalidData, format!("{}: PDMSHM01 ring with N {n}, block size {b} (want {BLOCK_BYTES})", path.display())));
    }
    Ok((n, u64::from_le_bytes(h[16..24].try_into().unwrap())))
}

impl ShmReader {
    /// Opens the ring; only blocks written after this call are read.
    pub fn open(path: &Path) -> io::Result<Self> {
        let f = File::open(path)?;
        let (n, w) = header(&f, path)?;
        let ino = f.metadata()?.ino();
        Ok(ShmReader { path: path.to_path_buf(), f, ino, n, next: w, overruns: 0, buf: vec![0; BLOCK_BYTES] })
    }

    fn written(&self) -> io::Result<u64> {
        let mut w = [0u8; 8];
        self.f.read_exact_at(&mut w, 16)?;
        Ok(u64::from_le_bytes(w))
    }

    /// A new file at the path (writer restarted): reopen it.
    fn replaced(&self) -> bool {
        std::fs::metadata(&self.path).map(|m| m.ino() != self.ino).unwrap_or(false)
    }

    /// Never blocks.
    pub fn next(&mut self) -> io::Result<Read> {
        let mut w = self.written()?;
        if w < self.next || self.replaced() {
            let f = File::open(&self.path)?;
            let (n, w2) = header(&f, &self.path)?;
            self.ino = f.metadata()?.ino();
            self.f = f;
            self.n = n;
            self.next = w2.saturating_sub(1);
            return Ok(Read::Reattached);
        }
        if w == self.next {
            return Ok(Read::Empty);
        }
        if w - self.next >= self.n {
            // block w - n shares its slot with block w, which the writer may be writing now
            let oldest = w - self.n + 1;
            let skipped = oldest - self.next;
            self.next = oldest;
            self.overruns += 1;
            return Ok(Read::Overrun { skipped });
        }
        let slot = self.next % self.n;
        self.f.read_exact_at(&mut self.buf, HEADER + slot * BLOCK_BYTES as u64)?;
        w = self.written()?;
        if w >= self.next + self.n {
            // overwritten while copying
            let oldest = w - self.n + 1;
            let skipped = oldest - self.next;
            self.next = oldest;
            self.overruns += 1;
            return Ok(Read::Overrun { skipped });
        }
        let mut out = [[0i32; 8]; 64];
        for (fr, o) in out.iter_mut().enumerate() {
            for (c, v) in o.iter_mut().enumerate() {
                let i = OUT_OFF + (fr * 8 + c) * 4;
                *v = i32::from_le_bytes(self.buf[i..i + 4].try_into().unwrap());
            }
        }
        let index = u64::from_le_bytes(self.buf[0..8].try_into().unwrap());
        self.next += 1;
        Ok(Read::Block(Block { index, out }))
    }

    pub fn overruns(&self) -> u64 {
        self.overruns
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const N: u64 = 8;

    fn tmp(name: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("ap_shm_{}_{name}", std::process::id()));
        let _ = std::fs::remove_file(&d);
        d
    }

    /// A ring file in the seam layout with N blocks and W = 0.
    fn create(p: &std::path::Path, magic: &[u8; 8]) -> std::fs::File {
        let f = std::fs::File::options().read(true).write(true).create(true).truncate(true).open(p).unwrap();
        f.set_len(64 + N * BLOCK_BYTES as u64).unwrap();
        let mut h = [0u8; 64];
        h[0..8].copy_from_slice(magic);
        h[8..12].copy_from_slice(&(N as u32).to_le_bytes());
        h[12..16].copy_from_slice(&(BLOCK_BYTES as u32).to_le_bytes());
        f.write_all_at(&h, 0).unwrap();
        f
    }

    /// Writes blocks from..to (block w has board index 1000 + 64 w, output ch0 = w) and sets W = to.
    fn write(f: &std::fs::File, from: u64, to: u64) {
        for w in from..to {
            let mut b = vec![0u8; BLOCK_BYTES];
            b[0..8].copy_from_slice(&(1000 + 64 * w).to_le_bytes());
            let o = 8 + 64 * 8 * 2;
            for fr in 0..64 {
                b[o + fr * 32..o + fr * 32 + 4].copy_from_slice(&(w as i32).to_le_bytes());
            }
            f.write_all_at(&b, 64 + (w % N) * BLOCK_BYTES as u64).unwrap();
        }
        f.write_all_at(&to.to_le_bytes(), 16).unwrap();
    }

    fn block_no(r: Read) -> u64 {
        match r {
            Read::Block(b) => {
                assert_eq!(b.index, 1000 + 64 * b.out[0][0] as u64);
                b.out[63][0] as u64
            }
            other => panic!("expected a block, got {other:?}"),
        }
    }

    #[test]
    fn reads_blocks_in_order() {
        let p = tmp("order");
        let f = create(&p, b"PDMSHM01");
        write(&f, 0, 1);
        let mut r = ShmReader::open(&p).unwrap();
        write(&f, 1, 3);
        assert_eq!(block_no(r.next().unwrap()), 1);
        assert_eq!(block_no(r.next().unwrap()), 2);
        assert!(matches!(r.next().unwrap(), Read::Empty));
        let _ = std::fs::remove_file(&p);
    }

    #[test]
    fn overrun_jumps_to_oldest_valid() {
        let p = tmp("overrun");
        let f = create(&p, b"PDMSHM01");
        write(&f, 0, 1);
        let mut r = ShmReader::open(&p).unwrap();
        write(&f, 1, 20);
        assert!(matches!(r.next().unwrap(), Read::Overrun { skipped: 12 }));
        assert_eq!(block_no(r.next().unwrap()), 13);
        assert_eq!(r.overruns(), 1);
        let _ = std::fs::remove_file(&p);
    }

    #[test]
    fn writer_restart_reattaches() {
        let p = tmp("restart");
        let f = create(&p, b"PDMSHM01");
        write(&f, 0, 50);
        let mut r = ShmReader::open(&p).unwrap();
        drop(f);
        std::fs::remove_file(&p).unwrap();
        let g = create(&p, b"PDMSHM01");
        write(&g, 0, 2);
        assert!(matches!(r.next().unwrap(), Read::Reattached));
        assert_eq!(block_no(r.next().unwrap()), 1);
        assert!(matches!(r.next().unwrap(), Read::Empty));
        let _ = std::fs::remove_file(&p);
    }

    #[test]
    fn bad_magic_is_an_error() {
        let p = tmp("magic");
        let _f = create(&p, b"NOTSHM01");
        let e = ShmReader::open(&p).err().expect("must fail");
        assert!(e.to_string().contains("PDMSHM01"), "{e}");
        let _ = std::fs::remove_file(&p);
    }
}
