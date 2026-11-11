# Booth transcripts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** While curated factory scenes loop through the LP-AM243, the `pdm-eth gui` window shows each talker's final text in one lane per talker under the spectrograms, recognised live from the board's output by `audio_pipe`.

**Architecture:** The GUI plays a playlist as one long capture set (scenes + 1 s gaps, concatenated) and marks each pass start with its board sample index. It exports the aligned board stream to `/dev/shm` (existing seam) and runs `audio_pipe live` as a supervised child process: control lines in on stdin (pass starts), text lines out on stdout (JSON). audio_pipe rebuilds its pipeline at every pass start, after flushing the previous pass, so every pass is a fresh run like the curated file runs.

**Tech Stack:** Rust (pc_app: egui/eframe 2021, hound, serde_json to add; audio_pipe: clap, crossbeam, whisper-rs on CUDA), Python 3 (curation script), bash (launcher).

**Spec:** `examples/electronica2026/pdm_audio_eth/pdm_audio_eth/docs/2026-11-07-booth-transcripts-design.md`

Paths below are relative to `examples/electronica2026/pdm_audio_eth/`. `pc_app/` = the GUI crate, `pctools/audio_pipe/` = the pipeline crate.

## Global Constraints

- Deadline: running at the booth on 2026-11-10. Smallest change that meets the spec; no refactoring of `pc_app/src/ui.rs` beyond the lanes, the view switch and the side lines.
- `pc_app` must build and `cargo test` without CUDA, whisper models or a board. No whisper/CUDA dependency in `pc_app`.
- Only final text is shown; `audio_pipe live` runs with partials off (`--partial-ms 0`). Rejected hypotheses are never sent.
- Gap after every pass: **1.0 s = 16000 frames** of silence.
- Lane reuse: azimuth within **20°**. Lane fades after **10 s** unused. At most **6** lanes.
- Restart policy: at most one audio_pipe start per **10 s**; after **3** failed starts in a row, status stays "transcription off" with the reason.
- Curation: every talker WER 0 (`wer4.py` normalisation) on **5** consecutive passes, no accepted text on non-talker streams, on the board, with the 1.0 s gap.
- Only one `pdm-eth` may feed the board (a `stream` while the GUI is open gets booth clips back).
- Booth PC: this PC, NIC `enx9c69d3c13719` (also `enp129s0` in the how-to), models in `pctools/audio_pipe/models/`.
- Copyright header on new files: `// SPDX-License-Identifier: BSD-3-Clause` + `// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/` (`#` for Python/shell), as in the existing files.
- Commit messages: `feat|fix|docs(pdm_audio_eth): ...`, ending with the attribution lines from the session.

## Review Focus

1. **audio_pipe restarted mid-pass** (crash, or started after the GUI): it must ignore audio until the next pass start and emit no text for the partial pass. Test in Task 3.
2. **The GUI restarts while audio_pipe runs** (the shm file is recreated, its `W` counter starts again from 0): audio_pipe must re-attach, not hang or read stale blocks. Test in Task 2.
3. **audio_pipe falls behind by more than the ring** (GPU stall, CPU load): the overrun is counted, the reader jumps to the newest block, and nothing is emitted until the next pass start. Tests in Tasks 2 and 3.
4. **Garbage, partial lines or an early exit on audio_pipe's stdout** (a panic message, a whisper log line on stdout, a killed child): the GUI ignores lines that are not valid text lines, and applies the restart policy on exit. Test in Task 6.
5. **No board or a reset board** (no output blocks, or a jump in board index): the lanes say "waiting for audio" and nothing crashes; a pass whose start index is never reached is skipped at the next mark. Tests in Tasks 3 and 5.

---

## File Structure

| File | Change | Responsibility |
|---|---|---|
| `pctools/audio_pipe/src/main.rs` | modify | split `RunArgs` into file-only flags + `PipeArgs`; `build_pipeline()`; new `Live` subcommand |
| `pctools/audio_pipe/src/shm.rs` | create | reader for the `PDMSHM01` ring written by `pc_app/src/seam.rs` |
| `pctools/audio_pipe/src/live.rs` | create | live loop: control lines, per-pass rebuild + flush, JSON text lines |
| `pc_app/src/wavin.rs` | modify | `CaptureSet.gain` (int16 counts per file unit) |
| `pc_app/src/playlist.rs` | create | load a playlist folder into one `InputSet` + pass table |
| `pc_app/src/ui.rs` | modify | `NetInput.marks`, `NetMsg::Mark`; lanes drawing, view switch, side lines |
| `pc_app/src/transcribe.rs` | create | supervise the `audio_pipe live` child: spawn, stdin, stdout parse, restart policy |
| `pc_app/src/lanes.rs` | create | lane model: placement by board index, reuse by azimuth, fade, palette |
| `pc_app/src/main.rs` | modify | `gui --playlist DIR --transcribe BIN --transcript-log FILE` |
| `pc_app/pdm-eth-demo.sh` | modify | build audio_pipe, check model, start with playlist + transcription |
| `pctools/audio_pipe/tools/wer4.py` | modify | importable `REF`, `norm`, `wer`, `score(transcript, settings)` |
| `pctools/audio_pipe/tools/booth_curate.py` | create | score a transcript log per scene and pass, verdict per scene |
| `pctools/audio_pipe/tools/make_booth_scenes.sh` | create | render the curated scenes into `pc_app/audio/booth/` |
| `pdm_audio_eth/docs/demo_howto.md` | modify | booth section |

