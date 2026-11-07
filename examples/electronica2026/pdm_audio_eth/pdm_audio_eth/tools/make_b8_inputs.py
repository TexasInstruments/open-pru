#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Board check B8 (docs/test_report.md): write the 8 input WAVs for
`pdm-eth stream`, 16 kHz mono 16-bit, peak 16384 (0.5 FS, as the clips).

    python3 tools/make_b8_inputs.py OUTDIR [--seconds 70]

The board's input ring is the 16384-frame clip buffer (1.024 s). If an
AUDIO_IN block (32 frames) never lands, RTU1 plays the audio of 1.024 s
earlier there. A tone at a multiple of 1/1.024 Hz (for example 750, 1000 or
2000 Hz) would hide that, so the tones here are not:

  ch0 757 Hz              ch4 263 Hz
  ch1 lin. chirp 100 -> 7000 Hz, 5 s, repeated
  ch2 noise, 4 kHz low-pass, fixed seed
  ch3 1243 Hz             ch5 3911 Hz
  ch6 443 Hz + 887 Hz     ch7 1511 Hz, level 1, 1/2, 1/4, 1/8 per 10 s
"""
import argparse
import wave
from pathlib import Path

import numpy as np

FS = 16000
PEAK = 16384
TONES = {0: 757.0, 3: 1243.0, 4: 263.0, 5: 3911.0}
TWO_TONE = (443.0, 887.0)
STEP_TONE = 1511.0
STEP_S = 10
CHIRP = (100.0, 7000.0, 5.0)
NOISE_CUTOFF = 4000.0
SEED = 2026


def signals(seconds):
    n = int(seconds * FS)
    t = np.arange(n) / FS
    out = {}
    for ch, f in TONES.items():
        out[ch] = np.sin(2 * np.pi * f * t)
    f0, f1, period = CHIRP
    tc = t % period
    out[1] = np.sin(2 * np.pi * (f0 * tc + (f1 - f0) / (2 * period) * tc ** 2))
    spec = np.fft.rfft(np.random.default_rng(SEED).standard_normal(n))
    spec[np.fft.rfftfreq(n, 1 / FS) > NOISE_CUTOFF] = 0
    noise = np.fft.irfft(spec, n)
    out[2] = noise / np.abs(noise).max()
    out[6] = 0.5 * (np.sin(2 * np.pi * TWO_TONE[0] * t) + np.sin(2 * np.pi * TWO_TONE[1] * t))
    level = 0.5 ** ((t // STEP_S) % 4)
    out[7] = level * np.sin(2 * np.pi * STEP_TONE * t)
    return {ch: np.round(PEAK * x).astype("<i2") for ch, x in out.items()}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("outdir", type=Path)
    ap.add_argument("--seconds", type=float, default=70.0)
    a = ap.parse_args()
    a.outdir.mkdir(parents=True, exist_ok=True)
    for ch, x in sorted(signals(a.seconds).items()):
        p = a.outdir / f"b8_ch{ch}.wav"
        with wave.open(str(p), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(FS)
            w.writeframes(x.tobytes())
        print(f"{p}: {len(x)} frames, peak {np.abs(x).max()}")


if __name__ == "__main__":
    main()
