# audio_pipe: 8-mic front-end to per-speaker ASR (Rust + CUDA)

This is a streaming implementation of the 8-mic pre-processing pipe of the humanoid voice-capture implementation
note (rev B): you give it the 8 ring-mic WAVs and get one audio stream and one transcript per talker.

It is the PC-side consumer of the [pdm_audio_eth](../../pdm_audio_eth/readme.md) demo. The Scene Simulator renders a
scene to 8 mic WAVs, `pdm-eth` ([../../pc_app](../../pc_app/README.md)) streams them through the LP-AM243 PDM chain,
and `audio_pipe` runs on what comes back (see "Through the board" below).

Scenes and the Scene Simulator are not in this repository. The tools look for them in `SCENE_ROOT` (default: the
folder that holds this crate): `$SCENE_ROOT/audio_wav*/` are scenes (mic1..8.wav + settings.json), and
`$SCENE_ROOT/scenes/web/` is the Scene Simulator checkout that `tools/render_mics.js` runs.

```
mic1..8.wav ─► health check ─► STFT 512/128 ─► online WPE ─► Φn tracking + multichannel SPP (ego noise)
   ─► whitened DOA voting + tracker ─► spatial masks ─► mask-MVDR per talker ─► Wiener post-filter ─► iSTFT
   ─► endpointer ─► whisper.cpp on CUDA (worker thread) ─► text per talker
```

## Build and run

```
export PATH=/usr/local/cuda/bin:$PATH CMAKE_CUDA_ARCHITECTURES=120   # CUDA 13.2, RTX PRO 4000 Blackwell
cargo build --release
# models (once):  models/ggml-large-v3-turbo-q5_0.bin, models/ggml-base.en.bin  (huggingface ggerganov/whisper.cpp)

./target/release/audio_pipe run $SCENE_ROOT/audio_wav --out out                 # as fast as possible
./target/release/audio_pipe run $SCENE_ROOT/audio_wav --out out --realtime      # paced like a live mic: true latency
./target/release/audio_pipe run $SCENE_ROOT/audio_wav --out out --realtime --hang-ms 200 --partial-ms 400   # lowest latency
./target/release/audio_pipe asr some.wav                                # plain whisper on one file (reference)
```

Outputs in `--out`:
- `speaker<N>_az<deg>.wav`: one per detected source, aligned to the input timeline.
- `transcript.json`: text and segments per speaker.
- `wpe_mic1.wav`: mic 2 after WPE, for diagnostics.
- `activity.csv`: written with `-v`.

## What each stage does (and where it departs from the note)

| Note section | Here | Notes |
|---|---|---|
| PDM→PCM | not needed | input is already 16 kHz PCM |
| STFT (sec. 3) | `stft.rs` | sqrt-Hann, N=512, H=128, OLA × 1/2; perfect-reconstruction unit test |
| Ego-noise NMF (sec. 4) | `noise.rs` | **substitute**: there is no motor state θ(t) offline, so ego noise and room noise are tracked as a noise covariance Φn, weighted by speech presence. It drives a multichannel SPP (Souden), whitens the DOA search and masks, and is what the MVDR nulls |
| WPE (sec. 5) | `wpe.rs` | RLS form, bin-parallel on rayon. K=5, Δ=2, α=0.999. λ comes from the observed power: with λ = \|x̂\|² and α=0.99 the predictor overfit and *added* energy, which made the speech unrecognisable |
| Localization + tracking (sec. 6) | `doa.rs` | Per-bin DOA in the noise-whitened domain, with a direct-path coherence test, votes into an azimuth histogram. A gated tracker confirms a source on 2 strong hits. Planar ring, so azimuth only |
| Neural masks on NPU (sec. 7) | `sep.rs::posteriors` | **substitute**: no trained net in the repo, so causal spatial-clustering masks are used (Watson-like posterior over tracked talkers plus a background class, times SPP). Same interface, M_c(t,f) ∈ [0,1] |
| Mask MVDR (sec. 8) | `sep.rs` | Φ_in for each talker is Φn plus the other talkers' Φ_c; weights re-solved every 4 frames, diagonal loading 2 % |
| Post-filter + iSTFT (sec. 9) | `sep.rs` | Decision-directed Wiener, floor 0.15, plus a spatial post-mask, floor 0.25 |
| → ASR | `asr.rs` | whisper large-v3-turbo q5_0 on CUDA in its own thread. Endpointer per stream; a hypothesis is rejected if its no-speech probability is > 0.6 or its mean token probability is < 0.35. This removes the hallucinated "Thank you." from noise streams |