`pc_app/audio/booth/` stays **untracked** (add to `pc_app/.gitignore` if one exists, else to the repo's `.gitignore` for that path): the scenes use `scenes/voices/` clips whose licence is not documented; `make_booth_scenes.sh` recreates them.

---

### Task 1: audio_pipe: share the pipeline set-up between `run` and `live`

No behaviour change; it makes Task 3 possible without copying ~120 lines of set-up.

**Files:**
- Modify: `pctools/audio_pipe/src/main.rs` (`RunArgs` at :50-150, `run()` at :302-473)

**Interfaces:**
- Produces: `#[derive(Args, Clone)] struct PipeArgs` holding every flag `run()` uses to build the pipeline (model, no_asr, cpu_asr, language, radius, wpe_*, max_speakers, mics_off, exclude, peak_min, birth_min, coh_min, replay_ms, white_load, kappa, s_bg, loading, pf_gmin, sp_floor, vad_*, hang_ms, partial_ms, sep, ghdss_*, neural*, music_*, ego_*, verbose).
- Produces: `struct RunArgs { input: PathBuf, out: PathBuf, realtime: bool, profile: bool, no_health: bool, #[command(flatten)] p: PipeArgs }` (file-only flags stay in `RunArgs`).
- Produces: `fn build_pipeline(p: &PipeArgs, active: &[bool], asr: Option<asr::AsrWorker>) -> Result<pipeline::Pipeline>`: everything `run()` does between the health check and the frame loop, except spawning the ASR worker.
- Produces: `fn spawn_asr(p: &PipeArgs) -> Result<Option<asr::AsrWorker>>`.

- [ ] **Step 1: Record the current output as the reference**

Run (from `pctools/audio_pipe`): `./target/release/audio_pipe run $SCENE_ROOT/audio_wav_factory_4workers_9s5_b270_f150 --out /tmp/ap_before 2>&1 | sed -n '/per-speaker/,$p' > /tmp/ap_before.txt`
Expected: 5 speakers, the male, worker A and worker B lines exact (as in the scene README).

- [ ] **Step 2: Split the flags and extract `build_pipeline` and `spawn_asr`**

All flag names and defaults stay the same, so every existing command line still works.

- [ ] **Step 3: Verify the output is unchanged**

Run: the Step 1 command with `--out /tmp/ap_after` into `/tmp/ap_after.txt`, then `diff <(grep -v ms /tmp/ap_before.txt) <(grep -v ms /tmp/ap_after.txt)` and `cargo test --release`.
Expected: no diff (timing lines excluded), all tests pass.

- [ ] **Step 4: Commit**

```bash
git add pctools/audio_pipe/src/main.rs
git commit -m "refactor(pdm_audio_eth): audio_pipe pipeline set-up shared by run and a coming live mode"
```

---

### Task 2: audio_pipe: shared-memory ring reader

**Files:**
- Create: `pctools/audio_pipe/src/shm.rs` (+ `mod shm;` in `main.rs`)
- Test: in-file `#[cfg(test)] mod tests`

**Interfaces:**
- Consumes: the layout documented in `pc_app/src/seam.rs:9-25` (magic `PDMSHM01`, u32 N, u32 B = 3080, u64 W at offset 16, blocks from offset 64; block = u64 index, 64×8 i16 input, 64×8 i32 output).
- Produces:
  ```rust
  pub struct Block { pub index: u64, pub out: [[i32; 8]; 64] }
  pub enum Read { Block(Block), Empty, Overrun { skipped: u64 }, Reattached }
  pub struct ShmReader { /* file, next: u64, n: u64, ... */ }
  impl ShmReader {
      pub fn open(path: &Path) -> std::io::Result<Self>;   // next = W at open: only blocks written after open are read
      pub fn next(&mut self) -> std::io::Result<Read>;      // never blocks
      pub fn overruns(&self) -> u64;
  }
  ```

- [ ] **Step 1: Write the failing tests**

A test helper writes a file in the seam layout (N = 8 blocks) with a given `W` and block contents.
- `reads_blocks_in_order`: open at W = 1, writer advances W to 3 → `Block` number 1, then number 2, then `Empty`.
- `overrun_jumps_to_oldest_valid`: open at W = 1, writer advances W to 20 (N = 8; blocks 12..19 are still in the ring) → `Overrun { skipped: 11 }` (blocks 1..11 lost), next read is block 12; `overruns() == 1`.
- `writer_restart_reattaches` (Review Focus 2): open at W = 50, the file is replaced by a new one with W = 2 → `Reattached`, next read is the newest block of the new file.
- `bad_magic_is_an_error`: a file with another magic → `open` returns `Err` with "PDMSHM01" in the message.

- [ ] **Step 2: Run them to verify they fail**

Run: `cargo test --release shm::`
Expected: FAIL (module missing).

- [ ] **Step 3: Implement `ShmReader`**

Read with `FileExt::read_at` (no mmap needed). A block is valid if `W` after copying it is still `< index_of_slot + N`; else it's an overrun. Detect a restart with `W < next` or a changed inode (`MetadataExt::ino`) → reopen by path.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cargo test --release shm::`
Expected: 4 passed.

- [ ] **Step 5: Commit**

```bash
git add pctools/audio_pipe/src/shm.rs pctools/audio_pipe/src/main.rs
git commit -m "feat(pdm_audio_eth): audio_pipe reader for the pdm-eth shared-memory ring"
```

---

### Task 3: audio_pipe: `live` subcommand

**Files:**
- Create: `pctools/audio_pipe/src/live.rs`
- Modify: `pctools/audio_pipe/src/main.rs` (`Cmd::Live(LiveArgs)`)
- Test: in-file tests in `live.rs`

**Interfaces:**
- Consumes: `PipeArgs`, `build_pipeline`, `spawn_asr` (Task 1); `ShmReader`, `Read`, `Block` (Task 2); `Pipeline::{process, flush, jobs_sent, streams, asr}`.
- Produces (CLI): `audio_pipe live --shm NAME [PipeArgs...]`. Path `/dev/shm/NAME`. Health check skipped (all 8 mics active unless `--mics-off`).
- Produces (stdin, one JSON object per line, from the GUI):
  `{"pass": u64, "scene": String, "start": u64, "scale": f32}`: at board index `start` a new pass begins; output samples become `out_i32 as f32 * scale` (the file level).
- Produces (stdout, one JSON object per line, nothing else on stdout; logs go to stderr):
  `{"pass": u64, "scene": String, "speaker": usize, "az": f32, "start": u64, "end": u64, "text": String, "latency_ms": f32}`, `start`/`end` = board indices (`pass_start + round(t * 16000)` of `t_start` / `t_speech_end`).
  `{"status": "waiting" | "running" | "overrun", "detail": String}` on a change of state.
- Produces (Rust): `pub fn handle_control(line: &str) -> Option<PassStart>`; `pub struct PassStart { pub pass: u64, pub scene: String, pub start: u64, pub scale: f32 }`; `pub fn text_line(pass: &PassStart, r: &asr::AsrResult, az: f32, latency_ms: f32) -> String`.

Behaviour:
- Until the first pass start is reached: discard blocks, status `waiting` (Review Focus 1 and 5).
- On reaching a pending pass start (the first block with `index >= start`): `flush()` the current pipeline, wait (at most 3 s) until the ASR results counted by `jobs_sent` have arrived, emit them, then `build_pipeline` again, moving the `AsrWorker` (`pipe.asr.take()`) into the new pipeline so the model stays loaded. Feed 128-sample hops from the start index on.
- A start index that is already behind the reader when its control line arrives is skipped (Review Focus 5).
- On `Read::Overrun` or `Read::Reattached`: drop the current pass without emitting it, status `overrun`, wait for the next pass start (Review Focus 3).
- Accepted finals only (`!r.partial && r.hyp.accepted`), and `--partial-ms` forced to 0.
- `latency_ms`: wall time from processing the hop that holds `t_speech_end` to receiving the result. Keep a ring of `(frame, Instant)` for the last 10 s.
- Missing block indices (lost frames): fill up to 8 missing blocks with zeros; a larger jump drops the pass like an overrun.

- [ ] **Step 1: Write the failing tests** (no GPU: `--no-asr` and a stub ASR where a result is needed)
- `control_line_parses`: `handle_control(r#"{"pass":3,"scene":"a","start":1000,"scale":0.5}"#)` == `Some(PassStart{pass:3, scene:"a", start:1000, scale:0.5})`; `handle_control("garbage")` == `None`.
- `text_line_has_board_indices`: for `PassStart{start: 1_000_000, ..}` and a result with `t_start = 0.5`, `t_speech_end = 2.0` → JSON with `"start":1008000,"end":1032000`.
- `live_matches_run_on_a_scene`: write the 9.5 s scene `factory_4workers_9s5_b270_f150` (int16 files → i32 × 256, scale `1/(256·g)`) into a test ring file, run the live loop with `--no-asr` for one pass, compare the azimuths of the confirmed tracks with `run --no-asr` on the folder: same count, each within 0.5°. (Skip with a message if `$SCENE_ROOT` is unset.)
- `no_output_before_first_pass_start` (Review Focus 1): blocks from index 0, pass start at 48000 → no track is born from samples before 48000 (`pipe.t` counts only from 48000).
- `overrun_drops_the_pass` (Review Focus 3): an `Overrun` during a pass → the stub ASR's results of that pass are not printed; the next pass is.

- [ ] **Step 2: Run them to verify they fail**

Run: `cargo test --release live::`
Expected: FAIL (module missing).

- [ ] **Step 3: Implement `live.rs` and `Cmd::Live`**

Use a stdin reader thread → `crossbeam_channel`; the main loop polls `ShmReader::next()` and sleeps 2 ms on `Empty`. Flush stdout after every line.

- [ ] **Step 4: Run the tests to verify they pass**

Run: `cargo test --release`
Expected: all pass (including Task 1/2 tests).

- [ ] **Step 5: Smoke test with the GPU, by hand**

Run `pdm-eth stream --emulator --set --ch0 mic1.wav .. --ch7 mic8.wav --export-shm aptest` on the 9.5 s scene, and at the same time `audio_pipe live --shm aptest` with a control line typed into stdin for the stream's first index (printed by `pdm-eth` as "first board index"; `scale` = `1/(256*g)`, g from the set's peak).
Expected: text lines for the male, worker A and worker B, latency_ms 300-700.

