// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Audio sources of the GUI's per-channel selector (design 7.2a): the booth
//! clip, the example files of `audio/` (compiled in), silence, or a file.

use crate::wavin::WavError;

const SPEECH: &[u8] = include_bytes!("../audio/speech_librivox_30s.wav");
const MUSIC: &[u8] = include_bytes!("../audio/music_chopin_nocturne_30s.wav");
const SPEECH_MUSIC: &[u8] = include_bytes!("../audio/speech_music_30s.wav");

/// What one channel plays.
#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Source {
    /// The channel's clip of `mcuplus/pdm_clips.h`.
    Booth,
    Speech,
    Music,
    SpeechMusic,
    Silence,
    /// A WAV file (its name, for the label).
    File(String),
    /// One file of a capture set (design 7.2a).
    SetFile(String),
}

/// The built-in entries of the selector, in menu order.
pub const BUILTIN: [Source; 5] = [Source::Booth, Source::Speech, Source::Music, Source::SpeechMusic, Source::Silence];

impl Source {
    /// Text in the selector and the IN pane.
    pub fn label(&self) -> String {
        match self {
            Source::Booth => "booth clip".into(),
            Source::Speech => "speech".into(),
            Source::Music => "music".into(),
            Source::SpeechMusic => "speech + music".into(),
            Source::Silence => "silence".into(),
            Source::File(n) => n.clone(),
            Source::SetFile(n) => format!("{n} (set)"),
        }
    }

    /// Samples of a built-in source for channel `ch` (16 kHz, peak 0.5 FS);
    /// `File` has none here (the UI loads it from its path).
    pub fn samples(&self, ch: usize) -> Result<Vec<i16>, WavError> {
        use crate::wavin::load_channel_bytes;
        match self {
            Source::Booth => Ok(crate::wavin::booth_clips()[ch.min(7)].clone()),
            Source::Speech => load_channel_bytes("speech_librivox_30s.wav", SPEECH),
            Source::Music => load_channel_bytes("music_chopin_nocturne_30s.wav", MUSIC),
            Source::SpeechMusic => load_channel_bytes("speech_music_30s.wav", SPEECH_MUSIC),
            Source::Silence => Ok(vec![0; 16384]),
            Source::File(n) | Source::SetFile(n) => Err(WavError(format!("{n}: a file source is loaded from its path"))),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn example_sources_are_30_s_at_half_scale() {
        for s in [Source::Speech, Source::Music, Source::SpeechMusic] {
            let x = s.samples(0).unwrap();
            assert_eq!(x.len(), 480_000, "{s:?}");
            assert_eq!(x.iter().map(|v| v.unsigned_abs()).max(), Some(16384), "{s:?}");
        }
    }

    #[test]
    fn booth_source_is_the_channel_clip_and_silence_is_zero() {
        assert_eq!(&Source::Booth.samples(3).unwrap(), &crate::wavin::booth_clips()[3]);
        let z = Source::Silence.samples(5).unwrap();
        assert!(!z.is_empty() && z.iter().all(|v| *v == 0));
    }

    #[test]
    fn set_files_are_marked_in_their_label() {
        assert_eq!(Source::SetFile("mic3.wav".into()).label(), "mic3.wav (set)");
    }

    #[test]
    fn labels_name_the_source() {
        assert_eq!(Source::Booth.label(), "booth clip");
        assert_eq!(Source::SpeechMusic.label(), "speech + music");
        assert_eq!(Source::File("song.wav".into()).label(), "song.wav");
        assert!(Source::File("x".into()).samples(0).is_err());
    }
}
