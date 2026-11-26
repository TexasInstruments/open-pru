# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
# python3 tools/bench_diff.py <tagA> <tagB>: per-talker WER / azimuth changes between two bench runs
import json, os, sys
TD = os.environ.get("TESTDATA", os.path.join(os.path.dirname(os.path.abspath(__file__)), "testdata"))
A, B = (json.load(open(os.path.join(TD, "bench", f"bench_{t}.json"))) for t in sys.argv[1:3])
for n, sa in A["scenes"].items():
    sb = B["scenes"].get(n)
    if not sb or "talkers" not in sa or "talkers" not in sb: continue
    for ta, tb in zip(sa["talkers"], sb["talkers"]):
        wa, wb = min(ta["wer"], 1), min(tb["wer"], 1)
        if abs(wa - wb) > 1e-9 or ta["est"] != tb["est"] and (ta["est"] is None or tb["est"] is None or abs(ta["est"] - tb["est"]) > 3):
            print(f"{n:18s} {ta['voice']:11s} WER {wa:.2f} -> {wb:.2f}  az {ta['est']} -> {tb['est']}  | {tb['text'][:60]}")