Extras not in the note:
- **Channel health check** (`health.rs`): a mic incoherent with both neighbours over the first 300 ms is switched off. The array then runs on M−1 mics. `--mics-off 1` does the same by hand.
- **Replay on detection**: when a new talker is confirmed, the last 2.5 s are re-run through that talker's beamformer. Speech spoken before the tracker was sure is therefore not lost.

## The supplied example (`$SCENE_ROOT/audio_wav`)

**mic1 does not belong to the same recording as mic2–8.**
- mic2–8 are sample-identical to the Scene Simulator's *Home* scenario (male at 20°, female at 150°).
- mic1 resembles a *Factory* render instead (ρ ≈ 0.72).
- The health check finds this (adjacent coherence 0.15 against 0.74–0.89 elsewhere) and drops mic1.

Result (real-time paced, `--realtime`):

| talker | true | found | text | reference |
|---|---|---|---|---|
| male | 20° | 19° | "Hi there. How are you doing today?" | Hi there, how are you doing today? |
| female | 150° | 154° | "What's your morning for the day?" | I'm good. What is the plan for the day? |

There are also noise-source streams: door knocks at 257° (rejected) and the kitchen at 114° ("Welcome to the home of the day.", which is leaked female speech).

The female is the hard case. The fridge noise sits in the same direction as her (150°) and the fan is 30° away. On a 10 cm array that hides most of her spatial evidence: she is confirmed only at 4.55 s, and part of her speech goes to the 114° stream.

## Latency (measured, `--realtime`, this machine)

| source | value |
|---|---|
| STFT window / algorithmic (N−H) + hop | 24 ms + 8 ms |
| DSP per 8 ms frame | mean 0.7–1.0 ms, p99 ≈ 2.3 ms. A one-off 18–40 ms burst when a new talker is replayed (absorbed in a few frames) |
| endpoint hang (`--hang-ms`) | 320 ms default, 200 ms in low-latency mode |
| whisper decode on the GPU | 130–145 ms per utterance |
| **final text after the talker stops** | **≈ 460 ms** default, **≈ 340 ms** with `--hang-ms 200` |
| partial text while talking (`--partial-ms 400`) | ≈ 130–150 ms behind the audio |

The CPU handles the DSP, including WPE, at about 10 % of a real-time core budget. CUDA is used where it pays off, which is ASR.

### CPU and memory per stage

`--profile` prints the time and state memory of each stage and writes `<out>/profile.json`, which includes the mean time grouped by the number of streams. Add `RAYON_NUM_THREADS=1` for single-core numbers. Measured on the dense 4-talker factory scene (4 talkers + 1 noise stream), on one core of a Core Ultra 7 265F:

| stage | µs / 8 ms frame | state |
|---|---|---|
| STFT | 8 | 34 KiB |
| WPE (K=5) | 748 (138 on 18 threads) | 4.0 MiB |
| noise Φn + SPP | 50 | 261 KiB |
| DOA | 289 | 2.6 MiB |
| tracker + 2.5 s replay buffer | 48 (20 ms burst per new talker) | 10.1 MiB |
| masks / covariances / MVDR / post-filter, per talker | 2.5 / 7 / 24 / 14 | 17 / 130 / 32 / 13 KiB |
| **total** | **1380 (17 % of one core)** | **18 MiB** |

The GPU side is whisper large-v3-turbo q5_0, which takes 770 MiB VRAM and 128–139 ms per utterance. The optional SepFormer sidecar takes 972 MiB VRAM and 34–186 ms per utterance.

## Neural second pass (SpeechBrain) and Python cross-checks

Setup, once (≈3 GB; torch 2.11 cu128 for the Blackwell GPU):
```
cd py && uv venv --python 3.12 .venv && uv pip install --python .venv/bin/python torch torchaudio --index-url https://download.pytorch.org/whl/cu128 \
  && uv pip install --python .venv/bin/python speechbrain soundfile numpy scipy nara_wpe pyroomacoustics
```

`--neural mvdr|sep` starts `py/mask_server.py`, a persistent sidecar that talks binary over stdin/stdout and runs
`speechbrain/sepformer-whamr16k` on CUDA. It is used as a second pass on each finished utterance, so the partials stay
fast. The sidecar gets the dereverberated mic plus 0.5 s of context, and the talker's stream for solving the
permutation. It returns that talker's T-F mask and separated waveform (about 70–120 ms per utterance on the GPU).
- `mvdr` re-estimates the talker and interference covariances from the mask over the whole utterance, takes a GEV
  RTF, and runs MVDR on the 8-ch x̂. The mask is optionally also applied as a post-mask (`--neural-post-floor`).
