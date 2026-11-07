// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Booth playlist (booth transcripts design 4): every subfolder of a folder
//! with `mic1.wav` .. `mic8.wav` is a scene; the scenes, each followed by
//! 1 s of silence, are played as one long capture set, looped.

use crate::wavin::{load_set, InputSet, WavError};
use std::path::Path;

/// Silence after every pass: 1.0 s.
pub const GAP_FRAMES: usize = 16000;

/// One scene in the loop.
#[derive(Clone, Debug)]
pub struct Pass {
    /// folder name
    pub scene: String,
    /// first frame of the scene in the loop
    pub offset: usize,
    /// scene length (without the gap)
    pub frames: usize,
    /// board output (24 bit) x scale = the scene's file level: 1 / (256 x gain)
    pub scale: f32,
}

pub struct Playlist {
    /// all scenes and gaps, every channel `total` frames, played from frame 0
    pub set: InputSet,
    pub passes: Vec<Pass>,
    pub total: usize,
}

/// Loads every subfolder of `dir` that holds `mic1.wav` as a scene (all of
/// `mic1.wav` .. `mic8.wav` needed), in name order.
pub fn load_playlist(dir: &Path) -> Result<Playlist, WavError> {
    let rd = std::fs::read_dir(dir).map_err(|e| WavError(format!("{}: {e}", dir.display())))?;
    let mut scenes: Vec<_> = rd.filter_map(|e| e.ok()).map(|e| e.path()).filter(|p| p.join("mic1.wav").is_file()).collect();
    scenes.sort();
    if scenes.is_empty() {
        return Err(WavError(format!("{}: no scene folders (subfolders with mic1.wav .. mic8.wav)", dir.display())));
    }
    let mut set = InputSet::default();
    let mut passes = Vec::new();
    for sd in &scenes {
        let name = sd.file_name().unwrap().to_string_lossy().into_owned();
        let paths: Vec<_> = (1..=8).map(|m| sd.join(format!("mic{m}.wav"))).collect();
        if let Some(p) = paths.iter().find(|p| !p.is_file()) {
            return Err(WavError(format!("scene {name}: {} missing", p.display())));
        }
        let cs = load_set(&paths).map_err(|e| WavError(format!("scene {name}: {}", e.0)))?;
        let offset = set.ch[0].len();
        for (c, x) in cs.ch.into_iter().enumerate() {
            set.ch[c].extend_from_slice(&x);
            set.ch[c].resize(offset + cs.frames + GAP_FRAMES, 0);
        }
        let scale = if cs.gain > 0.0 { 1.0 / (256.0 * cs.gain) } else { 0.0 };
        passes.push(Pass { scene: name, offset, frames: cs.frames, scale });
    }
    let total = set.ch[0].len();
    Ok(Playlist { set, passes, total })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::Path;

    /// A scene folder with 8 mono 16 kHz int16 files; mic m (1-based) holds `f(m, i)`.
    fn scene(dir: &Path, name: &str, mics: usize, frames: usize, f: impl Fn(usize, usize) -> i16) {
        let d = dir.join(name);
        std::fs::create_dir_all(&d).unwrap();
        let spec = hound::WavSpec { channels: 1, sample_rate: 16000, bits_per_sample: 16, sample_format: hound::SampleFormat::Int };
        for m in 1..=mics {
            let mut w = hound::WavWriter::create(d.join(format!("mic{m}.wav")), spec).unwrap();
            for i in 0..frames {
                w.write_sample(f(m, i)).unwrap();
            }
            w.finalize().unwrap();
        }
    }

    #[test]
    fn playlist_concatenates_with_gaps() {
        let d = tempfile::tempdir().unwrap();
        scene(d.path(), "a", 8, 1000, |_, i| 1000 + (i % 7) as i16);
        scene(d.path(), "b", 8, 2000, |_, _| -500);
        let p = load_playlist(d.path()).unwrap();
        assert_eq!(p.total, 1000 + 16000 + 2000 + 16000);
        assert_eq!(p.passes.len(), 2);
        assert_eq!((p.passes[0].scene.as_str(), p.passes[0].offset, p.passes[0].frames), ("a", 0, 1000));
        assert_eq!((p.passes[1].scene.as_str(), p.passes[1].offset, p.passes[1].frames), ("b", 17000, 2000));
        for c in 0..8 {
            assert_eq!(p.set.ch[c].len(), p.total);
            assert!(p.set.ch[c][1000..17000].iter().all(|&v| v == 0));
            assert!(p.set.ch[c][19000..].iter().all(|&v| v == 0));
            assert_ne!(p.set.ch[c][17000], 0);
        }
        assert_eq!(p.set.start, [0; 8]);
    }

    #[test]
    fn playlist_keeps_level_differences() {
        let d = tempfile::tempdir().unwrap();
        scene(d.path(), "a", 8, 1000, |m, i| {
            let v = (8000.0 * (i as f32 * 0.05).sin()) as i16;
            if m == 2 { v / 2 } else { v }
        });
        let p = load_playlist(d.path()).unwrap();
        for i in 0..1000 {
            let (a, b) = (p.set.ch[0][i] as i32, p.set.ch[1][i] as i32);
            assert!(((b as f32) - a as f32 / 2.0).abs() <= 1.5, "frame {i}: {a} vs {b}"); // source halves truncate, x gain ~2
        }
    }

    #[test]
    fn playlist_scale_maps_back_to_file_level() {
        let d = tempfile::tempdir().unwrap();
        scene(d.path(), "a", 8, 1000, |_, i| if i == 10 { 8192 } else { 0 }); // peak 0.25
        let p = load_playlist(d.path()).unwrap();
        let want = 1.0 / (256.0 * 16384.0 / 0.25);
        assert!((p.passes[0].scale - want).abs() < want * 1e-5, "{} vs {want}", p.passes[0].scale);
    }

    #[test]
    fn empty_or_bad_playlist_is_an_error() {
        let d = tempfile::tempdir().unwrap();
        let e = load_playlist(d.path()).err().expect("empty folder");
        assert!(e.0.contains(&d.path().display().to_string()), "{}", e.0);
        scene(d.path(), "seven", 7, 1000, |_, _| 100);
        let e = load_playlist(d.path()).err().expect("7 mics");
        assert!(e.0.contains("seven"), "{}", e.0);
    }
}
