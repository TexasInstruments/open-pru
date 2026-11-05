// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! WAV input (design 7.1): one file per channel, converted on load to
//! 16 kHz mono int16 with a peak of 16384 (0.5 FS, like the booth clips).

use std::fmt;
use std::path::Path;

/// Output rate of the board's input ring.
pub const RATE: u32 = 16_000;
/// Peak after normalisation (0.5 FS).
pub const PEAK: f32 = 16384.0;

/// A WAV file that could not be used; the message names the file.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WavError(pub String);

impl fmt::Display for WavError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for WavError {}

fn err(path: &Path, what: impl fmt::Display) -> WavError {
    WavError(format!("{}: {what}", path.display()))
}

/// Load one WAV file as the input of one channel: any rate, channel count,
/// integer (8 to 32 bit) or float samples; mixed to mono, resampled to
/// 16 kHz, peak-normalised to 16384 (silence stays silence).
pub fn load_channel(path: &Path) -> Result<Vec<i16>, WavError> {
    let r = hound::WavReader::open(path).map_err(|e| err(path, e))?;
    convert(path, r)
}

/// [`load_channel`] of a WAV held in memory (the built-in sources); `name`
/// appears in errors.
pub fn load_channel_bytes(name: &str, bytes: &[u8]) -> Result<Vec<i16>, WavError> {
    let path = Path::new(name);
    let r = hound::WavReader::new(std::io::Cursor::new(bytes)).map_err(|e| err(path, e))?;
    convert(path, r)
}

fn convert<R: std::io::Read>(path: &Path, r: hound::WavReader<R>) -> Result<Vec<i16>, WavError> {
    let x = decode(path, r)?.0;
    Ok(scale(&x, peak_of(&x)))
}

/// Largest magnitude of `x`.
fn peak_of(x: &[f32]) -> f32 {
    x.iter().fold(0.0f32, |m, v| m.max(v.abs()))
}

/// `x` scaled so that `peak` maps to 16384 (0.5 FS); silence stays silence.
fn scale(x: &[f32], peak: f32) -> Vec<i16> {
    let g = if peak > 0.0 { PEAK / peak } else { 0.0 };
    x.iter().map(|v| (v * g).round().clamp(-32768.0, 32767.0) as i16).collect()
}

/// Mono samples at 16 kHz (mixed down, resampled), the file's channel count
/// and sample rate.
fn decode<R: std::io::Read>(path: &Path, r: hound::WavReader<R>) -> Result<(Vec<f32>, u16, u32), WavError> {
    let spec = r.spec();
    let chans = spec.channels.max(1) as usize;
    let raw: Vec<f32> = match spec.sample_format {
        hound::SampleFormat::Float => r.into_samples::<f32>().collect::<Result<_, _>>().map_err(|e| err(path, e))?,
        hound::SampleFormat::Int => {
            let scale = 1.0 / (1u64 << (spec.bits_per_sample - 1)) as f32;
            r.into_samples::<i32>()
                .map(|s| s.map(|v| v as f32 * scale))
                .collect::<Result<_, _>>()
                .map_err(|e| err(path, e))?
        }
    };
    if raw.len() < chans {
        return Err(err(path, "no samples"));
    }
    let mono: Vec<f32> = raw.chunks_exact(chans).map(|f| f.iter().sum::<f32>() / chans as f32).collect();
    let x = if spec.sample_rate == RATE { mono } else { resample(&mono, spec.sample_rate, RATE) };
    Ok((x, spec.channels, spec.sample_rate))
}

/// A synchronized multi-microphone capture (design 7.2a).
#[derive(Clone, Debug)]
pub struct CaptureSet {
    /// One channel per file, in the order given, all `frames` long.
    pub ch: Vec<Vec<i16>>,
    /// Common length after trimming (16 kHz frames).
    pub frames: usize,
    /// Frames cut from the end of each file to reach `frames`.
    pub trimmed: Vec<usize>,
}

