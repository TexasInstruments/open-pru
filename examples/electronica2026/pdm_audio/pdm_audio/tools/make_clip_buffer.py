#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Build the 8-channel clip buffer of the pdm_audio demo as a C header.

RTU1 reads one frame of 8 x int16 (16 B) from MSRAM every 16 kHz sample, so
the R5F application needs one frame-interleaved buffer: sample (frame f,
channel c) is element f*8 + c. This tool loads the 8 WAV files in clips/
and writes mcuplus/pdm_clips.h with

    const int16_t pdm_clip_buffer[8000*8];     /* section ".pdm_clips" */
    #define PDM_CLIP_FRAMES 8000

Channel order (the order of CLIP_FILES below, also in clips/SOURCES.md):
    ch0 speech_librivox   ch1 chirp_lin_100_7000   ch2 chirp_log_100_7000
    ch3 tone_1000 (1 kHz) ch4 tone_3000 (3 kHz)    ch5 tone_500 (500 Hz)
    ch6 tone_2000 (2 kHz) ch7 tone_5000 (5 kHz)

Loading mirrors pru-simulator source/pdm/pdm_layout.py::load_clip (a test in
that repo compares the header to it): 16 kHz, 16-bit, mono or the first
channel of a multi-channel file, at most 8192 frames, normalised so that
max |x| == 16384 (0.5 FS), and a silent clip stays all zero.

Looping: every channel has exactly PDM_CLIP_FRAMES (8000 = 0.5 s) frames.
A clip shorter than that repeats from its first frame until the buffer is
full (the 4000-frame tones and chirps play twice, the wrap is at frame 4000,
not at the end of the buffer). A clip longer than that is truncated, so the
speech clip (7201 frames) fills 7201 frames and then repeats from frame 0.
RTU1 itself wraps from frame 7999 to frame 0.

Usage (from the project directory pdm_audio/pdm_audio):
    tools/make_clip_buffer.py                write mcuplus/pdm_clips.h
    tools/make_clip_buffer.py --check        fail if the committed header
                                             differs from a fresh build
    tools/make_clip_buffer.py --make-tones   (re)write clips/tone_500.wav,
                                             tone_2000.wav, tone_5000.wav
"""

import argparse
import difflib
import math
import struct
import sys
import wave
from pathlib import Path

PROJECT = Path(__file__).resolve().parent.parent
CLIPS_DIR = PROJECT / "clips"
HEADER = PROJECT / "mcuplus" / "pdm_clips.h"

PCM_RATE = 16000
CLIP_MAX_FRAMES = 8192        # same limit as pdm_layout.load_clip
NORM_PEAK = 16384             # 0.5 FS
FRAMES = 8000                 # frames in the buffer: 0.5 s, 128 KB
CHANNELS = 8
SECTION = ".pdm_clips"
PER_LINE = 16                 # int16 values per line of the header

# Channel order; index = channel.
CLIP_FILES = [
    "speech_librivox.wav",
    "chirp_lin_100_7000.wav",
    "chirp_log_100_7000.wav",
    "tone_1000.wav",
    "tone_3000.wav",
    "tone_500.wav",
    "tone_2000.wav",
    "tone_5000.wav",
]

# Tones made by --make-tones, in the style of pru-simulator
# source/pdm/tools/make_clips.py (0.25 s, amplitude 0.5 FS, 16 kHz).
TONES = {"tone_500.wav": 500.0, "tone_2000.wav": 2000.0, "tone_5000.wav": 5000.0}
TONE_DURATION = 0.25
TONE_AMPLITUDE = 0.5


def load_clip(path: Path) -> list[int]:
    """Mono (or first channel) 16 kHz 16-bit WAV, normalised to peak 16384."""
    try:
        w = wave.open(str(path), "rb")
    except (wave.Error, EOFError, FileNotFoundError) as e:
        raise ValueError(f"{path.name}: not a readable PCM WAV ({e})") from e
    with w:
        rate, width = w.getframerate(), w.getsampwidth()
        nch, nframes = w.getnchannels(), w.getnframes()
        if rate != PCM_RATE:
            raise ValueError(f"{path.name}: sample rate {rate} Hz, need {PCM_RATE}")
        if width != 2:
            raise ValueError(f"{path.name}: {8 * width}-bit samples, need 16-bit")
        if nframes > CLIP_MAX_FRAMES:
            raise ValueError(f"{path.name}: {nframes} frames, "
                             f"at most {CLIP_MAX_FRAMES} allowed")
        raw = w.readframes(nframes)
    samples = struct.unpack(f"<{len(raw) // 2}h", raw)[::nch]
    peak = max((abs(s) for s in samples), default=0)
    if peak == 0:
        return [0] * len(samples)
    return [round(s * NORM_PEAK / peak) for s in samples]


def loop_to(clip: list[int], frames: int = FRAMES) -> list[int]:
    """Repeat from the start up to `frames`; truncate a longer clip."""
    if not clip:
        raise ValueError("empty clip")
    return [clip[f % len(clip)] for f in range(frames)]


def build_buffer(clips_dir: Path = CLIPS_DIR) -> list[int]:
    """Frame-interleaved FRAMES x CHANNELS int16 values."""
    cols = [loop_to(load_clip(clips_dir / name)) for name in CLIP_FILES]
    return [cols[c][f] for f in range(FRAMES) for c in range(CHANNELS)]


def render_header(values: list[int]) -> str:
    assert len(values) == FRAMES * CHANNELS
    lines = []
    for i in range(0, len(values), PER_LINE):
        lines.append("    " + ", ".join(f"{v}" for v in values[i:i + PER_LINE]) + ",")
    channels = "\n".join(f" *   ch{c}  {n}" for c, n in enumerate(CLIP_FILES))
    return f"""\
