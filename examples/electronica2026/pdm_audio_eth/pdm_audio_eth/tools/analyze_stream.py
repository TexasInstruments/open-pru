#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Board check B8 (docs/test_report.md): compare in_ch*.wav and out_ch*.wav of
`pdm-eth stream --capture DIR` (inputs from tools/make_b8_inputs.py).

    python3 tools/analyze_stream.py DIR

The input first goes through the decoder's DC blocker (pru0_rx.asm:
e += (y - e) * 2^-7, y -= e, a 20 Hz high-pass with a 128-sample tail that
no short FIR can model). The lag comes from the noise channel (ch2): the
tones correlate at many lags. Then, per channel, a least-squares FIR (TAPS
taps around the lag) maps input to output; it absorbs the rest of the
chain's response (zero-order hold, decimation filter, the odd channels'
half-sample stagger). The residual is
given for the whole run and for the worst 64-frame block (one AUDIO_IN
block). A block that never reached the clip buffer plays audio of 1.024 s
earlier there, so its residual is near 0 dB; the inputs are chosen so that
1.024 s-old audio differs (tools/make_b8_inputs.py).

Also: gain of the tone channels (output / input at the tone frequency,
24-bit output vs 16-bit input, so 256 = 0 dB), and the level steps of ch7.
"""
import sys
import wave
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_capture import read24  # noqa: E402
from make_b8_inputs import STEP_S, STEP_TONE, TONES, TWO_TONE  # noqa: E402

FS = 16000
TAPS = 33
BLOCK = 64
EDGE = 2048       # frames left out at both ends of the overlap
SCALE = 256.0     # 24-bit output / 16-bit input


def read16(path):
    with wave.open(str(path)) as w:
        assert (w.getnchannels(), w.getsampwidth(), w.getframerate()) == (1, 2, FS), path
        return np.frombuffer(w.readframes(w.getnframes()), "<i2").astype(np.float64)


def dc_block(x):
    """pru0_rx.asm DC blocker in float: e += (y - e) / 128, y -= e. The
    one-pole low-pass e has the impulse response 2^-7 (1 - 2^-7)^k, below
    1e-14 after 4096 samples: FFT convolution with that is exact enough."""
    k = np.arange(4096)
    h = 2.0 ** -7 * (1.0 - 2.0 ** -7) ** k
    n = 1 << int(np.ceil(np.log2(len(x) + len(h))))
    e = np.fft.irfft(np.fft.rfft(x, n) * np.fft.rfft(h, n), n)[: len(x)]
    return x - e


def lag_of(x, y):
    """k so that y[n + k] best matches x[n]."""
    n = 1 << int(np.ceil(np.log2(len(x) + len(y))))
    r = np.fft.irfft(np.fft.rfft(y, n) * np.conj(np.fft.rfft(x, n)), n)
    k = int(np.argmax(np.abs(r)))
    return k if k < n // 2 else k - n


def fir_fit(x, y, lag):
    """h (TAPS taps) with y[n + lag] ~ sum_j h[j] x[n + j - TAPS//2]; aligned x, y, fit."""
    half = TAPS // 2
    start = max(EDGE, half, EDGE - lag)
    stop = min(len(x) - half, len(y) - lag) - EDGE
    xs = x[start - half : stop + half]
    ys = y[start + lag : stop + lag]
    cols = np.lib.stride_tricks.sliding_window_view(xs, TAPS)[:, ::-1]
    h, *_ = np.linalg.lstsq(cols[:: 7], ys[:: 7], rcond=None)  # every 7th row is plenty
    return xs[half:-half], ys, cols @ h


def db(a, b):
    return 10 * np.log10(max(a, 1e-30) / max(b, 1e-30))


def tone_amp(x, f):
    n = np.arange(len(x))
    a = np.column_stack([np.cos(2 * np.pi * f / FS * n), np.sin(2 * np.pi * f / FS * n)])
    c, *_ = np.linalg.lstsq(a, x, rcond=None)
    return np.hypot(*c)


def main(d):
    d = Path(d)
    lag = lag_of(read16(d / "in_ch2.wav"), read24(d / "out_ch2.wav"))
    print(f"lag of the output (from ch2, noise): {lag} frames = {lag / FS * 1e3:.2f} ms")
    print(f"{'ch':>2} {'frames':>8} {'residual dB':>11} {'worst block dB':>14} {'block':>6} {'gain dB':>8}")
    for ch in range(8):
        x = dc_block(read16(d / f"in_ch{ch}.wav"))
        y = read24(d / f"out_ch{ch}.wav")
        xa, ya, fit = fir_fit(x, y, lag)
        err = ya - fit
        k = len(err) // BLOCK
        pe = np.mean(err[: k * BLOCK].reshape(k, BLOCK) ** 2, axis=1)
        ps = np.mean(ya[: k * BLOCK].reshape(k, BLOCK) ** 2, axis=1)
        blk_db = 10 * np.log10(np.maximum(pe, 1e-30) / np.maximum(ps.mean(), 1e-30))
        worst = int(np.argmax(blk_db))
        f = TONES.get(ch) or {6: TWO_TONE[0], 7: STEP_TONE}.get(ch)
        gain = f"{20 * np.log10(tone_amp(ya, f) / tone_amp(xa, f) / SCALE):8.2f}" if f else f"{'':>8}"
        print(f"{ch:>2} {len(ya):>8} {db(np.mean(err ** 2), np.mean(ya ** 2)):>11.1f} "
              f"{blk_db[worst]:>14.1f} {worst:>6} {gain}")
        if ch == 7:
            # 10 s input segments, mapped to the output by the lag, 0.1 s
            # trimmed at both ends so no segment sees the next level
            seg, trim = STEP_S * FS, FS // 10
            levels = []
            for i in range(len(x) // seg):
                o = y[i * seg + lag + trim : (i + 1) * seg + lag - trim]
                levels.append(20 * np.log10(tone_amp(o, STEP_TONE) / (SCALE * 16384)))
            want = [-6.02 * (i % 4) for i in range(len(levels))]
            print("   ch7 level per 10 s, dB of the input's full level (input: "
                  + " ".join(f"{w:.2f}" for w in want) + "):")
            print("      " + " ".join(f"{v:.2f}" for v in levels))


if __name__ == "__main__":
    main(sys.argv[1])