- `sep` sends the separated waveform to ASR directly.

**The neural pass is off by default because it made results worse on this data.**

| configuration (7 test scenes, 14 talkers) | mean WER |
|---|---|
| `--neural off` (spatial pipeline only) | **0.336** |
| `--neural mvdr --neural-post-floor 1` (neural mask only for covariances) | 0.587 |
| `--neural mvdr` / `sep` on Factory | talker 1: "All right, how are you doing today?" / "All better. I need to learn to do this." |

Standalone checks agree with this:
- SepFormer-WHAMR on the raw or WPE'd mic does not separate these turn-taking, synthetically reverberated talkers
  (quiet scene: "Hi, Bear. How are you doing today? I'm... What's up for the day?").
- The SpeechBrain enhancers metricgan-plus-voicebank and sepformer-wham16k-enhancement, applied to the beamformed
  streams, lower the whisper accuracy on every stream tested.

Whisper is robust to residual noise but not to the artifacts these enhancers add. The pretrained models were trained
on overlapped WSJ mixtures and VoiceBank, which are far from a 5 cm robot array. The plumbing is in place for a
**model fine-tuned on Scene Simulator renders** (`tools/render_mics.js` can generate unlimited labelled array data).
That is the step that should make the neural mask pay off.

`py/crosscheck.py <scene>` compares the Rust stages with reference Python implementations:

| scene | raw mic WER | nara_wpe offline (batch) | nara_wpe online | Rust wpe.rs (K=5, D=2) |
|---|---|---|---|---|
| factory | 0.31 | 0.31 (−1.6 dB) | 1.00 (fails) | 0.31 (−2.1 dB) |
| home | 0.50 | 0.31 (−1.5 dB) | 0.44–1.00 | 0.31 (−2.0 dB) |
| quiet | 0.06 | 0.06 | 0.00–0.06 | 0.06 |

| DOA (truth) | pyroomacoustics NormMUSIC / SRP-PHAT | Rust (whitened votes + tracker) |
|---|---|---|
| factory male 30°, female 200° | 179° / 179°, 181° / 180° (locked on the fan) | 28.9°, 199.6° |
| home male 20°, female 150° | 180° / 180°, 178° / 180° (fan) | 20.6°, 156.3° |
| quiet male 40°, female 150° | 40° / 40°, 150° / 151° | 39.7°, 151.1° |

So the streaming Rust WPE matches the offline batch reference on ASR. Its online form is more robust than nara_wpe's
online form on noisy scenes, because λ comes from the observed power with a floor. The noise-whitened localization is
what keeps the robot's own fan from capturing the DOA.

## Four talkers on the factory floor

Run `SCENE_ROOT=... node tools/render_mics.js factory <dir> '<json>'`. The renderer supports in-memory overrides:
- `_dur`: scene length.
- `_voices` / `_rate`: put other 16 kHz clips into the `male_low` / `female_high` voice slots.
- `_noise_db`: offset for every ambient noise source and robot noise.

`tools/voices/worker_{a,b,c}.wav` are three extra sentences, synthesised with SpeechBrain TTS (Tacotron2 + HiFi-GAN,
LJSpeech). Worker A is pitched down by 0.85 at render time; worker C ("The forklift is waiting at gate two.") is pitched
up by 1.1 in the file and replaces the Scene Simulator's female in the booth scenes (`tools/make_booth_scenes.sh`). Score a run with `python3 tools/wer4.py <out> <scene>`.

| talker (Factory, RT60 1.2 s) | az | partial overlap, 9.5 s | same, noise −10 dB | dense: up to 4 at once, −10 dB |
|---|---|---|---|---|
| male: "Hi there, how are you doing today?" | 30° | 0.00 | 0.00 | 0.00 |
| worker A: "Please bring the red toolbox to station four." | 110° | 0.00 | 0.12 (tail cut) | 0.00 |
| female: "I'm good. What is the plan for the day?" | 200° | 0.56 | 0.11 | 0.89 |
| worker B: "The conveyor belt stopped five minutes ago." | 290° | 0.00 | 0.00 | 0.00 |
| single raw mic (all four in one transcript) | | garbled 2 of 4 | mostly right, but not split per talker | n/a |

