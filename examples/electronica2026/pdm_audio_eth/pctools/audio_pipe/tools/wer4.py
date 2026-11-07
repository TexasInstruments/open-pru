# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
# python3 tools/wer4.py <out dir> <scene dir>: per-talker WER for scenes rendered with tools/render_mics.js
import json, re, sys
REF = {"male": "hi there how are you doing today", "female": "i'm good what is the plan for the day",
       "male_low": "please bring the red toolbox to station four", "female_high": "the conveyor belt stopped five minutes ago"}
NUM = {"4": "four", "5": "five"}
def norm(t):
    t = t.lower().replace("what's", "what is")
    return [NUM.get(w, w) for w in re.sub(r"[^a-z0-9' ]", " ", t).split()]
def wer(r, h):
    r, h = norm(r), norm(h); d = list(range(len(h) + 1))
    for i in range(1, len(r) + 1):
        nd = [i] + [0] * len(h)
        for j in range(1, len(h) + 1): nd[j] = min(d[j] + 1, nd[j - 1] + 1, d[j - 1] + (r[i - 1] != h[j - 1]))
        d = nd
    return d[-1] / len(r)
tr = json.load(open(sys.argv[1] + "/transcript.json")); st = json.load(open(sys.argv[2] + "/settings.json"))
ang = lambda a, b: abs((a - b + 180) % 360 - 180)
used, tot = set(), []
for s in sorted(st["speakers"], key=lambda s: s["start"]):
    # the stream whose text is closest to this talker, among streams within 35° (bias from noise sources nearby)
    c = [t for t in tr if ang(t["azimuth_deg"], s["az"]) <= 35 and t["speaker"] not in used]
    best = min(c, key=lambda t: (wer(REF[s["voice"]], t["text"]), ang(t["azimuth_deg"], s["az"]))) if c else None
    w = wer(REF[s["voice"]], best["text"]) if best else 1.0
    if best: used.add(best["speaker"])
    tot.append(w)
    print(f"  {s['voice']:11s} {s['az']:5.0f}° -> {('%5.1f°' % best['azimuth_deg']) if best else ' MISS '}  WER {w:.2f}  {best['text'] if best else ''}")
extra = [t for t in tr if t["speaker"] not in used and t["text"]]
print(f"  mean WER {sum(tot)/len(tot):.2f}; extra streams with text: {[(round(t['azimuth_deg']), t['text']) for t in extra]}")