- [ ] **Step 6: Commit**

```bash
git add pctools/audio_pipe/src/live.rs pctools/audio_pipe/src/main.rs
git commit -m "feat(pdm_audio_eth): audio_pipe live mode on the pdm-eth shared-memory stream"
```

---

### Task 4: pc_app: playlist

**Files:**
- Modify: `pc_app/src/wavin.rs` (`CaptureSet`, `load_set` at :86-131)
- Create: `pc_app/src/playlist.rs` (+ `mod playlist;` in `main.rs`)
- Test: in-file tests

**Interfaces:**
- Produces: `CaptureSet.gain: f32` = int16 counts per file unit (`16384 / peak`, the factor `scale()` applied; `0.0` for a silent set).
- Produces:
  ```rust
  pub const GAP_FRAMES: usize = 16000;
  pub struct Pass { pub scene: String, pub offset: usize, pub frames: usize, pub scale: f32 }
  pub struct Playlist { pub set: InputSet, pub passes: Vec<Pass>, pub total: usize }
  pub fn load_playlist(dir: &Path) -> Result<Playlist, WavError>;
  ```
  Every subfolder of `dir` with `mic1.wav`..`mic8.wav`, in name order, is loaded with `load_set`; the scenes are concatenated, each followed by `GAP_FRAMES` zeros, all 8 channels the same length (`total`), `start` all 0. `Pass.offset` = first frame of the scene in the loop; `Pass.scale` = `1.0 / (256.0 * gain)` (the board's 24-bit output back to the file level).

- [ ] **Step 1: Write the failing tests**
- `playlist_concatenates_with_gaps`: two temp scenes of 1000 and 2000 frames → `total == 1000 + 16000 + 2000 + 16000`, `passes[1].offset == 17000`, frames 1000..17000 all zero on every channel.
- `playlist_keeps_level_differences`: a scene whose mic2 is half of mic1 → after loading, channel 1 is half of channel 0 (±1 LSB).
- `playlist_scale_maps_back_to_file_level`: a scene with peak 0.25 → `scale == 1.0 / (256.0 * 16384.0 / 0.25)`.
- `empty_or_bad_playlist_is_an_error`: an empty folder → `Err` naming the folder; a scene with 7 mics → `Err` naming the scene.

- [ ] **Step 2: Run them to verify they fail**

Run: `cargo test playlist::`
Expected: FAIL.

- [ ] **Step 3: Implement**

- [ ] **Step 4: Run all pc_app tests**

Run: `cargo test`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add pc_app/src/wavin.rs pc_app/src/playlist.rs pc_app/src/main.rs
git commit -m "feat(pdm_audio_eth): pc_app playlist of capture sets with a 1 s gap per pass"
```

---

### Task 5: pc_app: play the playlist and mark pass starts

**Files:**
- Modify: `pc_app/src/ui.rs` (`NetInput` :60-68, `NetMsg` :88-96, `run_net` :116-250, IN pane label)
- Modify: `pc_app/src/main.rs` (`Cmd::Gui`: `--playlist DIR`; conflicts with `--chN`/`--set`)
- Test: in `ui.rs` tests (emulator, like `net_thread_feeds_output_from_emulator`)

**Interfaces:**
- Consumes: `Playlist`, `Pass` (Task 4).
- Produces: `NetInput.marks: Vec<usize>` (loop offsets of pass starts; empty without a playlist).
- Produces: `NetMsg::Mark { slot: usize, cycle: u64, index: u64 }`: sent when the frame at loop offset `marks[slot]` is sent to the board; `index` = its board sample index, `cycle` = how many times the loop has wrapped.
- Produces: `App` field `pass: Option<(String, u64)>` (scene, pass number = `cycle * passes.len() + slot`), shown in the IN pane label as `IN (PC): <scene> (pass <n>)`.

- [ ] **Step 1: Write the failing tests**
- `marks_carry_board_indices`: emulator, playlist of two short scenes → the first two `Mark`s have `slot` 0 and 1, `index` difference == `passes[1].offset - passes[0].offset`, and the input frame at `Mark.index` (from `NetMsg::In`) equals the playlist's first frame of that scene.
- `marks_repeat_every_cycle`: after one loop, `Mark{slot:0, cycle:1}` arrives with `index` = first index + `total`.
- `board_reset_does_not_stop_marks` (Review Focus 5): emulator reset mid-loop (as in `input_pane_resumes_after_board_reset`) → marks keep coming.

- [ ] **Step 2: Run them to verify they fail**

Run: `cargo test marks`
Expected: FAIL.

- [ ] **Step 3: Implement**

The board index of a sent frame is already known in `run_net` where `sent_in` is filled; emit the mark there.

- [ ] **Step 4: Run all pc_app tests**

Run: `cargo test`
Expected: all pass.

- [ ] **Step 5: Commit**

```bash
git add pc_app/src/ui.rs pc_app/src/main.rs
git commit -m "feat(pdm_audio_eth): pc_app gui --playlist, pass marks with board indices"
```

---

### Task 6: pc_app: supervise `audio_pipe live`

**Files:**
- Create: `pc_app/src/transcribe.rs` (+ `mod transcribe;`)
- Modify: `pc_app/Cargo.toml` (add `serde` with `derive` and `serde_json`)
- Test: in-file tests with fake children (small shell scripts written to a temp dir)

**Interfaces:**
- Consumes: the stdin/stdout line formats of Task 3.
- Produces:
  ```rust
  #[derive(Clone, Debug, PartialEq, serde::Deserialize)]
  pub struct TextLine { pub pass: u64, pub scene: String, pub speaker: usize, pub az: f32,
                        pub start: u64, pub end: u64, pub text: String, pub latency_ms: f32 }
  pub enum TStatus { Off(String), Starting, Waiting, Running, Overrun }
  pub struct TextPipe { /* child, stdin, rx, restart bookkeeping */ }
  impl TextPipe {
      pub fn new(cmd: Vec<String>) -> Self;            // argv of `audio_pipe live --shm NAME ...`, not started yet
      pub fn pass_start(&mut self, pass: u64, scene: &str, start: u64, scale: f32); // starts the child if needed, writes the control line
      pub fn poll(&mut self, now: Instant) -> Vec<TextLine>; // non-blocking; applies the restart policy
      pub fn status(&self) -> &TStatus;
  }
  pub fn parse_line(s: &str) -> Option<Parsed>;   // enum Parsed { Text(TextLine), Status(TStatus) }
  ```
- Restart policy (Global Constraints): child exit → `Starting` and restart at the next `pass_start` call that is ≥ 10 s after the last start; 3 failed starts in a row (exit within 5 s of starting, or spawn error) → `Off(reason)`, no more restarts. A child that ran ≥ 60 s resets the failure count.
- A spawn error (binary missing) → `Off("audio_pipe not found: <path>")`.

- [ ] **Step 1: Write the failing tests**
- `parses_text_and_status_lines`: one valid text line → `Parsed::Text` with the fields; `{"status":"waiting","detail":""}` → `Parsed::Status(Waiting)`.
- `ignores_garbage` (Review Focus 4): lines `"whisper_init: ..."`, `"{"`, `""`, `{"pass":"x"}` → `None` each; a fake child printing them between two valid lines → `poll` returns exactly the 2 valid lines.
- `restarts_after_exit`: a fake child that exits after printing one line → status `Starting`; `pass_start` 11 s later (fake `now`) starts it again and writes the control line (the fake child echoes stdin to a file; check the file).
- `gives_up_after_three_quick_failures`: a fake child that exits immediately → after 3 starts, `Off(..)` and no 4th start.
- `missing_binary_is_off`: `TextPipe::new(vec!["/nonexistent".into()])` + `pass_start` → `Off` with "not found".

- [ ] **Step 2: Run them to verify they fail**

Run: `cargo test transcribe::`
Expected: FAIL.

- [ ] **Step 3: Implement**

The stdout reader thread sends `Parsed` over `std::sync::mpsc`; stderr is inherited (the terminal shows audio_pipe's log). On drop, kill the child.

- [ ] **Step 4: Run all pc_app tests**

Run: `cargo test`
Expected: all pass, no GPU needed.

- [ ] **Step 5: Commit**

```bash
git add pc_app/src/transcribe.rs pc_app/src/main.rs pc_app/Cargo.toml pc_app/Cargo.lock
git commit -m "feat(pdm_audio_eth): pc_app supervises audio_pipe live (control in, text out, restarts)"
```

---

### Task 7: pc_app: text lanes, view switch, side lines

**Files:**
- Create: `pc_app/src/lanes.rs` (+ `mod lanes;`)
- Modify: `pc_app/src/ui.rs` (`App::ui` at :758-820, `side_lines` at :611, top bar)
- Test: in-file tests in `lanes.rs`; screenshot check with `--screenshot`

**Interfaces:**
- Consumes: `TextLine` (Task 6); `time_x(left, right, secs_ago)` (`ui.rs:665`).
- Produces:
  ```rust
  pub const MAX_LANES: usize = 6; pub const REUSE_DEG: f32 = 20.0; pub const FADE_FRAMES: u64 = 160_000;
  pub struct Lane { pub az: f32, pub colour: usize, pub label: usize, pub items: Vec<TextLine>, pub last_end: u64 }
  pub struct Lanes { pub lanes: Vec<Lane> }
  impl Lanes {
      pub fn add(&mut self, t: TextLine);         // reuse a lane within REUSE_DEG (closest), else a new one; at MAX_LANES replace the least recently used
      pub fn prune(&mut self, now_index: u64);    // drop lanes with now_index - last_end > FADE_FRAMES; drop items older than 10 s
  }
  pub fn palette(i: usize) -> egui::Color32;      // 6 fixed hues readable on the dark background
  ```
  `label` = 1-based lane number shown as `● <label> · <az>°`. A lane keeps its colour for its lifetime.
- Drawing: one row per lane (26 px) under the difference pane, same left/right as the spectrogram image; each item is a rounded chip from `time_x(start)` to at least its text width, in the lane colour; the text is clipped at the right edge. `now_index` = the newest output board index the UI has.
- Top bar: two buttons right-aligned, `engineering` (selected) and `booth view` (disabled, hover text "for the real-mic project").
- Side panel: `text latency` (last `latency_ms`, "—" before the first) and `transcription` (`TStatus` as text; `Off(reason)` shows "off: <reason>"); with no `--transcribe`, the lane area shows "transcription off".

- [ ] **Step 1: Write the failing tests**
- `reuses_lane_within_20_degrees`: add az 221.8 then az 214.0 (next pass) → 1 lane, 2 items; add az 252 → 2 lanes.
- `wraps_around_360`: az 355 then az 5 → 1 lane.
- `fades_after_10_s`: a lane with `last_end = 0`, `prune(160_001)` → 0 lanes.
- `seventh_talker_replaces_least_recent`: 7 distinct azimuths → 6 lanes, the one with the oldest `last_end` gone.

- [ ] **Step 2: Run them to verify they fail**

Run: `cargo test lanes::`
Expected: FAIL.

- [ ] **Step 3: Implement `lanes.rs`, then the drawing, view switch and side lines in `ui.rs`**

- [ ] **Step 4: Run all tests and look at a screenshot**

Run: `cargo test`, then `WAYLAND_DISPLAY= pdm-eth gui --emulator --screenshot /tmp/lanes.png` (no `--transcribe` yet).
Expected: tests pass; the screenshot shows the lane area under the difference pane with "transcription off", the two view buttons (booth view greyed) and the two new side lines. Lanes with text are checked in Task 8 Step 3.

- [ ] **Step 5: Commit**

```bash
git add pc_app/src/lanes.rs pc_app/src/ui.rs pc_app/src/main.rs
git commit -m "feat(pdm_audio_eth): pc_app text lanes per talker, view switch, transcription side lines"
```

---

### Task 8: wire it up: `--transcribe`, launcher, emulator smoke test

**Files:**
- Modify: `pc_app/src/main.rs` (`gui`: `--transcribe BIN`, `--transcript-log FILE`)
- Modify: `pc_app/src/ui.rs` (on `NetMsg::Mark` → `TextPipe::pass_start`; each frame `poll` → `Lanes::add`, `prune`)
- Modify: `pc_app/pdm-eth-demo.sh`
- Modify: `pc_app/README.md` (commands table: the new `gui` options)

**Interfaces:**
- Consumes: Tasks 4-7.
- `--transcribe BIN`: implies `--export-shm pdm-eth-<pid>` if none is given, with a ring of **1024** blocks (4 s) instead of the GUI's 256; argv = `BIN live --shm <name>`. Requires `--playlist` (error otherwise: "--transcribe needs --playlist").
- `--transcript-log FILE`: append every received `TextLine` as its JSON line, plus one line per mark `{"mark": {"pass": n, "scene": s, "start": i}}` (input for Task 9).
- Launcher: build `pctools/audio_pipe` (`cargo build --release` with `PATH=/usr/local/cuda/bin:$PATH CMAKE_CUDA_ARCHITECTURES=120`) unless `PDM_NO_BUILD=1`; if `pctools/audio_pipe/models/ggml-large-v3-turbo-q5_0.bin` is missing, say so and start without `--transcribe`; playlist `PDM_PLAYLIST` (default `pc_app/audio/booth`), started with `--playlist $PDM_PLAYLIST --transcribe <audio_pipe> --transcript-log $HOME/pdm-eth-booth.log`. Without the playlist folder: today's behaviour.

- [ ] **Step 1: Write the failing test**
- `transcribe_needs_playlist`: `pdm-eth gui --emulator --transcribe /bin/true` exits non-zero with "--transcribe needs --playlist".

- [ ] **Step 2: Run it to verify it fails, implement, run `cargo test`**

Expected: all pass.

- [ ] **Step 3: Emulator smoke test (spec 7, not a pass/fail on the text)**

Run: `pdm-eth gui --emulator --playlist <folder with factory_4workers_9s5_b270_f150> --transcribe pctools/audio_pipe/target/release/audio_pipe --transcript-log /tmp/smoke.log` for 60 s.
Expected: the lanes fill with text each pass, a talker returning in the next pass stays in its lane and colour; take a screenshot (`--screenshot`, after 30 s) for the how-to; the status line shows running; `/tmp/smoke.log` has marks and text lines; killing audio_pipe (`pkill -f "audio_pipe live"`) shows "starting" and text returns within about 20 s.

- [ ] **Step 4: Commit**

```bash
git add pc_app/src/main.rs pc_app/src/ui.rs pc_app/pdm-eth-demo.sh pc_app/README.md
git commit -m "feat(pdm_audio_eth): pdm-eth gui --transcribe and the booth launcher"
```

---

### Task 9: curation tooling

**Files:**
- Modify: `pctools/audio_pipe/tools/wer4.py`
- Create: `pctools/audio_pipe/tools/booth_curate.py`
- Create: `pctools/audio_pipe/tools/make_booth_scenes.sh`
- Modify: `.gitignore` for `pc_app/audio/booth/`

**Interfaces:**
- `wer4.py` keeps its CLI and output; adds importable `REF`, `norm(t)`, `wer(r, h)`, `score(streams, settings) -> (per_talker: list[(voice, az, found_az|None, wer, text)], extra: list[(az, text)])` where `streams` = `[{"speaker", "azimuth_deg", "text"}]` (the `transcript.json` shape). The 35° gate stays.
- `booth_curate.py <log> <playlist dir>`: groups the log's text lines by `pass`, joins the lines of one speaker in a pass into one stream text, scores each pass with `score()` against `<playlist dir>/<scene>/settings.json`, prints one line per pass (`scene pass n: mean WER x, extra [...]`) and a verdict per scene: `PASS` if the last 5 passes of that scene all have every talker at WER 0 and no extra, else `FAIL` with the reason. Exit code 0 only if every scene passes.
- `make_booth_scenes.sh`: renders each curated scene with `tools/render_mics.js` into `pc_app/audio/booth/<name>/` (overrides inline, one scene per block, comments say what each scene is), writes `dry/` too.

- [ ] **Step 1: Write the failing test**

`python3 -m pytest` is not set up in this crate, so add `tools/test_booth_curate.py` with plain `assert`s run by `python3 tools/test_booth_curate.py`:
- a log with 5 passes of a scene where every talker's text is exact → verdict `PASS`, exit 0.
- the same with one pass where worker B says "Their belt stopped five minutes ago." → `FAIL`, reason names worker B and the pass.
- a pass with an extra stream "I don't know." at 170° → `FAIL`, reason "extra text".

- [ ] **Step 2: Run it to verify it fails, implement, run it again**

Run: `python3 tools/test_booth_curate.py` and `python3 tools/wer4.py out_factory_4workers_9s5_b270_f150_board $SCENE_ROOT/audio_wav_factory_4workers_9s5_b270_f150` (unchanged output vs before).
Expected: tests pass; wer4 output identical.

- [ ] **Step 3: Commit**

```bash
git add pctools/audio_pipe/tools/wer4.py pctools/audio_pipe/tools/booth_curate.py pctools/audio_pipe/tools/test_booth_curate.py pctools/audio_pipe/tools/make_booth_scenes.sh .gitignore
git commit -m "feat(pdm_audio_eth): booth scene curation (per-pass WER verdict) and scene render script"
```

---

### Task 10: curate the factory scenes on the board

Iterative, on the board. Close any other `pdm-eth` first.

- [ ] **Step 1: Fix the female in `factory_4workers_9s5_b270_f150`**

Try, in this order, until she is exact through the file path (`board_bench.py` + `audio_pipe run` + `wer4.py`, as in the scene READMEs): distance 2.0 → 1.4 m; level -2 → 0 dB; another azimuth away from noise sources (e.g. 160-170°, between the conveyor at 180° and robot 1's path, or 0°); last resort a different female clip (a third TTS sentence like the workers', added to `wer4.py`'s `REF`). Keep the other three talkers as they are.

- [ ] **Step 2: Build 2-4 factory scenes with different layouts**

The fixed scene, plus 1-3 more (other azimuths and start orders, same 4 voices), each added to `make_booth_scenes.sh` and checked through the file path first.

- [ ] **Step 3: Live curation**

Run the GUI with the board: `pdm-eth gui --iface enx9c69d3c13719 --playlist pc_app/audio/booth --transcribe ... --transcript-log /tmp/curate.log` until every scene has had at least 5 passes, then `python3 tools/booth_curate.py /tmp/curate.log pc_app/audio/booth`.
Expected: `PASS` for every scene kept; drop or fix any scene that fails.

- [ ] **Step 4: Commit the render script with the final scenes**

```bash
git add pctools/audio_pipe/tools/make_booth_scenes.sh pctools/audio_pipe/tools/wer4.py
git commit -m "feat(pdm_audio_eth): curated factory booth scenes (render script)"
```

- [ ] **Step 5 (only if time allows): other settings**

Home, hospital, automobile: same procedure, a scene only if it passes curation.

---

### Task 11: how-to, design docs, soak

**Files:**
- Modify: `pdm_audio_eth/docs/demo_howto.md` (new section "Booth transcripts")
- Add: `pdm_audio_eth/docs/2026-11-07-booth-transcripts-design.md`, this plan

- [ ] **Step 1: Write the how-to section**

Start (`PDM_IFACE=enx9c69d3c13719 pdm-eth-demo`), what the visitor sees (lanes, final text only, ~0.5 s after a talker stops), the status line values and what to do for each (`off: model not found` → download the models, command; `off` after crashes → restart the demo), the one-`pdm-eth` rule, how to re-run curation (`booth_curate.py`), `make_booth_scenes.sh` on a new PC.

- [ ] **Step 2: Soak test, 1 hour on the board**

Run the launcher for 60 min with the transcript log; record at start and end: audio_pipe RSS (`ps -o rss`), pdm-eth RSS, shm overruns (status lines in the log), text latency range, and `booth_curate.py` on the log.
Expected: RSS growth < 10 %, 0 overruns, latency within 300-800 ms, every scene `PASS`. Put the numbers in the how-to section.

- [ ] **Step 3: Commit**

```bash
git add pdm_audio_eth/docs/demo_howto.md pdm_audio_eth/docs/2026-11-07-booth-transcripts-design.md pdm_audio_eth/docs/2026-11-07-booth-transcripts-plan.md
git commit -m "docs(pdm_audio_eth): booth transcripts how-to, design and plan, soak results"
```