Three of the four talkers are separated and transcribed exactly, even when all four overlap. The female's errors are
not a separation failure. Rendered **alone** at the same spot, she is still misrecognised: the raw mic gives "I'm
here, watching the point for the day."; the pipeline gives "I can hear it. What's in the plane for the day?". Moving
her to 240° (away from the fan and conveyor at 180°) did not change that. Her clip does not survive 1.2 s of hall
reverb at 2 m. Azimuth estimates are within 1° for clear directions. They are biased up to 22° when a noise source
sits nearby (worker A at 110°, beside the machine at 90° and the AGV at 120°).

## Test scenes

`tools/render_mics.js <scene> <dir>` renders any Scene Simulator scenario with the page's own engine, giving known ground truth. Scenarios: home, factory, hospital, automobile, quiet, robot.

`TESTDATA=<dir with scene_*> python3 tools/eval.py <tag> [args]` scores WER per talker.

Mean WER over 14 talkers in 7 scenes, with current defaults: **0.34** (WER treats "what's" as "what is").
- Talker 1 is almost always exact.
- Talker 2 (who starts at 2.6 s) is the weak point.
- The car cabin (automobile) is the worst scene: its strong early reflections bias the DOA by about 20°.

## Ideas from HARK, benchmarked

HARK (hark.jp) is the Honda Research Institute Japan / Kyoto University robot-audition toolkit. Three of its ideas were
tried here. HARK itself cannot be installed on this machine: its apt packages target Ubuntu 20.04/22.04 and need
root. So its methods were re-implemented instead.

Setup, once: `tools/make_ego_takes.sh` re-renders every test scene and adds `<scene>/ego/`, a **robot-only take**
(robot noises with another random seed plus the sensor floor, no talkers, same gain). This is what a HARK user records
for the noise matrix. It also adds `<scene>/dry/`, the talkers' dry sources. The script verifies that each mix is
byte-identical to the existing scene.

`py/.venv/bin/python tools/bench.py <tag> [run args]` scores 30 talkers in 11 scenes: the 7 test scenes plus the 4
four-talker factory scenes. It reports WER, and SDR of each stream against the talker's dry source (bss_eval style,
512-tap filter allowed, so reverb is not penalised). `EGO=1` adds `--ego-noise <scene>/ego`. `tools/bench_diff.py A B`
lists per-talker changes. WER mostly moves on the female's fragile sentence, so SDR is the steadier signal.

| configuration | WER all 30 | 7 scenes | factory4 | exact | SDR | az err |
|---|---|---|---|---|---|---|
| **baseline (mask-MVDR, adaptive Φn)** | **0.246** | 0.336 | 0.168 | 16 | +5.44 dB | 4.9° |
| ego prior: Φn = (adaptive + 100·Φego)/(w+100) | 0.407 | 0.466 | 0.356 | 8 | −9.9 dB | 6.5°, 4 missed |
| ego, DOA whitened by Φego only (HARK GEVD-MUSIC with fixed K) | 0.428 | 0.582 | 0.293 | 10 | +5.76 dB | 10.1°, 4 missed |
| ego add: Φn = adaptive + Φego, everywhere | 0.242 | 0.328 | 0.168 | 16 | +5.44 dB | 5.1° |
| **ego add-doa, g = 1** (`--ego-mode add-doa --ego-weight 1`) | **0.231** | 0.320 | 0.154 | 16 | **+5.66 dB** | **4.6°** |
| ego add-doa, g = 0.5 / 2 | 0.209 / 0.283 | 0.280 / 0.431 | 0.147 / 0.154 | 16 / 15 | +5.56 / +5.94 dB | 5.6° / 4.9° |
| GHDSS, delay-and-sum start (HARK), raw / whitened | 0.345 / 0.304 | 0.548 / 0.498 | 0.168 / 0.135 | 14 / 13 | +5.56 / +5.47 dB | 4.9° |
| GHDSS, MVDR start, raw / whitened (`--sep ghdss --ghdss-mu 3 --ghdss-eta 10`) | 0.306 / 0.276 | 0.489 / 0.413 | 0.147 / 0.156 | 14 / 13 | +5.53 / +5.43 dB | 4.9° |
| GHDSS, MVDR start, whitened, HARK step (μ = 1) | 0.263 | 0.379 | 0.163 | 14 | +5.31 dB | 4.9° |
| local SEVD-MUSIC azimuth refinement, gated on activity (`--music-refine`) | 0.282 | 0.405 | 0.174 | 16 | +5.30 dB | 5.8° |
| same, 1.6 s window, ±30° | 0.334 | 0.508 | 0.181 | 15 | +5.53 dB | 6.9°, 2 missed |

**What came out of it**

