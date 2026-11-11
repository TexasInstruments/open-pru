#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""HARK-style localization baseline (LocalizeMUSIC): SEVD-MUSIC and GEVD-MUSIC with a pre-recorded noise matrix.

  py/.venv/bin/python py/hark_music.py [bench tag for the Rust azimuths, default: base]

HARK itself (apt packages for Ubuntu 20.04/22.04, root needed) cannot be installed on this machine, so this
re-implements its localization as documented:
  * R(f)  = mean x xᴴ over a 50-frame window (HARK WINDOW=50), bins 500-2800 Hz (LOWER/UPPER_BOUND_FREQUENCY)
  * SEVD-MUSIC: eigenvectors of R;  GEVD-MUSIC: generalized eigenvectors of (R, K), K = noise correlation matrix,
    here the robot-only take from tools/make_ego_takes.sh (what HARK users record with the robot running alone)
  * P(θ,f) = |dᴴd| / Σ_{i>Ns} |dᴴe_i|²,  broadband P(θ) = Σ_f sqrt(λ_1(f)) P(θ,f)  (HARK's eigenvalue weighting)
  * NUM_SOURCE Ns = 2
Per talker, the window covers 0.1-1.7 s after its start (as py/crosscheck.py). Scores: error of the global peak, and
whether the truth is within 10° of one of the top-K peaks (K = talkers in the scene). Compared with the Rust
tracker's azimuth of the stream matched to that talker in tools/bench.py results.
"""
import json, os, sys
import numpy as np
import soundfile as sf
from scipy.linalg import eigh
from scipy.signal import stft

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
import bench  # noqa: E402  (scene list, talkers, test-data paths)

FS, N, H, C = 16000, 512, 128, 343.0
GRID = np.arange(0, 360, 1.0)


def spec(x):  # [M, T] -> [F, M, frames]
    win = np.sqrt(np.hanning(N + 1)[:N])
    _, _, Z = stft(x, FS, window=win, nperseg=N, noverlap=N - H, boundary=None, padded=False)
    return Z.transpose(1, 0, 2)


def steering(pos, freqs):  # [F, A, M]; plane wave from azimuth θ arrives earlier at mics on that side
    u = np.stack([np.cos(np.deg2rad(GRID)), np.sin(np.deg2rad(GRID))], 1)  # [A, 2]
    tau = u @ pos[:, :2].T / C  # [A, M]
    return np.exp(2j * np.pi * freqs[:, None, None] * tau[None])


def music(X, K, A, ns=2):
    """X [F, M, T] window; K [F, M, M] or None; A [F, Az, M] -> broadband pseudo-spectrum [Az]."""
    P = np.zeros(A.shape[1])
    for f in range(X.shape[0]):
        x = X[f]
        R = x @ x.conj().T / x.shape[1]
        if K is None:
            lam, E = eigh(R)
        else:
            Kf = K[f] + 1e-3 * np.trace(K[f]).real / K.shape[1] * np.eye(K.shape[1])
            lam, E = eigh(R, Kf)
        lam, E = lam[::-1], E[:, ::-1]
        En = E[:, ns:]
        d = A[f]  # [Az, M]
        num = np.sum(np.abs(d) ** 2, 1)
        den = np.sum(np.abs(d.conj() @ En) ** 2, 1) + 1e-12
        P += np.sqrt(max(lam[0], 0)) * num / den
    return P


def peaks(P, k, min_sep=20):
    order = np.argsort(P)[::-1]
    out = []
    for i in order:
        if all(abs((GRID[i] - GRID[j] + 180) % 360 - 180) >= min_sep for j in out):
            out.append(i)
        if len(out) == k:
            break
    return [GRID[i] for i in out]


def main():
    tag = sys.argv[1] if len(sys.argv) > 1 else "base"
    rust = json.load(open(os.path.join(bench.TD, "bench", f"bench_{tag}.json")))["scenes"]
    rows = {"SEVD-MUSIC": [], "GEVD-MUSIC (K = robot-only take)": [], "Rust whitened votes + tracker": []}
    for name, (d, ego, _) in bench.SCENES.items():
        sdir = os.path.join(bench.TD, "scene_home") if name == "example" else d
        st = json.load(open(os.path.join(sdir, "settings.json")))
        pos = np.array(st["micPositions"], float)
        X = spec(np.stack([sf.read(os.path.join(d, f"mic{m}.wav"), dtype="float64")[0] for m in range(1, 9)]))
        Xe = spec(np.stack([sf.read(os.path.join(ego, f"mic{m}.wav"), dtype="float64")[0] for m in range(1, 9)]))
        freqs = np.arange(X.shape[0]) * FS / N
        band = (freqs >= 500) & (freqs <= 2800)
        X, Xe, A = X[band], Xe[band], steering(pos, freqs[band])
        if name == "example":  # mic1 is from another recording: use mics 2-8 (as the health check does)
            X, Xe, A = X[:, 1:], Xe[:, 1:], A[:, :, 1:]
        K = np.einsum("fmt,fnt->fmn", Xe, Xe.conj()) / Xe.shape[2]
        tk = bench.talkers(name, d)
        starts = {sp["voice"]: sp["start"] for sp in st["speakers"]}
        line = []
        for i, (v, az) in enumerate(tk):
            t0 = int((starts[v] + 0.1) * FS / H)
            t1 = min(X.shape[2], t0 + int(1.6 * FS / H))
            res = []
            for key, Km in (("SEVD-MUSIC", None), ("GEVD-MUSIC (K = robot-only take)", K)):
                P = music(X[:, :, t0:t1], Km, A)
                pk = peaks(P, len(tk))
                err = bench.angd(pk[0], az)
                hit = min(bench.angd(p, az) for p in pk) <= 10
                rows[key].append((err, hit))
                res.append(f"{pk[0]:.0f}")
            est = rust[name]["talkers"][i]["est"]
            rows["Rust whitened votes + tracker"].append((bench.angd(est, az) if est is not None else 180.0, est is not None and bench.angd(est, az) <= 10))
            line.append(f"{v}@{az:.0f}: SEVD {res[0]}, GEVD {res[1]}, Rust {est if est is None else round(est)}")
        print(f"{name:18s} " + " | ".join(line))
    print()
    for k, v in rows.items():
        e = np.array([x[0] for x in v])
        print(f"{k:32s} median err {np.median(e):5.1f}°  mean err {e.mean():5.1f}°  within 10° (top-K peaks) {sum(x[1] for x in v)}/{len(v)}")


if __name__ == "__main__":
    main()
