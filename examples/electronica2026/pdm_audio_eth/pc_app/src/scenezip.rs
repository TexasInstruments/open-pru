// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
//! Unpacks a Scene Simulator zip: its `makeZip` stores entries without
//! compression and without data descriptors, so the local file headers are
//! enough (no zip library in pc_app).

use crate::wavin::WavError;
use std::path::Path;

/// Writes every entry of the stored zip `zip` below `into` (folders kept).
pub fn unpack(zip: &Path, into: &Path) -> Result<(), WavError> {
    let err = |m: String| WavError(format!("{}: {m}", zip.display()));
    let b = std::fs::read(zip).map_err(|e| err(e.to_string()))?;
    let u16at = |i: usize| u16::from_le_bytes([b[i], b[i + 1]]) as usize;
    let u32at = |i: usize| u32::from_le_bytes([b[i], b[i + 1], b[i + 2], b[i + 3]]) as usize;
    let mut i = 0;
    let mut n = 0;
    while i + 30 <= b.len() && b[i..i + 4] == *b"PK\x03\x04" {
        let (flags, method, size, name_len, extra_len) = (u16at(i + 6), u16at(i + 8), u32at(i + 22), u16at(i + 26), u16at(i + 28));
        let name_at = i + 30;
        let data_at = name_at + name_len + extra_len;
        if data_at + size > b.len() {
            return Err(err("truncated zip".into()));
        }
        let name = String::from_utf8_lossy(&b[name_at..name_at + name_len]).into_owned();
        if method != 0 {
            return Err(err(format!("compressed zip entry {name}; unpack the zip and open the folder")));
        }
        if flags & 8 != 0 {
            return Err(err(format!("zip entry {name} uses a data descriptor; unpack the zip and open the folder")));
        }
        if name.split('/').any(|p| p == "..") || name.starts_with('/') {
            return Err(err(format!("zip entry {name} points outside the scene")));
        }
        let out = into.join(&name);
        if name.ends_with('/') {
            std::fs::create_dir_all(&out).map_err(|e| err(e.to_string()))?;
        } else {
            if let Some(p) = out.parent() {
                std::fs::create_dir_all(p).map_err(|e| err(e.to_string()))?;
            }
            std::fs::write(&out, &b[data_at..data_at + size]).map_err(|e| err(e.to_string()))?;
        }
        n += 1;
        i = data_at + size;
    }
    if n == 0 {
        return Err(err("not a zip".into()));
    }
    Ok(())
}