- **Fixed ego-noise matrix: a small gain, as long as it is added, not averaged.**
  - Mixing the robot-only take into Φn as a prior, or using it alone for localization (HARK style), under-counts the
    factory's and car's ambient sources. SPP and localization then break.
  - Adding it *on top of* the adaptive Φn for the DOA whitening only (`add-doa`) is the one variant that helped on
    every steady metric. SDR rises with the gain (+0.12 / +0.22 / +0.50 dB at g = 0.5 / 1 / 2), and azimuth error is
    lowest at g = 1. It costs about 12 µs per frame.
  - The effect is small because the simulated fan is steady, so the adaptive tracker already learns it. It should
    matter more for servo noise during motion, keyed by motor state, which the Scene Simulator does not model yet.
  - It is an option, not the default, because it needs a robot-only recording.
- **GHDSS: no overall gain.**
  - SDR is level with mask-MVDR (+5.3 to +5.6 vs +5.44 dB) and factory4 WER is slightly better (0.135–0.156 vs 0.168).
  - But the 7-scene WER is worse. Starting from delay-and-sum, as HARK does, GHDSS needs time to converge, so a talker
    confirmed at 0.5 s loses his first sentence. Starting from the MVDR solution helps, but the every-frame weight
    updates still leave artifacts whisper rejects ("Hi there." and then nothing).
  - Its worst case per frame is lower (p99 295 vs 603 µs on one core), because it does not re-solve every 4th frame.
  - Available as `--sep ghdss`.
- **HARK-style MUSIC localization: right offline, not yet usable streaming.**
  - `py/hark_music.py` runs SEVD-/GEVD-MUSIC as HARK's LocalizeMUSIC does: 500–2800 Hz, Ns = 2, eigenvalue-weighted.
    It uses a 1.6 s window over each talker's speech.
  - SEVD-MUSIC finds worker A at 105–111° in every factory scene. Our tracker reports 129–132° in two of them. So that
    20° bias is in our estimator (vote histogram merging with the AGV), not in the acoustics.
  - Median error is 3.0° (SEVD) vs 1.6° (Rust). Truth is within 10° of a top-K MUSIC peak for 28/30 talkers, vs 25/30
    for the Rust estimate.
  - GEVD-MUSIC with the robot-only noise matrix is worse (24/30), for the same reason as the ego-only Rust mode.
  - A streaming local-MUSIC refinement did not reproduce the offline accuracy. It drifts while a talker is silent and
    gets pulled to the fan. Worth a better design later, for example MUSIC on each stream's mask-weighted covariance.
- **Measured transfer functions (HARKTOOL): not tested.** The simulator's array is free-field with no head or body, so
  free-field steering is already exact here. This matters on the real robot.

## Through the board (pdm_audio_eth)

`tools/board_bench.py` streams the four factory4 scenes through the LP-AM243 and builds board versions of them:

```
py/.venv/bin/python tools/board_bench.py [--iface enp129s0 | --emulator] [scene ...]
BOARD=_board   SCENES=factory4,factory4_n10,factory4_dense,factory4_dense240 py/.venv/bin/python tools/bench.py board
BOARD=_boardin SCENES=factory4,factory4_n10,factory4_dense,factory4_dense240 py/.venv/bin/python tools/bench.py boardin
```

- It runs `pdm-eth stream --set --ch0 mic1.wav .. --ch7 mic8.wav --export-shm NAME`. `--set` gives the 8 mics one
  common gain, so their level differences survive. It reads the aligned input/output blocks from `/dev/shm/NAME`.
  `--legacy` uses `--capture` and finds the lag offline instead, for a `pdm-eth` without these options.
- `<scene>_board/` holds the board output, back at the file level with one gain for all 8 channels.
  `<scene>_boardin/` holds the int16 board input (quantisation only, no PDM), as the control.
- It prints the delay and gain of each channel, output against input. A skew of one 32 kHz sample (31 µs) between
  channels looks like a 1 cm mic shift on the 5 cm ring.

Factory4 scenes, 16 talkers:

| input | WER | SDR | az err | exact |
|---|---|---|---|---|
| direct (scene WAVs) | 0.168 | +6.79 dB | 5.8° | 11/16 |
| board input, int16 | 0.154 | +6.82 dB | 6.9° | 11/16 |
| board output, firmware before 2026-11-07 (even channels +0.5 sample late) | 0.230 | +5.11 dB | 4.1° | 6/16 |
| board output, current firmware (channels time-aligned) | 0.147 | +6.82 dB | 6.5° | 11/16 |

With the channels time-aligned, the PDM chain costs nothing measurable: WER, SDR and azimuth match the direct run.