/// Load up to 8 mono files of one sample rate as one set: trimmed to the
/// shortest from a common start, one gain (the set's loudest sample maps to
/// 0.5 FS, so the level differences between the channels are kept).
pub fn load_set(paths: &[std::path::PathBuf]) -> Result<CaptureSet, WavError> {
    if paths.is_empty() || paths.len() > 8 {
        return Err(WavError(format!("a capture set has 1 to 8 files, not {}", paths.len())));
    }
    let mut chans = Vec::with_capacity(paths.len());
    let mut rate0 = None;
    for p in paths {
        let r = hound::WavReader::open(p).map_err(|e| err(p, e))?;
        let (x, channels, rate) = decode(p, r)?;
        if channels != 1 {
            return Err(err(p, format!("{channels} channels; a capture set needs mono files")));
        }
        match rate0 {
            None => rate0 = Some((rate, p)),
            Some((r0, p0)) if r0 != rate => {
                return Err(err(p, format!("{rate} Hz, but {} is {r0} Hz; a capture set needs one sample rate",
                                          p0.display())));
            }
            Some(_) => {}
        }
        chans.push(x);
    }
    let frames = chans.iter().map(Vec::len).min().unwrap_or(0);
    let trimmed = chans.iter().map(|c| c.len() - frames).collect();
    let peak = chans.iter().map(|c| peak_of(&c[..frames])).fold(0.0f32, f32::max);
    let ch = chans.iter().map(|c| scale(&c[..frames], peak)).collect();
    Ok(CaptureSet { ch, frames, trimmed })
}

/// Windowed-sinc resampler: 64 taps, Hann window, cutoff at 0.9 of the
/// lower Nyquist frequency.
pub fn resample(x: &[f32], fin: u32, fout: u32) -> Vec<f32> {
    const HALF: i64 = 32;
    let ratio = fin as f64 / fout as f64;
    let fc = 0.45 * (fin.min(fout) as f64) / fin as f64; // cycles per input sample
    let n_out = (x.len() as f64 / ratio).floor() as usize;
    let mut y = Vec::with_capacity(n_out);
    for n in 0..n_out {
        let t = n as f64 * ratio;
        let k0 = t.floor() as i64;
        let (mut acc, mut wsum) = (0.0f64, 0.0f64);
        for k in (k0 - HALF + 1)..=(k0 + HALF) {
            let d = t - k as f64;
            let w = 0.5 + 0.5 * (std::f64::consts::PI * d / HALF as f64).cos();
            if !(-(HALF as f64)..=HALF as f64).contains(&d) {
                continue;
            }
            let a = 2.0 * fc * d;
            let sinc = if a.abs() < 1e-9 { 1.0 } else { (std::f64::consts::PI * a).sin() / (std::f64::consts::PI * a) };
            let h = 2.0 * fc * sinc * w;
            wsum += h;
            if (0..x.len() as i64).contains(&k) {
                acc += h * x[k as usize] as f64;
            }
        }
        y.push(if wsum != 0.0 { (acc / wsum) as f32 } else { 0.0 });
    }
    y
}

/// The eight channel inputs of one stream.
#[derive(Clone, Debug, Default)]
pub struct InputSet {
    pub ch: [Vec<i16>; 8],
    /// Sent-frame count at which each channel's source was set: the GUI
    /// plays it from its first sample from there (design 7.2a).
    pub start: [u64; 8],
}

impl InputSet {
    /// Length of the longest channel, frames.
    pub fn frames(&self) -> usize {
        self.ch.iter().map(Vec::len).max().unwrap_or(0)
    }

    /// Frame `i`; a channel past its end (or without a file) is silent.
    pub fn frame(&self, i: usize) -> [i16; 8] {
        std::array::from_fn(|c| self.ch[c].get(i).copied().unwrap_or(0))
    }

    /// Frame `i` with every channel looping on its own length (the GUI);
    /// a channel without samples is silent.
    pub fn frame_looped(&self, i: u64) -> [i16; 8] {
        std::array::from_fn(|c| {
            let x = &self.ch[c];
            if x.is_empty() { 0 } else { x[(i.saturating_sub(self.start[c]) % x.len() as u64) as usize] }
        })
    }

    /// New samples for channel `ch`, played from their first sample at
    /// sent-frame count `at`.
    pub fn set_channel(&mut self, ch: usize, x: Vec<i16>, at: u64) {
        if ch < 8 {
            self.ch[ch] = x;
            self.start[ch] = at;
        }
    }

    /// Give every channel without a file its booth clip (design 7.2a).
    pub fn fill_with_booth(&mut self) {
        let b = booth_clips();
        for (c, x) in self.ch.iter_mut().enumerate() {
            if x.is_empty() {
                *x = b[c].clone();
            }
        }
    }
}

