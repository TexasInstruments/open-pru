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

/// One scene as a one-pass playlist: a Scene Simulator zip (unpacked to a
/// temp folder) or a folder with `mic1..8.wav`, directly or under `mics/`
/// (the zip's layout). 8 mics of 16 kHz, or an error naming the scene.
pub fn load_scene(path: &Path) -> Result<Playlist, WavError> {
    let stem = |p: &Path| p.file_stem().map_or_else(|| p.display().to_string(), |n| n.to_string_lossy().into_owned());
    let name = stem(path);
    let dir = if path.is_file() {
        let d = std::env::temp_dir().join(format!("pdm-eth-scene-{}", std::process::id())).join(&name);
        let _ = std::fs::remove_dir_all(&d);
        crate::scenezip::unpack(path, &d)?;
        d
    } else {
        path.to_path_buf()
    };
    let mics = if dir.join("mic1.wav").is_file() { dir.clone() } else { dir.join("mics") };
    let paths: Vec<_> = (1..=8).map(|m| mics.join(format!("mic{m}.wav"))).collect();
    if let Some(p) = paths.iter().find(|p| !p.is_file()) {
        return Err(WavError(format!("scene {name}: {} missing", p.display())));
    }
    for p in &paths {
        let r = hound::WavReader::open(p).map_err(|e| WavError(format!("scene {name}: {}: {e}", p.display())))?;
        if r.spec().sample_rate != 16000 {
            return Err(WavError(format!("scene {name}: {} is {} Hz, not 16 kHz", p.display(), r.spec().sample_rate)));
        }
    }
    let cs = load_set(&paths).map_err(|e| WavError(format!("scene {name}: {}", e.0)))?;
    let mut set = InputSet::default();
    for (c, mut x) in cs.ch.into_iter().enumerate() {
        x.resize(cs.frames + GAP_FRAMES, 0);
        set.ch[c] = x;
    }
    let scale = if cs.gain > 0.0 { 1.0 / (256.0 * cs.gain) } else { 0.0 };
    let total = set.ch[0].len();
    Ok(Playlist { set, passes: vec![Pass { scene: name, offset: 0, frames: cs.frames, scale }], total })
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

    /// A zip with local file headers only (what `scenezip::unpack` reads); `method` 0 = stored.
    fn zip(path: &Path, files: &[(String, Vec<u8>)], method: u16) {
        let mut z = Vec::new();
        for (name, data) in files {
            z.extend_from_slice(b"PK\x03\x04");
            z.extend_from_slice(&20u16.to_le_bytes()); // version
            z.extend_from_slice(&0u16.to_le_bytes()); // flags
            z.extend_from_slice(&method.to_le_bytes());
            z.extend_from_slice(&[0; 8]); // time, date, crc (not checked)
            z.extend_from_slice(&(data.len() as u32).to_le_bytes());
            z.extend_from_slice(&(data.len() as u32).to_le_bytes());
            z.extend_from_slice(&(name.len() as u16).to_le_bytes());
            z.extend_from_slice(&0u16.to_le_bytes());
            z.extend_from_slice(name.as_bytes());
            z.extend_from_slice(data);
        }
        z.extend_from_slice(b"PK\x05\x06"); // end of central directory (rest ignored)
        z.extend_from_slice(&[0; 18]);
        std::fs::write(path, z).unwrap();
    }

    /// The 8 mic files of a scene made by `scene`, as zip entries under `prefix`.
    fn entries(dir: &Path, prefix: &str) -> Vec<(String, Vec<u8>)> {
        let mut v: Vec<_> = (1..=8).map(|m| (format!("{prefix}mic{m}.wav"), std::fs::read(dir.join(format!("mic{m}.wav"))).unwrap())).collect();
        v.push(("settings.json".into(), b"{}".to_vec()));
        v
    }

    #[test]
    fn scene_folder_flat_and_mics_layout() {
        let d = tempfile::tempdir().unwrap();
        scene(d.path(), "flat", 8, 1000, |_, i| (i % 50) as i16 * 100);
        scene(&d.path().join("nested"), "mics", 8, 1000, |_, i| (i % 50) as i16 * 100);
        for (p, name) in [(d.path().join("flat"), "flat"), (d.path().join("nested"), "nested")] {
            let pl = load_scene(&p).unwrap();
            assert_eq!(pl.passes.len(), 1);
            assert_eq!(pl.passes[0].scene, name);
            assert_eq!(pl.total, 1000 + GAP_FRAMES);
        }
    }

    #[test]
    fn scene_zip_loads() {
        let d = tempfile::tempdir().unwrap();
        scene(d.path(), "src", 8, 1000, |_, i| (i % 50) as i16 * 100);
        let z = d.path().join("scene_simulator_factory_booth.zip");
        zip(&z, &entries(&d.path().join("src"), "mics/"), 0);
        let pl = load_scene(&z).unwrap();
        assert_eq!(pl.passes[0].scene, "scene_simulator_factory_booth");
        assert_eq!(pl.passes[0].frames, 1000);
    }

    #[test]
    fn scene_compressed_zip_is_refused() {
        let d = tempfile::tempdir().unwrap();
        scene(d.path(), "src", 8, 100, |_, _| 100);
        let z = d.path().join("c.zip");
        zip(&z, &entries(&d.path().join("src"), "mics/"), 8);
        let e = load_scene(&z).err().expect("compressed");
        assert!(e.0.contains("compressed"), "{}", e.0);
    }

    #[test]
    fn scene_with_7_mics_is_refused() {
        let d = tempfile::tempdir().unwrap();
        scene(d.path(), "seven", 7, 100, |_, _| 100);
        let e = load_scene(&d.path().join("seven")).err().expect("7 mics");
        assert!(e.0.contains("mic8"), "{}", e.0);
    }

    #[test]
    fn scene_22k_is_refused() {
        let d = tempfile::tempdir().unwrap();
        let s = d.path().join("fast");
        std::fs::create_dir_all(&s).unwrap();
        let spec = hound::WavSpec { channels: 1, sample_rate: 22050, bits_per_sample: 16, sample_format: hound::SampleFormat::Int };
        for m in 1..=8 {
            let mut w = hound::WavWriter::create(s.join(format!("mic{m}.wav")), spec).unwrap();
            for _ in 0..100 {
                w.write_sample(100i16).unwrap();
            }
            w.finalize().unwrap();
        }
        let e = load_scene(&s).err().expect("22.05 kHz");
        assert!(e.0.contains("16 kHz"), "{}", e.0);
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
