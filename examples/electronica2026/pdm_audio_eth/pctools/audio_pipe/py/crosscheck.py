#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Cross-check the Rust stages against reference Python implementations.

  py/.venv/bin/python py/crosscheck.py <scene dir with mic1..8.wav [+ settings.json]> [--mics-off 1]

1. WPE: nara_wpe offline (iterative, batch) and online (RLS, same recursion as wpe.rs) on the same STFT grid,
   vs. the Rust output (`audio_pipe run --no-asr` -> wpe_mic1.wav). Scored by whisper WER on the dereverberated
   mic (via `audio_pipe asr`) and by the late-reverb energy removed.
2. DOA: pyroomacoustics NormMUSIC and SRP-PHAT over each talker's active time vs. the Rust tracker's azimuths and
   the ground truth in settings.json (Scene Simulator renders).
"""
import argparse
import json
import os
import re
import subprocess
import sys
import tempfile

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, "..", "target", "release", "audio_pipe")
FS, N, H = 16000, 512, 128
REF = {"male": "hi there how are you doing today", "female": "i'm good what is the plan for the day"}


def norm(t):
    t = t.lower().replace("what's", "what is")
    return re.sub(r"[^a-z' ]", " ", t).split()


def wer(ref, hyp):
    r, h = norm(ref), norm(hyp)
    d = list(range(len(h) + 1))
    for i in range(1, len(r) + 1):
        nd = [i] + [0] * len(h)
        for j in range(1, len(h) + 1):
            nd[j] = min(d[j] + 1, nd[j - 1] + 1, d[j - 1] + (r[i - 1] != h[j - 1]))
        d = nd
    return d[-1] / max(len(r), 1)


def asr(paths):
    out = subprocess.run([BIN, "asr", *paths], capture_output=True, text=True, check=True).stdout
    return [line.split("|", 1)[1].strip() for line in out.strip().splitlines()]


def stft(x):  # x [M, T] -> [F, M, frames], same grid as the Rust STFT (sqrt-Hann, 512/128)
    from nara_wpe.utils import stft as nstft

    return nstft(x, size=N, shift=H, window=lambda n: np.sqrt(np.hanning(n + 1)[:n])).transpose(2, 0, 1)


def istft(Y):  # [F, frames] -> [T]
    from nara_wpe.utils import istft as nistft

    return nistft(Y.T, size=N, shift=H, window=lambda n: np.sqrt(np.hanning(n + 1)[:n]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("scene")
    ap.add_argument("--mics-off", default="")
    ap.add_argument("--radius", type=float, default=0.05)
    a = ap.parse_args()
    off = {int(m) - 1 for m in a.mics_off.split(",") if m}
    X = np.stack([sf.read(os.path.join(a.scene, f"mic{m}.wav"), dtype="float32")[0] for m in range(1, 9)])
    act = [m for m in range(8) if m not in off]
    ref_mic = 1 if 1 in act else act[0]  # Rust's wpe_mic1.wav is mic 2
    settings = json.load(open(os.path.join(a.scene, "settings.json"))) if os.path.exists(os.path.join(a.scene, "settings.json")) else None
    tmp = tempfile.mkdtemp(prefix="crosscheck_")

    # ---------------- 1. WPE
    from nara_wpe.wpe import online_wpe_step, get_power_online, OnlineWPE, wpe  # noqa: F401

    Y = stft(X[act])  # [F, M', T']
    files = {}
    sf.write(os.path.join(tmp, "raw.wav"), X[ref_mic], FS)
    files["raw mic"] = os.path.join(tmp, "raw.wav")
    Z = wpe(Y, taps=10, delay=3, iterations=3, statistics_mode="full")
    sf.write(os.path.join(tmp, "nara_offline.wav"), istft(Z[:, act.index(ref_mic)]), FS)
    files["nara_wpe offline K=10 D=3 x3"] = os.path.join(tmp, "nara_offline.wav")
    for taps, delay, alpha in [(10, 3, 0.9999), (5, 2, 0.999)]:
        Fb, Mb, T = Y.shape
        Yt = Y.transpose(2, 0, 1)  # [T, F, M]
        o = OnlineWPE(taps=taps, delay=delay, alpha=alpha, frequency_bins=Fb, channel=Mb)
        out = np.zeros_like(Yt)
        out[: taps + delay] = Yt[: taps + delay]
        for t in range(taps + delay, T):
            out[t] = o.step_frame(Yt[t - taps - delay : t + 1])
        name = f"nara_online_K{taps}_D{delay}"
        sf.write(os.path.join(tmp, name + ".wav"), istft(out[:, :, act.index(ref_mic)].T), FS)
        files[f"nara_wpe online K={taps} D={delay} a={alpha}"] = os.path.join(tmp, name + ".wav")
    rust_out = os.path.join(tmp, "rust")
    extra = ["--mics-off", a.mics_off] if a.mics_off else []
    for taps, delay, alpha in [(5, 2, 0.999), (10, 3, 0.9999)]:
        d = f"{rust_out}_K{taps}"
        subprocess.run([BIN, "run", a.scene, "--out", d, "--no-asr", "--wpe-taps", str(taps), "--wpe-delay", str(delay), "--wpe-alpha", str(alpha), *extra], capture_output=True, check=True)
        files[f"Rust wpe.rs K={taps} D={delay} a={alpha}"] = os.path.join(d, "wpe_mic1.wav")
    texts = asr(list(files.values()))
    refs = []
    if settings:
        refs = [REF[s["voice"]] for s in sorted(settings["speakers"], key=lambda s: s["start"]) if s["voice"] in REF]
    full_ref = " ".join(refs)
    raw = sf.read(files["raw mic"])[0]
    print("\n== WPE: whisper on the dereverberated mic", ref_mic + 1, "(both talkers in one transcript)")
    for (k, p), txt in zip(files.items(), texts):
        y = sf.read(p)[0]
        n = min(len(y), len(raw))
        red = 10 * np.log10(np.sum(y[:n] ** 2) / np.sum(raw[:n] ** 2))
        w = f"WER {wer(full_ref, txt):.2f}" if full_ref else ""
        print(f"  {k:34s} energy {red:+5.1f} dB  {w}  | {txt}")

    # ---------------- 2. DOA
    if not settings:
        return
    import pyroomacoustics as pra

    ang = 2 * np.pi * np.arange(8) / 8
    R = np.stack([a.radius * np.cos(ang), a.radius * np.sin(ang)])[:, act]
    Xs = stft(X[act]).transpose(1, 0, 2)  # [M, F, T]
    rust = json.load(open(os.path.join(f"{rust_out}_K5", "transcript.json")))
    print("\n== DOA (azimuth, deg): truth vs pyroomacoustics vs Rust tracker")
    for s in sorted(settings["speakers"], key=lambda s: s["start"]):
        t0 = int((s["start"] + 0.1) * FS / H)
        t1 = min(Xs.shape[2], t0 + int(1.8 * FS / H))
        seg = Xs[:, :, t0:t1]
        res = []
        for alg in ["NormMUSIC", "SRP"]:
            d = pra.doa.algorithms[alg](R, FS, N, c=343.0, num_src=1, azimuth=np.deg2rad(np.arange(0, 360, 1.0)))
            d.locate_sources(seg, freq_range=[300.0, 3500.0])
            res.append(f"{alg} {np.rad2deg(d.azimuth_recon[0]) % 360:5.1f}")
        near = min(rust, key=lambda r: abs((r["azimuth_deg"] - s["az"] + 180) % 360 - 180)) if rust else None
        rs = f"Rust {near['azimuth_deg']:5.1f}" if near else "Rust -"
        print(f"  {s['voice']:6s} truth {s['az']:5.1f}   {'   '.join(res)}   {rs}")


if __name__ == "__main__":
    sys.exit(main())
