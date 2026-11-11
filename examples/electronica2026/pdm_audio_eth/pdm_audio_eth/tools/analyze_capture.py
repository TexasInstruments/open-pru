#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Board check B6 (docs/test_report.md): analyse `pdm-eth capture --out DIR`.

Usage: python3 tools/analyze_capture.py DIR   (DIR holds out_ch0..7.wav, 16 kHz, 24-bit mono)

- tones ch3..7: least-squares sine fit per 10 s block at the nominal
  frequency (board sample clock), amplitude / 2^22, fit residual in dB,
  frequency from the phase drift across blocks
- every channel: loop check. The board plays a 16384-frame clip in a loop,
  so frame n and n + 16384 must agree up to the PDM noise floor; a lost,
  duplicated or zero-filled frame shows as a large difference.
- ch0..2: peak, RMS, and the best normalised correlation with the clip
  channel of pdm_clips.h (right clip in the right channel)
"""
import re
import sys
import wave
from pathlib import Path

import numpy as np

FS = 16000
LOOP = 16384
FULL = 2 ** 22  # clip peak 16384 (0.5 FS int16) = 2^22 in 24-bit PCM
TONES = {3: 1000.0, 4: 3000.0, 5: 500.0, 6: 2000.0, 7: 5000.0}
REF_AMP = {3: 1.000, 4: 0.942, 5: 0.999, 6: 0.981, 7: 0.850}  # pdm_audio board run
HEADER = Path(__file__).resolve().parent.parent / "mcuplus" / "pdm_clips.h"


def read24(path):
    with wave.open(str(path)) as w:
        assert (w.getnchannels(), w.getsampwidth(), w.getframerate()) == (1, 3, FS), path
        raw = w.readframes(w.getnframes())
    b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
    v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
    return np.where(v & 0x800000, v - (1 << 24), v).astype(np.float64)


def clip_channels():
    text = HEADER.read_text()
    body = text[text.index("pdm_clip_buffer[") :]
    body = body[body.index("{") + 1 : body.index("};")]
    v = np.array([int(x) for x in re.findall(r"-?\d+", body)], np.float64)
    return v.reshape(-1, 8).T


def sine_fit(x, f):
    n = np.arange(len(x))
    a = np.column_stack([np.cos(2 * np.pi * f / FS * n), np.sin(2 * np.pi * f / FS * n), np.ones(len(x))])
    coef, *_ = np.linalg.lstsq(a, x, rcond=None)
    res = x - a @ coef
    amp = np.hypot(coef[0], coef[1])
    return amp, np.arctan2(-coef[1], coef[0]), 20 * np.log10(np.std(res) / (amp / np.sqrt(2)))


def loop_check(x):
    """Worst RMS (dB of FULL) of x[n+LOOP]-x[n] per loop, and the worst sample."""
    k = len(x) // LOOP
    blocks = x[: k * LOOP].reshape(k, LOOP)
    d = np.diff(blocks, axis=0)
    rms = np.sqrt(np.mean(d ** 2, axis=1))
    worst = int(np.argmax(np.abs(d).max(axis=1)))
    db = lambda v: 20 * np.log10(max(v, 1e-3) / FULL)  # 1e-3 LSB floor: bit-exact loops
    return k, db(rms.max()), db(np.median(rms)), db(np.abs(d).max()), worst


def best_corr(x, clip):
    seg = x[LOOP : 2 * LOOP]
    seg = (seg - seg.mean()) / (np.std(seg) + 1e-9)
    c = (clip - clip.mean()) / (np.std(clip) + 1e-9)
    r = np.fft.irfft(np.fft.rfft(seg) * np.conj(np.fft.rfft(c)), LOOP) / LOOP
    return float(r.max()), int(r.argmax())


def main(d):
    d = Path(d)
    clips = clip_channels()
    print(f"{'ch':>2} {'frames':>9} {'loops':>5} {'loop diff max/median dB':>24} {'worst sample dB':>16}")
    data = {}
    for ch in range(8):
        x = read24(d / f"out_ch{ch}.wav")
        data[ch] = x
        k, lmax, lmed, smax, worst = loop_check(x)
        print(f"{ch:>2} {len(x):>9} {k:>5} {lmax:>11.1f} / {lmed:>6.1f} {smax:>16.1f}  (loop {worst})")
    print()
    print(f"{'ch':>2} {'f nominal':>9} {'f fit':>11} {'amp/2^22':>9} {'pdm_audio':>9} {'amp min..max':>15} {'residual dB':>12}")
    for ch, f in TONES.items():
        x = data[ch]
        blk = min(10 * FS, len(x) // 2)
        fits = [sine_fit(x[i : i + blk], f) for i in range(0, len(x) - blk + 1, blk)]
        amps = np.array([a for a, _, _ in fits]) / FULL
        ph = np.unwrap([p for _, p, _ in fits])
        drift = np.polyfit(np.arange(len(ph)) * blk / FS, ph, 1)[0] if len(ph) > 1 else 0.0
        res = max(r for _, _, r in fits)
        print(f"{ch:>2} {f:>9.1f} {f + drift / (2 * np.pi):>11.4f} {np.median(amps):>9.3f} {REF_AMP[ch]:>9.3f} "
              f"{amps.min():>7.3f}..{amps.max():.3f} {res:>12.1f}")
    print()
    for ch in range(3):
        x = data[ch]
        r = [best_corr(x, clips[c])[0] for c in range(8)]
        print(f"ch{ch}: peak {np.abs(x).max() / FULL:.3f}, rms {np.sqrt(np.mean(x ** 2)) / FULL:.3f} x 2^22; "
              f"correlation with clip ch0..7: " + " ".join(f"{v:.2f}" for v in r))


if __name__ == "__main__":
    main(sys.argv[1])