/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
 */

/*
 * pdm_clips.h - clip buffer of the pdm_audio demo. GENERATED by
 * tools/make_clip_buffer.py from the WAV files in clips/, do not edit; the script's
 * --check mode fails if this file is out of date.
 *
 * Frame-interleaved, little endian: sample (frame f, channel c) is
 * pdm_clip_buffer[f * 8 + c]. One frame is 16 B, {FRAMES} frames (0.5 s at
 * 16 kHz) are {FRAMES * CHANNELS * 2} B. Each clip is normalised to a peak of
 * 16384 (0.5 FS); a shorter clip repeats from its first frame until the
 * buffer is full, a longer one is truncated.
 *
 * Channel order:
{channels}
 *
 * Section: the array is placed in the output section "{SECTION}", so the
 * linker command file can put it into an uncached MSRAM region that RTU1
 * reads through 0x7000_0000 + offset. Include this header in exactly one
 * source file, it defines the array. Other code can refer to it as
 *     extern const int16_t pdm_clip_buffer[PDM_CLIP_FRAMES * 8];
 */

#ifndef PDM_CLIPS_H_
#define PDM_CLIPS_H_

#include <stdint.h>

#define PDM_CLIP_FRAMES {FRAMES}
#define PDM_CLIP_CHANNELS {CHANNELS}
#define PDM_CLIP_SECTION "{SECTION}"

__attribute__((section("{SECTION}"), aligned(16)))
const int16_t pdm_clip_buffer[{FRAMES}*{CHANNELS}] = {{
{chr(10).join(lines)}
}};

#endif /* PDM_CLIPS_H_ */
"""


def make_tones(out_dir: Path = CLIPS_DIR) -> list[Path]:
    n = int(PCM_RATE * TONE_DURATION)
    out_dir.mkdir(parents=True, exist_ok=True)
    paths = []
    for name, freq in TONES.items():
        samples = [round(TONE_AMPLITUDE * 32767 * math.sin(2 * math.pi * freq * (i / PCM_RATE)))
                   for i in range(n)]
        with wave.open(str(out_dir / name), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(PCM_RATE)
            w.writeframes(struct.pack(f"<{n}h", *samples))
        paths.append(out_dir / name)
    return paths


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true",
                    help="fail if the committed header differs from a fresh build")
    ap.add_argument("--make-tones", action="store_true",
                    help="write clips/tone_500.wav, tone_2000.wav, tone_5000.wav and exit")
    ap.add_argument("--clips", type=Path, default=CLIPS_DIR, help="clip directory")
    ap.add_argument("--out", type=Path, default=HEADER, help="header path")
    args = ap.parse_args(argv)

    if args.make_tones:
        for p in make_tones(args.clips):
            print(p)
        return 0

    try:
        text = render_header(build_buffer(args.clips))
    except ValueError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    if args.check:
        have = args.out.read_text() if args.out.exists() else ""
        if have == text:
            print(f"{args.out}: up to date")
            return 0
        diff = difflib.unified_diff(have.splitlines(), text.splitlines(),
                                    "committed", "regenerated", lineterm="", n=0)
        for i, line in enumerate(diff):
            if i >= 20:
                print("...")
                break
            print(line, file=sys.stderr)
        print(f"error: {args.out} is out of date, run tools/make_clip_buffer.py",
              file=sys.stderr)
        return 1

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(text)
    print(f"{args.out}: {FRAMES} frames x {CHANNELS} ch, {len(text)} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
