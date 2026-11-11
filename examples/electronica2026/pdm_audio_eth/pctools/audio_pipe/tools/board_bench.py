#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Run scenes through the LP-AM243 PDM chain (pdm_audio_eth) and build board versions of them for tools/bench.py.

  py/.venv/bin/python tools/board_bench.py [--emulator] [--iface enp129s0] [scene ...]   (default: the 4 factory4 scenes)

Per scene <d> = $SCENE_ROOT/audio_wav_<scene> (default: the folder that holds this crate):
  1. pdm-eth stream --set --ch0 mic1.wav .. --ch7 mic8.wav --export-shm NAME --capture <d>_board/capture
     (--set: one gain for the 8 mics, so their level differences survive; the board input is int16, peak 0.5 FS)
  2. this script reads /dev/shm/NAME while it runs: aligned 64-frame blocks of board input (int16) and output (int32,
     24-bit), the processing seam of pc_app/src/seam.rs
  3. <d>_board/mic1..8.wav    = board output, file-aligned (offset found on the exact int16 input copy), one
                                least-squares gain for all 8 channels back to the scene level, float32
     <d>_boardin/mic1..8.wav  = the int16 board input at the same gain (quantisation only, no PDM)
     settings.json copied, dry/ and ego/ linked (SDR references, robot-only take)
  4. per-channel delay and gain of output against input (cross-spectrum phase slope, 200-4000 Hz): the
     inter-channel sync check (one 32 kHz sample = 31 us would look like a 1 cm mic shift)
