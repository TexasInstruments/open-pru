# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
# python3 tools/wer4.py <out dir> <scene dir>: per-talker WER for scenes rendered with tools/render_mics.js
# importable: REF, norm(t), wer(r, h), score(streams, settings) (used by booth_curate.py)
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
def ref(s):
    """reference sentence of a talker: its "text" in settings.json, else the voice's"""
    return s.get("text") or REF[s["voice"]]
ang = lambda a, b: abs((a - b + 180) % 360 - 180)
def score(streams, settings):
    """streams: [{"speaker", "azimuth_deg", "text"}] (transcript.json). Returns
    (per talker [(voice, az, found_az or None, wer, text)], extra [(az, text)])."""
    used, per = set(), []
    for s in sorted(settings["speakers"], key=lambda s: s["start"]):
        # the stream whose text is closest to this talker, among streams within 35° (bias from noise sources nearby)
        c = [t for t in streams if ang(t["azimuth_deg"], s["az"]) <= 35 and t["speaker"] not in used]
        best = min(c, key=lambda t: (wer(ref(s), t["text"]), ang(t["azimuth_deg"], s["az"]))) if c else None
        w = wer(ref(s), best["text"]) if best else 1.0
        if best: used.add(best["speaker"])
        per.append((s["voice"], s["az"], best["azimuth_deg"] if best else None, w, best["text"] if best else ""))
    extra = [(t["azimuth_deg"], t["text"]) for t in streams if t["speaker"] not in used and t["text"]]
    return per, extra
if __name__ == "__main__":
    tr = json.load(open(sys.argv[1] + "/transcript.json")); st = json.load(open(sys.argv[2] + "/settings.json"))
    per, extra = score(tr, st)
    for voice, az, found, w, text in per:
        print(f"  {voice:11s} {az:5.0f}° -> {('%5.1f°' % found) if found is not None else ' MISS '}  WER {w:.2f}  {text}")
    print(f"  mean WER {sum(p[3] for p in per)/len(per):.2f}; extra streams with text: {[(round(a), t) for a, t in extra]}")
