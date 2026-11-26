#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/
"""Neural mask sidecar for audio_pipe (SpeechBrain SepFormer, WHAMR! 16 kHz, on CUDA).

Rust spawns this once and talks binary over stdin/stdout (little-endian):

  request : b"MASK" u32 n_ref  u32 n_bf  f32[n_ref] ref  f32[n_bf] bf
            ref = single-channel mixture (WPE'd mic), sample 0 = start of Rust frame t0's window
            bf  = this speaker's streaming beamformer output over the same span (permutation anchor)
  response: b"MASK" u32 n_frames u32 F f32 score  f32[n_frames*F] mask  u32 n_sep f32[n_sep] sep
            frame j <-> Rust frame t0+j (window ref[jH : jH+N]); mask = Wiener-style ratio of the chosen
            source's power to the mixture power; sep = the chosen separated waveform (scaled to the mixture)
  b"QUIT" -> exit.   Startup prints b"READY\\n" on stdout once the model is loaded and warmed up.
"""
import os
import struct
import sys
import time

import numpy as np
import torch

N, H = 512, 128
F = N // 2 + 1
MODEL = os.environ.get("AUDIO_PIPE_SEP_MODEL", "speechbrain/sepformer-whamr16k")
CACHE = os.environ.get("AUDIO_PIPE_SEP_CACHE", os.path.join(os.path.dirname(os.path.abspath(__file__)), "pretrained"))


def log(*a):
    print("[mask_server]", *a, file=sys.stderr, flush=True)


def load_model():
    from speechbrain.inference.separation import SepformerSeparation

    dev = "cuda:0" if torch.cuda.is_available() else "cpu"
    m = SepformerSeparation.from_hparams(source=MODEL, savedir=os.path.join(CACHE, MODEL.split("/")[-1]), run_opts={"device": dev})
    m.eval()
    return m, dev


WIN = torch.sqrt(torch.hann_window(N, periodic=True))


def stft(x):  # x: [T] -> [frames, F], frames of x[jH : jH+N] (no centring, same grid as Rust)
    n = (x.shape[-1] - N) // H + 1
    if n <= 0:
        return torch.zeros(0, F, dtype=torch.complex64, device=x.device)
    fr = x.unfold(-1, N, H)[:n] * WIN.to(x.device)
    return torch.fft.rfft(fr, n=N)


@torch.inference_mode()
def separate(model, dev, ref, bf):
    x = torch.from_numpy(ref).to(dev)
    est = model.separate_batch(x[None])[0].T  # [2, T]
    est = est[:, : x.shape[0]]
    if est.shape[1] < x.shape[0]:
        est = torch.nn.functional.pad(est, (0, x.shape[0] - est.shape[1]))
    # least-squares scale of each source to the mixture (SepFormer output scale is arbitrary)
    A = est.T  # [T, 2]
    alpha = torch.linalg.lstsq(A, x[:, None]).solution[:, 0]
    est = est * alpha[:, None]
    X = stft(x)
    S = torch.stack([stft(e) for e in est])  # [2, frames, F]
    R = X - S.sum(0)  # residual = noise + reverb the model removed
    ps = S.abs() ** 2
    den = ps.sum(0) + R.abs() ** 2 + 1e-10
    masks = (ps / den).clamp(0, 1)  # [2, frames, F]
    # permutation: which separated source is this stream's talker? correlate magnitude spectrograms
    B = stft(torch.from_numpy(bf).to(dev)) if bf.size >= N else None
    score = torch.zeros(2)
    if B is not None and B.shape[0] > 0:
        n = min(B.shape[0], S.shape[1])
        b = B[:n].abs().log1p().flatten()
        b = (b - b.mean()) / (b.std() + 1e-9)
        for k in range(2):
            s = S[k, :n].abs().log1p().flatten()
            s = (s - s.mean()) / (s.std() + 1e-9)
            score[k] = (s * b).mean()
    k = int(torch.argmax(score))
    return masks[k].float().cpu().numpy(), float(score[k]), est[k].float().cpu().numpy()


def read_exact(f, n):
    b = bytearray()
    while len(b) < n:
        c = f.read(n - len(b))
        if not c:
            raise EOFError
        b += c
    return bytes(b)


def main():
    t0 = time.time()
    model, dev = load_model()
    # warm-up (cuDNN autotune, allocator)
    separate(model, dev, np.zeros(16000, np.float32), np.zeros(16000, np.float32))
    log(f"{MODEL} on {dev} ({torch.cuda.get_device_name(0) if dev.startswith("cuda") else 'cpu'}) ready in {time.time() - t0:.1f} s")
    fin, fout = sys.stdin.buffer, sys.stdout.buffer
    fout.write(b"READY\n")
    fout.flush()
    while True:
        try:
            magic = read_exact(fin, 4)
        except EOFError:
            return
        if magic == b"QUIT":
            return
        if magic != b"MASK":
            log("bad magic", magic)
            return
        n_ref, n_bf = struct.unpack("<II", read_exact(fin, 8))
        ref = np.frombuffer(read_exact(fin, 4 * n_ref), dtype="<f4").copy()
        bf = np.frombuffer(read_exact(fin, 4 * n_bf), dtype="<f4").copy()
        t = time.time()
        mask, score, sep = separate(model, dev, ref, bf)
        if os.environ.get("AUDIO_PIPE_SEP_DEBUG"):
            log(f"{n_ref / 16000:.2f} s -> {mask.shape[0]} frames, score {score:.2f}, {1e3 * (time.time() - t):.0f} ms")
        fout.write(b"MASK" + struct.pack("<IIf", mask.shape[0], F, score))
        fout.write(np.ascontiguousarray(mask, dtype="<f4").tobytes())
        fout.write(struct.pack("<I", sep.shape[0]))
        fout.write(np.ascontiguousarray(sep, dtype="<f4").tobytes())
        fout.flush()


if __name__ == "__main__":
    main()
