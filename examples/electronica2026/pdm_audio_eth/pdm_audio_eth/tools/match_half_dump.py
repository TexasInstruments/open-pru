#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Board check B6, CCS spot check: find the PCM halves of a CCS memory dump
in a `pdm-eth capture`.

Save 0x30010100, 0x1000 bytes (half A and half B: 64 frames x 8 ch x int32,
signed 24-bit, frame-interleaved) from the CCS memory view, then

    python3 tools/match_half_dump.py DUMP CAPTURE_DIR

The tone channels 3 to 7 repeat bit for bit every 16384 frames (the clip
loop), so a dump taken at any time must occur in the capture. Each tone
repeats bit for bit within at most 32 frames (500 Hz), so a 64-frame window
matches at many clip positions: the check is that all five channels of a
half match at a common position (values in board memory = values that came
over Ethernet), not where. PRU0 may be writing a half while CCS reads it: a
half that matches nowhere is reported, the check needs one full match.
"""
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_capture import LOOP, TONES, read24  # noqa: E402
from compare_clip_dump import dump_bytes  # noqa: E402

FRAMES = 64


def find(seq, x):
    """Capture frames n (within the first loop + 64) with x[n:n+64] == seq."""
    span = x[: LOOP + FRAMES]
    windows = np.lib.stride_tricks.sliding_window_view(span, FRAMES)
    return list(np.flatnonzero((windows == seq).all(axis=1)))


def main(argv):
    dump, cap = Path(argv[1]), Path(argv[2])
    raw = dump_bytes(dump)
    if len(raw) < 2 * FRAMES * 32:
        sys.exit(f"{dump}: {len(raw)} bytes, need {2 * FRAMES * 32} (half A and half B)")
    pcm = np.frombuffer(raw[: 2 * FRAMES * 32], "<i4").reshape(2, FRAMES, 8).astype(np.float64)
    caps = {ch: read24(cap / f"out_ch{ch}.wav") for ch in TONES}
    full = 0
    for h, name in enumerate("AB"):
        pos = {}
        for ch in TONES:
            hits = find(pcm[h, :, ch], caps[ch])
            pos[ch] = sorted({int(n) % LOOP for n in hits})
        cells = "  ".join(f"ch{ch} {len(p)}" for ch, p in pos.items())
        common = sorted(set.intersection(*(set(p) for p in pos.values())))
        ok = bool(common)
        full += ok
        where = f"{len(common)} common clip positions, first {common[:4]}" if ok else "no common match"
        print(f"half {name}: {where}   [positions per channel: {cells}]")
    print("MATCH" if full else "NO MATCH")
    return 0 if full else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