/// The board's booth clips: `mcuplus/pdm_clips.h` (compiled in), 8 channels
/// x 16384 frames, what RTU1 plays without a PC stream.
pub fn booth_clips() -> &'static [Vec<i16>; 8] {
    static CLIPS: std::sync::OnceLock<[Vec<i16>; 8]> = std::sync::OnceLock::new();
    CLIPS.get_or_init(|| {
        let text = include_str!("../../pdm_audio_eth/mcuplus/pdm_clips.h");
        let body = &text[text.find("pdm_clip_buffer[").expect("pdm_clips.h: no array")..];
        let body = &body[body.find('{').expect("pdm_clips.h: no {") + 1..body.find("};").expect("pdm_clips.h: no };")];
        let v: Vec<i16> = body
            .split(|ch: char| ch == ',' || ch.is_whitespace())
            .filter(|t| !t.is_empty())
            .map(|t| t.parse().expect("pdm_clips.h: not a number"))
            .collect();
        std::array::from_fn(|c| v.iter().skip(c).step_by(8).copied().collect())
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn write(path: &Path, rate: u32, channels: u16, bits: u16, float: bool, f: impl Fn(usize, u16) -> f64, n: usize) {
        let spec = hound::WavSpec {
            channels,
            sample_rate: rate,
            bits_per_sample: bits,
            sample_format: if float { hound::SampleFormat::Float } else { hound::SampleFormat::Int },
        };
        let mut w = hound::WavWriter::create(path, spec).unwrap();
        let max = ((1i64 << (bits - 1)) - 1) as f64;
        for i in 0..n {
            for c in 0..channels {
                let v = f(i, c);
                if float {
                    w.write_sample(v as f32).unwrap();
                } else if bits == 8 {
                    w.write_sample((v * max) as i8).unwrap();
                } else {
                    w.write_sample((v * max) as i32).unwrap();
                }
            }
        }
        w.finalize().unwrap();
    }

    fn sine(rate: u32, hz: f64, amp: f64) -> impl Fn(usize, u16) -> f64 {
        move |i, _| amp * (2.0 * std::f64::consts::PI * hz * i as f64 / rate as f64).sin()
    }

    fn freq_by_zero_crossings(x: &[i16]) -> f64 {
        let z = x.windows(2).filter(|w| w[0] < 0 && w[1] >= 0).count();
        z as f64 * 16000.0 / x.len() as f64
    }

    #[test]
    fn wav_48k_stereo_float_to_16k_mono() {
        let d = tempfile::tempdir().unwrap();
        let p = d.path().join("a.wav");
        write(&p, 48000, 2, 32, true, sine(48000, 1000.0, 0.25), 48000);
        let x = load_channel(&p).unwrap();
        assert!((x.len() as i64 - 16000).abs() <= 2, "{}", x.len());
        let f = freq_by_zero_crossings(&x[100..x.len() - 100]);
        assert!((f - 1000.0).abs() < 5.0, "{f} Hz");
    }

    #[test]
    fn wav_24bit_and_8bit_load() {
        let d = tempfile::tempdir().unwrap();
        for bits in [8u16, 24] {
            let p = d.path().join(format!("b{bits}.wav"));
            write(&p, 16000, 1, bits, false, sine(16000, 500.0, 0.5), 8000);
            let x = load_channel(&p).unwrap();
            assert_eq!(x.len(), 8000);
            assert!((freq_by_zero_crossings(&x) - 500.0).abs() < 5.0);
        }
    }

    #[test]
    fn wav_peak_is_16384() {
        let d = tempfile::tempdir().unwrap();
        let p = d.path().join("c.wav");
        write(&p, 16000, 1, 16, false, sine(16000, 300.0, 0.03), 16000);
        let x = load_channel(&p).unwrap();
        assert_eq!(x.iter().map(|v| v.unsigned_abs()).max().unwrap(), 16384);
        let p = d.path().join("silent.wav");
        write(&p, 16000, 1, 16, false, |_, _| 0.0, 100);
        assert!(load_channel(&p).unwrap().iter().all(|v| *v == 0));
    }

    #[test]
    fn wav_short_file_pads_silence() {
        let mut set = InputSet::default();
        set.ch[2] = vec![5; 10];
        set.ch[5] = vec![7; 100];
        assert_eq!(set.frames(), 100);
        assert_eq!(set.frame(3), [0, 0, 5, 0, 0, 7, 0, 0]);
        assert_eq!(set.frame(50), [0, 0, 0, 0, 0, 7, 0, 0]);
        assert_eq!(set.frame(1000), [0; 8]);
    }

    #[test]
    fn wav_empty_or_missing_reports_file() {
        let d = tempfile::tempdir().unwrap();
        let missing = d.path().join("nope.wav");
        let e = load_channel(&missing).unwrap_err().to_string();
        assert!(e.contains("nope.wav"), "{e}");
        let p = d.path().join("empty.wav");
        write(&p, 16000, 1, 16, false, |_, _| 0.0, 0);
        let e = load_channel(&p).unwrap_err().to_string();
        assert!(e.contains("empty.wav") && e.contains("no samples"), "{e}");
        let junk = d.path().join("junk.wav");
        std::fs::write(&junk, b"not a wav").unwrap();
        let e = load_channel(&junk).unwrap_err().to_string();
        assert!(e.contains("junk.wav"), "{e}");
    }

    #[test]
    fn booth_clips_are_the_board_clip_buffer() {
        let b = booth_clips();
        for c in b.iter() {
            assert_eq!(c.len(), 16384);
        }
        // ch3 is the 1 kHz tone (16 samples per period), peak 0.5 FS
        assert!((0..1000).all(|i| b[3][i] == b[3][i + 16]));
        assert_eq!(b[3].iter().map(|v| v.unsigned_abs()).max(), Some(16384));
        // the header's last frame (the last 8 numbers of the initialiser)
        let last: [i16; 8] = std::array::from_fn(|c| b[c][16383]);
        assert_eq!(last, [312, 15544, -4674, -6270, -15136, -3196, -11585, -15136]);
    }

    #[test]
    fn frame_looped_wraps_each_channel_on_its_own_length() {
        let mut s = InputSet::default();
        s.ch[0] = vec![1, 2, 3];
        s.ch[1] = vec![10, 20, 30, 40, 50];
        let f = s.frame_looped(7);
        assert_eq!((f[0], f[1], f[2]), (2, 30, 0));
    }

    #[test]
    fn fill_with_booth_fills_only_channels_without_a_file() {
        let mut s = InputSet::default();
        s.ch[1] = vec![5; 100];
        s.fill_with_booth();
        assert_eq!(s.ch[1], vec![5; 100]);
        assert_eq!(s.ch[3], booth_clips()[3]);
        assert_eq!(s.ch[0].len(), 16384);
    }

    fn mono16(path: &Path, rate: u32, x: &[f64]) {
        let spec = hound::WavSpec { channels: 1, sample_rate: rate, bits_per_sample: 16,
                                    sample_format: hound::SampleFormat::Int };
        let mut w = hound::WavWriter::create(path, spec).unwrap();
        for v in x {
            w.write_sample((v * 32767.0) as i16).unwrap();
        }
    }

    #[test]
    fn capture_set_is_trimmed_to_the_shortest_with_one_gain() {
        let d = tempfile::tempdir().unwrap();
        let p: Vec<_> = (0..3).map(|k| d.path().join(format!("mic{k}.wav"))).collect();
        mono16(&p[0], RATE, &vec![0.8; 1010]);  // loudest
        mono16(&p[1], RATE, &vec![0.2; 1000]);  // shortest
        mono16(&p[2], RATE, &vec![-0.4; 1003]);
        let s = load_set(&p).unwrap();
        assert_eq!(s.frames, 1000);
        assert_eq!(s.trimmed, vec![10, 0, 3]);
        assert!(s.ch.iter().all(|c| c.len() == 1000));
        assert_eq!(s.ch[0][0], 16384, "loudest sample of the set = 0.5 FS");
        assert!((s.ch[1][0] - 4096).abs() <= 1, "{}", s.ch[1][0]); // 0.2 / 0.8 of it
        assert!((s.ch[2][0] + 8192).abs() <= 1, "{}", s.ch[2][0]);
    }

    #[test]
    fn capture_set_refuses_stereo_mixed_rates_and_more_than_8() {
        let d = tempfile::tempdir().unwrap();
        let a = d.path().join("a.wav");
        mono16(&a, RATE, &vec![0.1; 100]);
        let b = d.path().join("b.wav");
        mono16(&b, 48_000, &vec![0.1; 300]);
        let e = load_set(&[a.clone(), b.clone()]).unwrap_err().to_string();
        assert!(e.contains("b.wav") && e.contains("48000"), "{e}");
        let st = d.path().join("stereo.wav");
        write(&st, RATE, 2, 16, false, |_, _| 0.1, 100);
        let e = load_set(&[a.clone(), st]).unwrap_err().to_string();
        assert!(e.contains("stereo.wav") && e.contains("mono"), "{e}");
        let e = load_set(&vec![a; 9]).unwrap_err().to_string();
        assert!(e.contains("8"), "{e}");
    }

    #[test]
    fn frame_looped_counts_each_channel_from_its_start() {
        let mut s = InputSet::default();
        s.set_channel(0, vec![1, 2, 3], 0);
        s.set_channel(1, vec![10, 20, 30, 40, 50], 640);
        assert_eq!(s.frame_looped(640)[1], 10, "a new source starts at its first sample");
        assert_eq!(s.frame_looped(642)[1], 30);
        assert_eq!(s.frame_looped(641)[0], 641 % 3 + 1);
    }
}
