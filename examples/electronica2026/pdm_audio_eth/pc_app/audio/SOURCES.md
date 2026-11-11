# Example audio: origin

Example input files for the PC app: speech, music, and the two mixed. All
three are 16-bit mono 16 kHz WAV (plain PCM, 44-byte header, no metadata
chunks), exactly 30.000 s (480000 frames) each. The files are stored as
converted; nothing normalises them except the explicit gain in the mix.

| File | Content | Peak | RMS |
|---|---|---|---|
| `speech_librivox_30s.wav` | speech, LibriVox excerpt, see below | -4.6 dBFS | -24.6 dBFS |
| `music_chopin_nocturne_30s.wav` | solo piano, Musopen excerpt, see below | -8.4 dBFS | -28.5 dBFS |
| `speech_music_30s.wav` | the speech over the music, music RMS 12 dB below speech RMS | -1.0 dBFS | -20.8 dBFS |

## speech_librivox_30s.wav

- Book: *Letters of Two Brides* by Honoré de Balzac, translated by R. S. Scott (LibriVox, English)
- Section: 01, "Letter 1" (29:24), as listed on the book page below
- Reader: Kara Shallenberg (1969-2023), exactly as the book page lists the
  reader of section 01 (https://librivox.org/reader/19)
- Book page: https://librivox.org/letters-of-two-brides-by-honore-de-balzac/
- File URL: https://www.archive.org/download/letters_brides_0709_librivox/letters_of_two_brides_01_debalzac_64kb.mp3
  (MD5 of the downloaded MP3: `22b2f5a53d183f2455a79464d5ac3eea`, retrieved 2026-11-05)
- Start time: 97.5 s into the section file (well after the LibriVox intro
  disclaimer; the start falls in a pause, the level there is below -55 dBFS)
- Duration: 30 s (480000 frames after resampling)
- Selection: the 30 s window from 60 s on with the shortest longest pause
  (0.55 s; a pause is a 50 ms frame more than 35 dB below the window's
  loudest frame), found by an RMS scan, not by listening. Same recording as
  the booth clip `pdm_audio_eth/clips/speech_librivox.wav`.
- Conversion (10 ms fade-in, 50 ms fade-out so the file loops without a click):

  ```
  ffmpeg -y -ss 97.5 -t 30 -i letters_of_two_brides_01_debalzac_64kb.mp3 -ac 1 -ar 16000 -sample_fmt s16 -af "afade=t=in:d=0.01,afade=t=out:st=29.95:d=0.05" -map_metadata -1 -bitexact speech_librivox_30s.wav
  ```

- Licence: LibriVox recordings are in the public domain (the book page says
  "LibriVox recordings are Public Domain in the USA"). The downloaded MP3
  is kept outside the repository; only this converted excerpt is committed.

## music_chopin_nocturne_30s.wav

- Work: Frédéric Chopin (1810-1849), Nocturne in E-flat major, Op. 9 No. 2
  (published 1832; the composition is in the public domain)
- Performer: Frank Levy, piano. Source of the name: the `artist` tag of
  Musopen's own lossless file below (`artist: Frank Levy`, `album: Musopen:
  The Complete Works of Frédéric Chopin`, `date: 2015`). The item page itself
  lists no per-track performers.
- Item page: https://archive.org/details/musopen-chopin ("Musopen - The
  Complete Chopin Collection", uploaded 2015-01-29 by `aaron@musopen.org`,
  creator Aaron Dunn, Musopen's founder)
- File URL: https://archive.org/download/musopen-chopin/NocturneOp9No2.m4a
  (Apple Lossless, 96 kHz, 24-bit, stereo, 4:31.47; MD5
  `afddd01a0c9586d2ddc021f74e9399fe`, which matches the item's file list;
  retrieved 2026-11-05). The item also holds a second recording of the same
  nocturne, `Nocturne Op. 9 no. 2 in E flat major.m4a` (Aya Higuchi); that
  one is not used.
- Start time: 1.0 s into the file (the first note sounds at about 1.12 s)
- Duration: 30 s (480000 frames after resampling, downmixed to mono)
- Conversion (10 ms fade-in, 0.5 s fade-out; `-vn` drops the cover image):

  ```
  ffmpeg -y -ss 1.0 -t 30 -i NocturneOp9No2.m4a -vn -ac 1 -ar 16000 -sample_fmt s16 -af "afade=t=in:d=0.01,afade=t=out:st=29.5:d=0.5" -map_metadata -1 -bitexact music_chopin_nocturne_30s.wav
  ```

- Licence: CC0 1.0 Universal (public domain dedication) for the recording.
  Evidence, as seen on 2026-11-05:
  - The item page https://archive.org/details/musopen-chopin shows, under
    "Usage": "CC0 1.0 Universal", linked to
    http://creativecommons.org/publicdomain/zero/1.0/
  - The item metadata https://archive.org/metadata/musopen-chopin has
    `"licenseurl": "http://creativecommons.org/publicdomain/zero/1.0/"` and
    `"uploader": "aaron@musopen.org"`.

  The downloaded M4A is kept outside the repository; only this converted
  excerpt is committed.

## speech_music_30s.wav

The two files above, mixed. The music gain sets the music RMS 12 dB below
the speech RMS (speech -24.64 dBFS, music -28.50 dBFS over the full 30 s,
so the music gain is -24.64 - 12 - (-28.50) = -8.15 dB). The sum then gets
+3.53 dB so its peak is -1.0 dBFS (no sample clips):

```
ffmpeg -y -i speech_librivox_30s.wav -i music_chopin_nocturne_30s.wav -filter_complex "[1:a]volume=-8.15dB[m];[0:a][m]amix=inputs=2:normalize=0,volume=3.53dB" -ac 1 -ar 16000 -sample_fmt s16 -map_metadata -1 -bitexact speech_music_30s.wav
```

Run with ffmpeg 8.0.1. A least-squares fit of the result against the two
inputs gives gains 1.501 (speech) and 0.587 (music), music 12.00 dB below
the speech, residual -101 dBFS (rounding to 16 bits). Licence: as the two
inputs (public domain).
