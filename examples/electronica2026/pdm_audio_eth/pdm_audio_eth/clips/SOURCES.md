# Booth clips: origin

All clips are 16-bit mono 16 kHz WAV, at most 0.512 s (8192 frames).
`pdm_layout.load_clip()` normalises them to a peak of 0.5 FS (16384) at load
time; the files themselves are stored un-normalised.

## Channel order of the clip buffer

`tools/make_clip_buffer.py` loads these 8 files into `mcuplus/pdm_clips.h`
(frame-interleaved, 8000 frames, shorter clips repeat from their start, a
longer clip is truncated; see the tool's docstring). Channel k of the buffer
is the clip of ΣΔ channel k:

| Channel | File | Content | Origin |
|---|---|---|---|
| 0 | `speech_librivox.wav` | speech, 7201 frames | LibriVox excerpt, see below (copied from pru-simulator) |
| 1 | `chirp_lin_100_7000.wav` | linear chirp 100 to 7000 Hz | pru-simulator `make_clips.py chirps` (copied) |
| 2 | `chirp_log_100_7000.wav` | logarithmic (exponential) chirp 100 to 7000 Hz | pru-simulator `make_clips.py chirps` (copied) |
| 3 | `tone_1000.wav` | 1 kHz sine | pru-simulator `make_clips.py chirps` (copied) |
| 4 | `tone_3000.wav` | 3 kHz sine | pru-simulator `make_clips.py chirps` (copied) |
| 5 | `tone_500.wav` | 500 Hz sine | `tools/make_clip_buffer.py --make-tones` (generated here) |
| 6 | `tone_2000.wav` | 2 kHz sine | `tools/make_clip_buffer.py --make-tones` (generated here) |
| 7 | `tone_5000.wav` | 5 kHz sine | `tools/make_clip_buffer.py --make-tones` (generated here) |

## Generated clips

The four copied generated clips (two chirps, 1 kHz and 3 kHz) were produced
by `python3 source/pdm/tools/make_clips.py chirps` in pru-simulator (stdlib
`wave` and `math` only; 0.25 s = 4000 frames each, amplitude 0.5 FS) and
copied here unchanged. `tone_500.wav`, `tone_2000.wav` and `tone_5000.wav`
are produced by `tools/make_clip_buffer.py --make-tones` in the same style
(16-bit mono 16 kHz, 0.25 s = 4000 frames, amplitude 0.5 FS, sine).

## speech_librivox.wav

- Book: *Letters of Two Brides* by Honoré de Balzac, translated by R. S. Scott (LibriVox, English)
- Section: 01, "Letter 1" (29:24), as listed on the book page below
- Reader: Kara Shallenberg (1969-2023), exactly as the book page lists the
  reader of section 01 (https://librivox.org/reader/19), retrieved 2026-11-03
- Book page: https://librivox.org/letters-of-two-brides-by-honore-de-balzac/
- File URL: https://www.archive.org/download/letters_brides_0709_librivox/letters_of_two_brides_01_debalzac_64kb.mp3
- Start time: 115.5 s into the section file
- Duration: 0.45 s (7201 frames after resampling)
- Selection: sustained high-energy stretch with a low zero-crossing rate
  (voiced speech), picked by an RMS and zero-crossing scan, not by listening.
- Conversion (run through `python3 source/pdm/tools/make_clips.py speech
  <in-file> --start 115.5 --dur 0.45`, which executes):

  ```
  ffmpeg -y -ss 115.5 -t 0.45 -i letters_of_two_brides_01_debalzac_64kb.mp3 -ac 1 -ar 16000 -sample_fmt s16 speech_librivox.wav
  ```

- Licence: LibriVox recordings are in the public domain (the book page says
  "LibriVox recordings are Public Domain in the USA"). The downloaded MP3
  is kept outside the repository; only this converted excerpt is committed.
