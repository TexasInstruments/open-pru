#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Benchmark audio_pipe configurations on all test scenes (WER + azimuth error per talker).

  python3 tools/bench.py <tag> [audio_pipe run args...]      e.g.  bench.py ego_prior --ego-mode prior
  EGO=1 adds --ego-noise <scene>/ego (the robot-only take from tools/make_ego_takes.sh).
  BOARD=_board / _boardin: the factory4 scenes after the LP-AM243 PDM chain / its int16 input (tools/board_bench.py).
  TESTDATA = folder with scene_<name> renders (default tools/testdata). JOBS = parallel runs (default 4).
SDR: each talker's stream vs its dry source (<scene>/dry/, from tools/make_ego_takes.sh), with a 512-tap distortion
filter allowed (bss_eval-style), so reverb is not penalised but interference and noise are. Run with py/.venv/bin/python.
Writes bench_<tag>.json next to the outputs (in $TESTDATA/bench/).
"""
import glob, json, os, re, subprocess, sys
import numpy as np
import soundfile as sf
from scipy.linalg import solve_toeplitz
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, "..", "target", "release", "audio_pipe")
TD = os.environ.get("TESTDATA", os.path.join(HERE, "testdata"))
sys.path.insert(0, HERE)
from paths import SCENE_ROOT as UP  # folder with the audio_wav* scenes
REF = {"male": "hi there how are you doing today", "female": "i'm good what is the plan for the day",
       "male_low": "please bring the red toolbox to station four", "female_high": "the conveyor belt stopped five minutes ago"}
NUM = {"4": "four", "5": "five"}

# name -> (scene dir, ego dir, extra args)
SCENES = {"example": (os.path.join(UP, "audio_wav"), os.path.join(TD, "scene_home", "ego"), [])}
for s in ["home", "factory", "hospital", "automobile", "quiet", "robot"]:
    SCENES[s] = (os.path.join(TD, f"scene_{s}"), os.path.join(TD, f"scene_{s}", "ego"), [])
for s in ["factory4", "factory4_n10", "factory4_dense", "factory4_dense240"]:
    d = os.path.join(UP, f"audio_wav_{s}" + os.environ.get("BOARD", ""))  # BOARD=_board: after the PDM chain (tools/board_bench.py)
    SCENES[s] = (d, os.path.join(d, "ego"), ["--max-speakers", "8"])
TALKERS = {"example": [("male", 20), ("female", 150)]}


def norm(t):
    t = t.lower().replace("what's", "what is")
    return [NUM.get(w, w) for w in re.sub(r"[^a-z0-9' ]", " ", t).split()]


def wer(r, h):
    r, h = norm(r), norm(h)
    d = list(range(len(h) + 1))
    for i in range(1, len(r) + 1):
        nd = [i] + [0] * len(h)
        for j in range(1, len(h) + 1):
            nd[j] = min(d[j] + 1, nd[j - 1] + 1, d[j - 1] + (r[i - 1] != h[j - 1]))
        d = nd
    return min(d[-1] / len(r), 1.0)  # capped: a wrong stream counts like a miss


def angd(a, b):
    return abs((a - b + 180) % 360 - 180)


def sdr(ref, est, L=512):
    """bss_eval-style SDR with one reference: project est on ref filtered by a causal L-tap FIR."""
    n = min(len(ref), len(est))
    r, e = ref[:n].astype(np.float64), est[:n].astype(np.float64)
    nf = 1 << int(np.ceil(np.log2(2 * n)))
    Rf, Ef = np.fft.rfft(r, nf), np.fft.rfft(e, nf)
    ac = np.fft.irfft(Rf * np.conj(Rf), nf)[:L]
    xc = np.fft.irfft(Ef * np.conj(Rf), nf)[:L]
    ac[0] += 1e-8 * ac[0] + 1e-12
    h = solve_toeplitz(ac, xc)
    st = np.convolve(r, h)[:n]
    return 10 * np.log10(np.sum(st ** 2) / max(np.sum((e - st) ** 2), 1e-20))


def dry_ref(scene_dir, name, voice):
    d = os.path.join(TD, "scene_home") if name == "example" else scene_dir
    f = sorted(glob.glob(os.path.join(d, "dry", f"*_{voice}.wav")))
    return sf.read(f[0], dtype="float32")[0] if f else None


def talkers(name, d):
    if name in TALKERS:
        return TALKERS[name]
    st = json.load(open(os.path.join(d, "settings.json")))
    return [(s["voice"], s["az"]) for s in st["speakers"] if s["voice"] in REF]


def run(name, tag, extra):
    d, ego, args = SCENES[name]
    out = os.path.join(TD, "bench", f"{tag}_{name}")
    cmd = [BIN, "run", d, "--out", out, *args, *extra]
    if os.environ.get("EGO") and os.path.isdir(ego):
        cmd += ["--ego-noise", ego]
    env = dict(os.environ, RAYON_NUM_THREADS=os.environ.get("RAYON_NUM_THREADS", "4"))
    r = subprocess.run(cmd, capture_output=True, text=True, env=env)
    if r.returncode:
        return name, {"error": r.stderr[-400:]}
    tr = json.load(open(os.path.join(out, "transcript.json")))
    res, used = [], set()
    for v, az in talkers(name, d):
        c = sorted([t for t in tr if angd(t["azimuth_deg"], az) <= 25 and t["speaker"] not in used], key=lambda t: angd(t["azimuth_deg"], az))
        txt = c[0]["text"] if c else ""
        if c:
            used.add(c[0]["speaker"])
        ref = dry_ref(d, name, v)
        sd = sd_in = None
        if ref is not None:
            sd_in = sdr(ref, sf.read(os.path.join(d, "mic2.wav"), dtype="float32")[0])
            if c:
                sd = sdr(ref, sf.read(os.path.join(out, c[0]["wav"]), dtype="float32")[0])
        res.append({"voice": v, "az": az, "est": c[0]["azimuth_deg"] if c else None, "wer": wer(REF[v], txt), "text": txt,
                    "sdr": sd, "sdr_in": sd_in})
    extra_txt = [t["text"] for t in tr if t["speaker"] not in used and t["text"].strip()]
    m = re.search(r"per-frame ms: mean ([\d.]+)", r.stderr)
    return name, {"talkers": res, "extra_text_streams": len(extra_txt), "frame_ms": float(m.group(1)) if m else None}


def main():
    tag, extra = sys.argv[1], sys.argv[2:]
    os.makedirs(os.path.join(TD, "bench"), exist_ok=True)
    only = os.environ.get("SCENES")
    names = [n for n in SCENES if not only or n in only.split(",")]
    with ThreadPoolExecutor(int(os.environ.get("JOBS", "4"))) as ex:
        results = dict(ex.map(lambda n: run(n, tag, extra), names))
    allw, base, four, azs, sds, dsd = [], [], [], [], [], []
    for n in names:
        r = results[n]
        if "error" in r:
            print(f"{n:18s} FAILED {r['error']}")
            continue
        cells = []
        for t in r["talkers"]:
            allw.append(t["wer"])
            (four if n.startswith("factory4") else base).append(t["wer"])
            if t["est"] is not None:
                azs.append(angd(t["est"], t["az"]))
            if t.get("sdr") is not None:
                sds.append(t["sdr"])
                dsd.append(t["sdr"] - t["sdr_in"])
            sd = f" {t['sdr']:+.1f}dB" if t.get("sdr") is not None else ""
            cells.append(f"{t['voice']}@{t['az']:.0f}->{'%.0f' % t['est'] if t['est'] is not None else 'MISS'} {t['wer']:.2f}{sd}")
        print(f"{n:18s} {' | '.join(cells)} | extra {r['extra_text_streams']}")
    mean = lambda v: sum(v) / max(len(v), 1)
    summ = {"tag": tag, "args": extra, "ego": bool(os.environ.get("EGO")), "mean_wer": mean(allw), "mean_wer_7scenes": mean(base),
            "mean_wer_factory4": mean(four), "talkers": len(allw), "mean_az_err": mean(azs), "missed": len(allw) - len(azs),
            "exact": sum(w == 0 for w in allw), "mean_sdr": mean(sds), "mean_sdr_gain": mean(dsd), "scenes": results}
    print(f"== {tag}: mean WER {summ['mean_wer']:.3f} (7 scenes {summ['mean_wer_7scenes']:.3f}, factory4 {summ['mean_wer_factory4']:.3f}) "
          f"| exact {summ['exact']}/{len(allw)} | SDR {summ['mean_sdr']:+.2f} dB (gain {summ['mean_sdr_gain']:+.2f}) | az err {summ['mean_az_err']:.1f}° | missed {summ['missed']}")
    json.dump(summ, open(os.path.join(TD, "bench", f"bench_{tag}.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
