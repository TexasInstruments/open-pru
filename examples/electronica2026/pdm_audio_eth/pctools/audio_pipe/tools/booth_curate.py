# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
# python3 tools/booth_curate.py <transcript log> <playlist dir>
#
# Scores a `pdm-eth gui --transcript-log` run per scene and pass (booth
# transcripts design 3): a scene PASSES if its last 5 scored passes have every
# talker word for word right (WER 0 after wer4.py's normalisation) and no text
# on any other stream. Not scored: passes before the first text line
# (audio_pipe starting) and the pass running when the log ends. Exit 0 only if
# every scene of the playlist passes.
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from wer4 import score

NEED = 5


def main(logp, pldir):
    marks, texts = [], {}
    for line in open(logp):
        try:
            j = json.loads(line)
        except ValueError:
            continue
        if "mark" in j:
            marks.append(j["mark"])
        elif "text" in j:
            texts.setdefault(j["pass"], []).append(j)
    first_text = min(texts) if texts else None
    scored = [m for m in marks[:-1] if first_text is not None and m["pass"] >= first_text]
    scenes = sorted(d for d in os.listdir(pldir) if os.path.isfile(os.path.join(pldir, d, "mic1.wav")) or
                    os.path.isfile(os.path.join(pldir, d, "settings.json")))
    results = {s: [] for s in scenes}
    for m in scored:
        sd = os.path.join(pldir, m["scene"])
        if not os.path.isfile(os.path.join(sd, "settings.json")):
            print(f"pass {m['pass']}: scene {m['scene']} not in {pldir}, skipped")
            continue
        st = json.load(open(os.path.join(sd, "settings.json")))
        # one stream per audio_pipe speaker id: its utterances in time order
        by = {}
        for t in sorted(texts.get(m["pass"], []), key=lambda t: t["start"]):
            s = by.setdefault(t["speaker"], {"speaker": t["speaker"], "azimuth_deg": t["az"], "text": ""})
            s["text"] = (s["text"] + " " + t["text"]).strip()
        per, extra = score(list(by.values()), st)
        bad = [f"{v} WER {w:.2f} \"{text}\"" for v, az, found, w, text in per if w > 0]
        mean = sum(p[3] for p in per) / len(per)
        print(f"{m['scene']} pass {m['pass']}: mean WER {mean:.2f}, extra {[(round(a), t) for a, t in extra]}")
        for v, az, found, w, text in per:
            print(f"    {v:11s} {az:5.0f}° -> {('%5.1f°' % found) if found is not None else ' MISS '}  WER {w:.2f}  {text}")
        why = [f"pass {m['pass']}: {b}" for b in bad] + [f"pass {m['pass']}: extra text at {round(a)}°: \"{t}\"" for a, t in extra]
        results.setdefault(m["scene"], []).append(why)
    ok = True
    for s in sorted(results):
        last = results[s][-NEED:]
        if len(last) < NEED:
            print(f"{s}: FAIL: {len(last)} of {NEED} passes scored")
            ok = False
            continue
        reasons = [r for why in last for r in why]
        if reasons:
            print(f"{s}: FAIL: " + "; ".join(reasons))
            ok = False
        else:
            print(f"{s}: PASS ({NEED} passes exact)")
    return 0 if ok else 1


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__ if __doc__ else "usage: booth_curate.py <transcript log> <playlist dir>")
    sys.exit(main(sys.argv[1], sys.argv[2]))