Then: BOARD=_board tools/bench.py board   and   BOARD=_boardin tools/bench.py boardin   (SCENES=factory4,...).
"""
import argparse, json, mmap, os, shutil, struct, subprocess, sys, threading, time
import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from paths import SCENE_ROOT as UP  # folder with the audio_wav* scenes
PDM = os.environ.get("PDM_ETH", os.path.expanduser(
    "~/GitHub/open-pru/examples/electronica2026/pdm_audio_eth/pc_app/target/release/pdm-eth"))
FS = 16000
BLOCK = 8 + 64 * 8 * 2 + 64 * 8 * 4  # seam.rs BLOCK_BYTES


def read_shm(name, res, stop, wait_s=20.0):
    """Collect every block of /dev/shm/<name> into res['blocks'] = [(index, in[64,8] i16, out[64,8] i32)]."""
    path = f"/dev/shm/{name}"
    t0 = time.time()
    while True:
        try:
            f = open(path, "rb")
            hdr = f.read(64)
            if len(hdr) == 64 and hdr[:8] == b"PDMSHM01":
                break
            f.close()
        except FileNotFoundError:
            pass
        if time.time() - t0 > wait_s or stop.is_set():
            res["error"] = f"{path} did not appear"
            return
        time.sleep(0.01)
    n, bsz = struct.unpack_from("<II", hdr, 8)
    assert bsz == BLOCK, (bsz, BLOCK)
    m = mmap.mmap(f.fileno(), 64 + n * bsz, prot=mmap.PROT_READ)  # stays valid after pdm-eth unlinks the file
    blocks, nxt, overrun = [], 0, 0
    while True:
        w = struct.unpack_from("<Q", m, 16)[0]
        if w - nxt > n:  # the writer lapped us
            overrun += w - nxt - n
            nxt = w - n
        while nxt < w:
            off = 64 + (nxt % n) * bsz
            raw = m[off:off + bsz]
            idx = struct.unpack_from("<Q", raw, 0)[0]
            x = np.frombuffer(raw, "<i2", 512, 8).reshape(64, 8).copy()
            y = np.frombuffer(raw, "<i4", 512, 8 + 1024).reshape(64, 8).copy()
            if struct.unpack_from("<Q", m, 16)[0] - nxt <= n:  # not overwritten while copying
                blocks.append((idx, x, y))
            else:
                overrun += 1
            nxt += 1
        if stop.is_set() and nxt >= struct.unpack_from("<Q", m, 16)[0]:
            break
        time.sleep(0.005)
    res.update(blocks=blocks, overrun=overrun)


def assemble(blocks):
    """Blocks -> contiguous (first index, in [T,8], out [T,8], gap frames); gaps (skipped blocks) stay zero."""
    i0 = blocks[0][0]
    T = blocks[-1][0] + 64 - i0
    X, Y = np.zeros((T, 8), np.int16), np.zeros((T, 8), np.int32)
    have = np.zeros(T, bool)
    for idx, x, y in blocks:
        X[idx - i0:idx - i0 + 64], Y[idx - i0:idx - i0 + 64] = x, y
        have[idx - i0:idx - i0 + 64] = True
    return i0, X, Y, int((~have).sum())


def file_offset(xin, mic):
    """Shift s with xin[s + p] = gain * mic[p] (xin is the int16 copy of the file): cross-correlation, then exact check."""
    n = len(mic)
    L = 1 << int(np.ceil(np.log2(len(xin) + n)))
    c = np.fft.irfft(np.fft.rfft(xin.astype(float), L) * np.conj(np.fft.rfft(mic, L)), L)
    lim = len(xin) - n // 2
    s = int(np.argmax(c[:max(lim, 1)]))
    return s


def chan_delay(x, y, lo=200, hi=4000, nfft=1024):
    """Delay (samples) and gain (dB) of y against x: Welch cross-spectrum, weighted phase-slope fit."""
    from scipy.signal import csd, welch
    f, Pxy = csd(x, y, FS, nperseg=nfft)
    _, Pxx = welch(x, FS, nperseg=nfft)
    b = (f >= lo) & (f <= hi)
    ph = np.unwrap(np.angle(Pxy[b]))
    w = np.abs(Pxy[b])
    k = np.polyfit(2 * np.pi * f[b] / FS, ph, 1, w=w)[0]
    g = np.sum(np.abs(Pxy[b])) / np.sum(Pxx[b])
    return -k, 20 * np.log10(g)


def run_scene(scene, args):
    d = os.path.join(UP, f"audio_wav_{scene}")
    mics = [os.path.join(d, f"mic{m}.wav") for m in range(1, 9)]
    orig = np.stack([sf.read(p, dtype="float64")[0] for p in mics], 1)  # [T, 8]
    out_dir, in_dir = d + "_board", d + "_boardin"
    name = f"audio_pipe_{scene}_{os.getpid()}"
    for od in (out_dir, in_dir):
        shutil.rmtree(od, ignore_errors=True)
        os.makedirs(od)
    cap = os.path.join(out_dir, "capture")
    tgt = ["--emulator"] if args.emulator else ["--iface", args.iface]
    chans = sum([[f"--ch{c}", mics[c]] for c in range(8)], [])
    c0 = int(np.argmax((orig ** 2).sum(0)))  # loudest mic: offsets are found on it
    if not args.legacy:
        res, stop = {}, threading.Event()
        th = threading.Thread(target=read_shm, args=(name, res, stop))
        th.start()
        r = subprocess.run([args.pdm, "stream", *tgt, "--set", *chans, "--export-shm", name, "--capture", cap],
                           capture_output=True, text=True)
        stop.set()
        th.join()
        log = (r.stdout + r.stderr).strip()
        if r.returncode or "error" in res or not res.get("blocks"):
            sys.exit(f"{scene}: pdm-eth failed ({r.returncode}): {log}\n{res.get('error', '')}")
        i0, X, Y, gaps = assemble(res["blocks"])
        overrun = res["overrun"]
        s = file_offset(X[:, c0].astype(float), orig[:, c0])  # file sample p sits at X[s + p] and Y[s + p]
        T = min(len(orig), len(X) - s)
        xin, yo = X[s:s + T].astype(float), Y[s:s + T].astype(float)
    else:
        # pdm-eth without --set / --export-shm: every file was scaled to 0.5 FS on its own (undone below per
        # channel from the exact int16 copy in in_ch*.wav), the output lag is found here by correlation
        r = subprocess.run([args.pdm, "stream", *tgt, *chans, "--capture", cap], capture_output=True, text=True)
        log = (r.stdout + r.stderr).strip()
        if r.returncode:
            sys.exit(f"{scene}: pdm-eth failed ({r.returncode}): {log}")
        X = np.stack([sf.read(os.path.join(cap, f"in_ch{c}.wav"), dtype="int16")[0] for c in range(8)], 1)
        Y = np.stack([sf.read(os.path.join(cap, f"out_ch{c}.wav"), dtype="int32")[0] >> 8 for c in range(8)], 1)
        i0, gaps, overrun = None, None, None
        s = file_offset(X[:, c0].astype(float), orig[:, c0])
        T = min(len(orig), len(X) - s)
        xin = X[s:s + T].astype(float)
        o = file_offset(Y[:, c0].astype(float), xin[:, c0])  # output sample o + p carries input sample p
        T = min(T, len(Y) - o)
        xin, yo = xin[:T], Y[o:o + T].astype(float)
        log += f"\nlag found offline: output wav sample {o} = input wav sample 0"
    # int16 counts per file unit, per channel (one value for all with --set); the input must be the file exactly
    a = np.array([np.sum(xin[:, c] * orig[:T, c]) / np.sum(orig[:T, c] ** 2) for c in range(8)])
    q_err = np.max(np.abs(xin - a * orig[:T]))
    assert q_err <= 1.01, f"{scene}: board input is not the file (max diff {q_err:.2f} LSB at offset {s})"
    xn, yn = xin / a, yo / a
    g = np.sum(yn * orig[:T]) / np.sum(yn ** 2)  # one least-squares gain, all channels, back to the file level
    for c in range(8):
        sf.write(os.path.join(out_dir, f"mic{c + 1}.wav"), (g * yn[:, c]).astype(np.float32), FS, subtype="FLOAT")
        sf.write(os.path.join(in_dir, f"mic{c + 1}.wav"), xn[:, c].astype(np.float32), FS, subtype="FLOAT")
    for od in (out_dir, in_dir):
        shutil.copy(os.path.join(d, "settings.json"), od)
        for sub in ("dry", "ego"):
            if os.path.isdir(os.path.join(d, sub)):
                os.symlink(os.path.join(d, sub), os.path.join(od, sub))
    dl = [chan_delay(xin[:, c], yo[:, c]) for c in range(8)]
    delays = np.array([v[0] for v in dl])
    gains = np.array([v[1] for v in dl])
    lag_line = next((l for l in log.splitlines() if l.startswith("lag")), "")
    out = {"scene": scene, "frames": T, "first_index": i0, "file_offset": int(s), "gap_frames": gaps,
           "overrun_blocks": overrun, "input_gain_counts": a.tolist(), "output_gain_to_file": g,
           "delay_samples": delays.tolist(), "delay_spread_us": float((delays.max() - delays.min()) / FS * 1e6),
           "gain_db": gains.tolist(), "gain_spread_db": float(gains.max() - gains.min()), "pdm_eth": log}
    json.dump(out, open(os.path.join(out_dir, "board_run.json"), "w"), indent=1)
    print(f"{scene:18s} {T / FS:5.2f} s, gaps {gaps}, overrun {overrun}, {lag_line}")
    print(f"{'':18s} delay out-in per mic [samples]: {' '.join(f'{v:+.3f}' for v in delays)}  spread {out['delay_spread_us']:.2f} us")
    print(f"{'':18s} gain per mic [dB]:              {' '.join(f'{v:+.3f}' for v in gains)}  spread {out['gain_spread_db']:.3f} dB")
    for line in log.splitlines():
        if line.startswith(("sent", "output lost", "exported", "lag found")):
            print(f"{'':18s} {line}")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--emulator", action="store_true")
    ap.add_argument("--iface", default="enp129s0")
    ap.add_argument("--pdm", default=PDM, help="pdm-eth binary (needs cap_net_raw,cap_net_admin unless --emulator)")
    ap.add_argument("--legacy", action="store_true", help="pdm-eth without --set/--export-shm: align from --capture")
    ap.add_argument("scenes", nargs="*", default=["factory4", "factory4_n10", "factory4_dense", "factory4_dense240"])
    args = ap.parse_args()
    for s in args.scenes:
        run_scene(s, args)


if __name__ == "__main__":
    main()
