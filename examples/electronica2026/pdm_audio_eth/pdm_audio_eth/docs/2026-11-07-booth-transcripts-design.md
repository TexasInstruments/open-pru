# Booth transcripts: live per-talker text in the pdm-eth window

Design, 2026-11-07. For electronica 2026, which opens Tuesday 2026-11-10.
Calls marked *(builder's call)* were made by me, the author, not by the user.

## 1. Intent

At the booth, visitors watch the `pdm-eth gui` window (layout C, the
spectrograms of the board's 8-channel PDM round trip). Today they see that
the audio gets through the LP-AM243 unharmed. This change adds what that
audio is good for: **while a rendered multi-talker scene plays through the
board, the window shows what each person said, live, in one text lane per
talker under the spectrograms**. The text comes from
`pctools/audio_pipe` (8-mic front-end + whisper) running on the **board's
output**, so the visitor sees the PDM chain feeding a working speech
front-end.

The audio is Scene Simulator renders on a loop. Real MEMS microphones are a
separate, later project.

## 2. Goals, non-goals, anti-goals

**Goals (must work at the booth next week)**
- Layout B: one text lane per talker under the IN/OUT/difference panes, on
  the same time axis (-10 s to now), each sentence placed where it was
  spoken and scrolling left with the spectrograms. **Only final text is
  shown** (decided by the user, 2026-11-07): a sentence appears once the
  talker stops and whisper has decoded it, about 0.5 s after the end of
  speech. No partial (in-progress) text is drawn.
- A view switch in the top bar: **engineering** (selected, this view) |
  **booth view** (greyed out, not clickable; placeholder for layout C:
  robot top view with talker dots and transcript cards, for the real-mic
  project).
- A playlist of curated factory scenes, looped forever.
- One command starts everything (`pdm-eth-demo`, see 6).
- The spectrograms never stop because of the transcription: if audio_pipe
  is missing, crashes, or has no GPU, the lanes show "transcription off"
  and the rest of the window works as today.

**Factory first.** Other Scene Simulator settings (home, hospital, car) are
added only after the factory playlist is done and curated.

**Non-goals for this week:** the booth view itself; real mics; visitors
choosing scenes or talking into the demo; neural second pass
(`--neural`, off by default anyway); changes to the board firmware.

**Anti-goal: garbled text on screen.** A wrong transcript in front of a
visitor is a failure even if everything else works. This is handled by
curation (3), not by tuning the recogniser at the booth: every talker of
every looped scene must come out word-for-word right.

## 3. Scenes and curation

- A scene = a capture set (`mic1..8.wav` + `settings.json`, 16 kHz, one
  gain for all 8 mics), as in `pc_app/audio/factory_4workers*`. Rendered
  with `pctools/audio_pipe/tools/render_mics.js` (its `_dur` fix is
  committed, 6e02a34b).
- **Curation rule:** a scene is in the playlist only if, through the live
  path (board included, 7), every talker's final text matches the
  reference sentence exactly (WER 0 after `wer4.py`'s normalisation) on at
  least 5 consecutive passes, with no accepted text on any non-talker
  stream.
- Known starting point (measured 2026-11-07, file path through the board):
  `factory_4workers_9s5_b270_f150` gets male, worker A and worker B exact,
  but the female (150°, 2.0 m, -2 dB) is separated (stream correlates
  0.73 with her dry clip) and not recognised. Her dry clip alone is
  recognised exactly. So she needs changing: closer, louder, or a different
  voice. *(builder's call: which, and how many scenes; aim for 2-4 factory
  scenes with different talker layouts so the loop is not one repeat.)*
- References: `wer4.py`'s sentences for the male, female and the two TTS
  workers. A scene with other voices adds its references to the scorer.

## 4. How it fits together

```
 playlist ──► pdm-eth gui ──AUDIO_IN──► LP-AM243 ──AUDIO_OUT──► pdm-eth gui ──► spectrograms (as today)
                  │                                                   │
                  │ control lines (stdin)          /dev/shm/<name>    │ --export-shm (aligned in/out blocks)
                  ▼                                     ▼             │
             audio_pipe live  ◄──────── reads board OUTPUT ◄──────────┘
                  │
                  └── text lines (stdout, JSON) ──► pdm-eth gui ──► text lanes
```

**Decided with the user:** layout B, booth view as a greyed button, text
from the board output, curated scenes on a loop, factory first.

**Builder's calls (keep unless there is a reason not to):**

- **Separate process.** The GUI spawns `audio_pipe` as a child process.
  whisper.cpp and CUDA stay out of the `pc_app` build; `pc_app` keeps
  building and testing without a GPU.
- **Audio in:** `pdm-eth gui --export-shm NAME` already writes aligned
  64-frame blocks (board index, 8 x i16 input, 8 x i32 output) to a ring
  in `/dev/shm/NAME` (layout: `pc_app/src/seam.rs`). audio_pipe gets a new
  subcommand `live --shm NAME` that reads the **output** half, converts it
  to the same float level the file path produces (the level the file
  pipeline was tuned and curated on; `tools/board_bench.py` shows the
  scaling: file gain of the set, x 256 for 16 to 24 bit, board gain
  about -0.3 dB), and feeds `Pipeline::process` 128 samples (one hop)
  at a time. The GUI ring holds 256 blocks (1 s); audio_pipe must keep up
  (it runs at about 17 % of a core) and count any overrun.
- **Text out:** one JSON object per line on audio_pipe's stdout, for
  example
  `{"pass":12,"speaker":3,"az":221.8,"start":145343168,"end":145395200,"text":"Please bring…","final":true}`.
  `start`/`end` are board sample indices, so the GUI can place the text
  on its time axis no matter how late it arrives. Partials are off
  (`--partial-ms 0`, the default) and `final` is always true; rejected
  hypotheses are never sent.
- **Control in:** the GUI writes one JSON line to audio_pipe's stdin
  when a scene pass starts: `{"pass":12,"scene":"factory_4workers_a","start":<board index>}`.
  audio_pipe resets its pipeline (tracker, covariances, WPE, ASR
  endpointers) at that index. **Every pass is then a fresh run**, which is
  what the curation check measured, so the booth shows what was
  verified. *(builder's call)* Before the reset, audio_pipe **flushes** the
  old pass the way `run` ends a file (`pipe.flush()`, zeros through the
  synthesis, endpointers closed) and delivers its final text. To give the last utterance its endpoint hang and
  whisper's decode time, every pass is followed by **1.0 s of silence**
  in the stream (the playlist inserts it; scenes do not need trailing
  silence), and the control line for the next pass is sent at the end of
  that gap. The curation check (7) runs with the same gap.
- **Lanes:** a lane appears with a talker's first accepted text in a pass.
  A talker in the next pass whose azimuth is within 20° of an existing
  lane reuses that lane and its colour, so a repeated scene keeps its
  lanes. Lanes not used for 10 s fade out. At most 6 lanes (audio_pipe's
  `--max-speakers`). The label is a colour dot, a number and the azimuth.
  Colours from a fixed palette of 6 distinct, dark-background-safe hues.
- **Side panel:** one more line, "text latency" (end of speech to final
  text, last utterance), and one status line for audio_pipe (running /
  starting / transcription off + reason).
- **Playlist:** `pdm-eth gui --playlist <dir>`: every subfolder with
  `mic1..8.wav` is a scene, played in name order as a capture set, one
  pass each, then from the start again, with the 1.0 s silence after each
  pass (above). The IN pane label names the scene and the pass number.
- **audio_pipe dying mid-show** *(builder's call)*: the GUI restarts it
  automatically (status "starting"), at most once per 10 s, and sends it
  the control line of the next pass boundary; it joins there, so a
  restart costs the rest of one pass. After 3 failed starts in a row the
  status stays "transcription off" with the reason until the demo is
  restarted. The booth staff never need to do anything for a single
  crash.

## 5. Constraints

- Rust for `pc_app` (egui/eframe, Linux, AF_PACKET) and `audio_pipe`, as
  today. No new heavy dependencies in `pc_app`.
- `pc_app` must still build and `cargo test` without CUDA, whisper models
  or a board (emulator).
- **Only one `pdm-eth` may feed the board.** A `stream` run while the GUI
  is open returns the booth clips; the curation runs (7) must close the
  GUI or use the GUI itself.
- Booth PC = this PC (RTX 5060 Ti, CUDA 13.x, the ASIX USB 3 adapter
  `enx9c69d3c13719`; the demo how-to also covers `enp129s0`). Models live
  in `pctools/audio_pipe/models/` (gitignored, downloaded once).
- Deadline: running at the booth on 2026-11-10. Prefer the smallest change
  that meets the goals; nothing in this document justifies refactoring
  `ui.rs` beyond adding the lanes, the view switch and the side lines.

## 6. Running it

- `pdm-eth-demo` (pc_app/pdm-eth-demo.sh) gains: build audio_pipe if
  needed, check that the model file exists (else start without
  transcription and say why), then `pdm-eth gui --playlist <dir>
  --transcribe <audio_pipe binary>`. Defaults: the playlist is
  `pc_app/audio/booth/` *(builder's call)*, overridable with an
  environment variable as `PDM_IFACE` is today.
- `--transcribe` off (or audio_pipe missing): today's behaviour plus the
  greyed booth-view button.
- The demo how-to (`demo_howto.md`) gets a section: start, what the
  visitor sees, what to do when the lanes say "transcription off".

## 7. Checking it

- **Curation:** run the GUI on the playlist with the board, log
  audio_pipe's JSON lines, and score each pass against the scene's
  `settings.json` with `wer4.py`'s rules. A script for this is part of
  the work *(builder's call how)*. Rule in 3.
- **Without a board** *(builder's call)*: `pdm-eth gui --emulator --playlist … --transcribe …`
  is a development smoke test only: lanes appear, text arrives, nothing
  crashes. It is not a pass/fail on the text (the emulator's round trip is
  not bit-exact); curation is on the board only.
- **Unit tests** (no GPU): lane placement from board indices; lane reuse
  by azimuth; JSON line parsing; the GUI survives audio_pipe exiting,
  never starting, or printing garbage.
- **Soak:** at least 1 hour on the board before the show: no growth in
  memory, no shm overruns, text latency stable, still exact.

## 8. Left to the builder

The scene changes that make the female (or a replacement) exact; the
number and layout of curated scenes; palette and exact lane drawing; the
playlist folder name; how the curation script is built; whether the
live subcommand shares code with `run` or wraps it.
