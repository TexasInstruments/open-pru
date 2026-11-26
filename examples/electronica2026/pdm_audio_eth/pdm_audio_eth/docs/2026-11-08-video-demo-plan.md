# Video demo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Readable fonts on a 4K booth screen, a Scene Simulator scene (Kokoro voices, booth preset) opened in the pdm-eth window, transcribed, and live listening to one mic channel or one separated talker.

**Architecture:** pc_app gains a UI scale, an "open scene…" path that swaps the running playlist for a one-scene playlist (new `NetCmd::SetPlaylist`), and a `Player` (an `aplay` child fed raw PCM) for channel audio. audio_pipe `live` gains a `listen_az` control line and plays the chosen talker's separated stream with its own `Player`. The Scene Simulator (`~/GitHub/scenes`) gains a Kokoro engine for "Typed text" and a "Factory (booth)" scenario. A board check picks Kokoro voices and the sentences the user may type.

**Tech Stack:** Rust (egui/eframe 0.37, clap, serde_json), audio_pipe (whisper-rs, CUDA), Scene Simulator (vanilla JS page built by `python build.py`, esbuild bundles, playwright tests), `kokoro-js` + onnxruntime-web, `aplay` (alsa-utils).

**Spec:** `pdm_audio_eth/docs/2026-11-08-video-demo-design.md` (builds on `2026-11-07-booth-transcripts-design.md`).

## Global Constraints

- `pc_app` builds and `cargo test --release` passes without CUDA, a board, a whisper model or an audio device. No new audio library in pc_app; the player is a child process.
- Run pc_app tests with `--release` (`stream_exports_aligned...` is timing-sensitive in debug).
- audio_pipe builds need `PATH=/usr/local/cuda/bin:$PATH CMAKE_CUDA_ARCHITECTURES=120`.
- The booth loop with no scene opened behaves exactly as curated (same scenes, same transcription); `pc_app/audio/booth` is not changed.
- Listening must never stall the spectrograms or the transcription: player writes never block the UI thread or audio_pipe's reader loop.
- Only one `pdm-eth` feeds the board; close any other before board runs. Every pc_app rebuild drops the raw-socket capability: board runs need `sudo setcap cap_net_raw,cap_net_admin+ep pc_app/target/release/pdm-eth` from the user (or `pdm-eth-demo`).
- Scene Simulator: edit `web/src/*` and `web/tools/*`, rebuild with `python build.py`, never hand-edit `web/robot_acoustic.html`; its tests stay green. Commits in that repo are separate; nothing is pushed in either repo.
- Deadline: all tasks done before 2026-11-10 (booth opens); order of work = task order.
- Commit messages end with the session's attribution lines.

## Review Focus

1. **Scene opened while audio_pipe is mid-pass**: the old pass ends, the next mark carries the new scene name and scale; no text from the old scene lands under the new scene's lane. (Task 2 test `open_scene_replaces_the_playlist` + audio_pipe's existing pass handling.)
2. **Player missing or dying** (`aplay` not installed, device busy, child exits): "listening" says "audio output unavailable", spectrograms and text continue, the next choice tries again. (Tasks 3 and 5 tests.)
3. **A stalled player pipe**: writes go through a bounded channel to a writer thread and are dropped when full; the UI frame time and audio_pipe's reader are unaffected. (Task 3 test `full_player_drops_not_blocks`.)
4. **Talker stream changes**: no stream near the azimuth (silence), the stream ends and a new one is born at a pass start (re-chosen, its replayed history not played), audio_pipe restarted (the GUI resends `listen_az`). (Task 5 tests.)
5. **Odd zips**: compressed entries, `mics/` vs flat layout, 7 mics, 22.05 kHz WAVs, a folder with no WAVs: refused with a message naming the problem, the running playlist untouched. (Task 2 tests.)

---

## File Structure

| File | Change | Responsibility |
|---|---|---|
| `pc_app/src/ui.rs` | modify | scale/fonts, open-scene button + `open_scene`, `NetCmd::SetPlaylist`, listen state, speaker toggles, lane-label clicks, "listening" side line |
| `pc_app/src/main.rs` | modify | `--ui-scale`, `--scene` |
| `pc_app/src/playlist.rs` | modify | `load_scene(path)` (folder or zip → one-scene `Playlist`) |
| `pc_app/src/scenezip.rs` | create | read a stored (uncompressed) zip into a temp folder |
| `pc_app/src/player.rs` | create | `Player`: `aplay` child, bounded writer thread |
| `pc_app/src/transcribe.rs` | modify | `TextPipe::listen(az)`, resent after restart |
| `pc_app/pdm-eth-demo.sh` | modify | `PDM_UI_SCALE` → `--ui-scale` |
| `pctools/audio_pipe/src/live.rs` | modify | `listen_az` control, stream choice, tap, player |
| `pctools/audio_pipe/src/player.rs` | create | same `Player` contract as pc_app (own copy; the crates share no code) |
| `pctools/audio_pipe/tools/page_export.py` | create | drive the Scene Simulator page headless: preset + texts → zip → playlist folder |
| `pctools/audio_pipe/tools/video_sentences.md` | create | the checked sentences per slot (for the user) |
| `~/GitHub/scenes/web/tools/build_kokoro.sh`, `kokoro_entry.js` | create | build `web/vendor/scene_kokoro.js` + `web/vendor/ort/*.wasm` |
| `~/GitHub/scenes/web/src/robot_acoustic.template.html` | modify | engine select, Kokoro speech path, licence line |
| `~/GitHub/scenes/web/src/scenarios.js` | modify | `factory_booth` scenario |
| `~/GitHub/scenes/web/tests/ui_speech.py` | modify | Kokoro case, booth preset case |
| `pdm_audio_eth/docs/demo_howto.md` | modify | 7.9 video flow, UI scale, listening |

---

### Task 1: UI scale and bigger top bar and lane text

**Files:** Modify `pc_app/src/ui.rs`, `pc_app/src/main.rs`, `pc_app/pdm-eth-demo.sh`.

**Interfaces:**
- Produces: `pub fn ui_scale(flag: Option<f32>, env: Option<&str>) -> f32` in `ui.rs` (flag wins, then env `PDM_UI_SCALE`, else 1.0; clamped to 0.5..=4.0; unparsable env → 1.0); constants `TOP_PX = 20.0`, `LANE_H = 40.0`, `LANE_LABEL_PX = 18.0`, `LANE_TEXT_PX = 21.0`, `LANE_MSG_PX = 22.0`.

- [ ] **Step 1: Write failing test** `ui::tests::ui_scale_flag_env_default`: `ui_scale(Some(1.75), Some("2")) == 1.75`; `ui_scale(None, Some("2")) == 2.0`; `ui_scale(None, Some("x")) == 1.0`; `ui_scale(None, None) == 1.0`; `ui_scale(Some(9.0), None) == 4.0`.
- [ ] **Step 2: Run** `cargo test --release --lib ui_scale` in `pc_app`. Expected: FAIL (no `ui_scale`).
- [ ] **Step 3: Implement** `ui_scale`; `gui --ui-scale F` (clap `Option<f32>`) → `ctx.set_zoom_factor(ui_scale(flag, env))` once at creation. Top bar: title, "channel", channel buttons, source label, view switch at `TOP_PX` (RichText size). Lanes: row height `LANE_H`, label font `LANE_LABEL_PX`, chip font `LANE_TEXT_PX`, chip radius = half the chip height, the "transcription off/…" message `LANE_MSG_PX`. `pdm-eth-demo.sh`: if `PDM_UI_SCALE` is set, add `--ui-scale "$PDM_UI_SCALE"`; help text names it.
- [ ] **Step 4: Run** the full suite (`cargo test --release` in pc_app). Expected: all pass.
- [ ] **Step 5: Screenshot** `pdm-eth gui --emulator --playlist pc_app/audio/booth --transcribe pctools/audio_pipe/target/release/audio_pipe --ui-scale 1.5 --screenshot S/scale.png --screenshot-after 25` and look at it. Expected: top bar and lane text clearly larger, nothing clipped, side panel visible.
- [ ] **Step 6: Commit** `feat(pdm_audio_eth): --ui-scale and larger top bar and transcript text`.

### Task 2: Open a Scene Simulator scene

**Files:** Create `pc_app/src/scenezip.rs`; modify `pc_app/src/playlist.rs`, `pc_app/src/ui.rs`, `pc_app/src/main.rs`, `pc_app/src/lib.rs`.

**Interfaces:**
- Consumes: `Playlist`, `Pass`, `GAP_FRAMES`, `load_set` (existing).
- Produces:
  - `scenezip::unpack(zip: &Path, into: &Path) -> Result<(), WavError>`: stored entries only; a compressed entry → `WavError("…: compressed zip entry <name>; unpack the zip and open the folder")`.
  - `playlist::load_scene(path: &Path) -> Result<Playlist, WavError>`: a `.zip` (unpacked to `std::env::temp_dir()/pdm-eth-scene-<pid>/<stem>/`), or a folder with `mic1..8.wav` flat or under `mics/`; one pass named after the zip stem / folder name; same `scale` and `GAP_FRAMES` as `load_playlist`. Not 8 mics, or not 16 kHz → error naming the scene.
  - `NetCmd::SetPlaylist { set: InputSet, marks: Vec<usize> }`: the net thread replaces `inp.set` and `inp.marks` and restarts the loop at its first frame (as `SetSet` restarts a set).
  - `App::open_scene(&mut self, path: &Path)`: on success sends `SetPlaylist`, sets `scenes = [name]`, `pass_scales = [scale]`, `pass = None`, clears lanes, source label `"scene <name>"`; on error sets `self.error`, changes nothing.
  - `gui --scene PATH` (conflicts with ch0..ch7 and `--set`; with `--playlist` → error "use --scene or --playlist"); `--transcribe` accepts `--scene` as it accepts `--playlist`.

- [ ] **Step 1: Write failing tests** in `playlist.rs`: `scene_folder_flat_and_mics_layout` (both layouts load, `passes.len() == 1`, name = folder name); `scene_zip_loads` (a stored zip written by the test with `mics/mic1..8.wav` + `settings.json` → loads, name = zip stem); `scene_compressed_zip_is_refused` (entry with method 8 → error contains "compressed"); `scene_with_7_mics_is_refused` (error contains "mic8"); `scene_22k_is_refused` (error contains "16 kHz"). In `ui.rs`: `open_scene_replaces_the_playlist` (App with commands + playlist ["factory_a","factory_b"]; `open_scene(folder)` → one `NetCmd::SetPlaylist` with `marks == [0]`, `app.scenes == ["myscene"]`, `pass_scales.len() == 1`, lanes empty) and `open_bad_scene_changes_nothing` (no command, `scenes` unchanged, `error` set).
- [ ] **Step 2: Run** `cargo test --release --lib scene`. Expected: FAIL (functions missing).
- [ ] **Step 3: Implement** the Interfaces block. The stored-zip reader walks local file headers (signature `PK\x03\x04`, method at offset 8, sizes at 18/22, name length 26, extra length 28); data descriptors are not used by the page's `makeZip`. UI: in the side panel above the locked Sources table, when a playlist runs: buttons **"open scene…"** (rfd file dialog, `.zip`) and **"open scene folder…"** (rfd folder dialog), both → `open_scene`.
- [ ] **Step 4: Run** the full suite. Expected: all pass.
- [ ] **Step 5: Smoke** on the emulator: start with the booth playlist and `--transcribe`, open `pc_app/audio/booth/factory_b` via `--scene` in a second run; screenshot after 30 s. Expected: IN label "factory_b (pass n)", text lanes fill.
- [ ] **Step 6: Commit** `feat(pdm_audio_eth): open a Scene Simulator scene (zip or folder) in the window`.

### Task 3: Player and channel listening

**Files:** Create `pc_app/src/player.rs`; modify `pc_app/src/ui.rs`, `pc_app/src/lib.rs`.

**Interfaces:**
- Produces:
  - `Player::spawn(cmd: &[String]) -> std::io::Result<Player>`; `Player::push(&self, pcm: &[i16])` (non-blocking: `sync_channel(32)` of chunks to a writer thread, `try_send`, a full channel drops the chunk); `Player::alive(&self) -> bool` (false after the child or writer ended); `Drop` kills and reaps the child.
  - `pub fn player_cmd() -> Vec<String>`: `PDM_PLAYER` split on whitespace if set, else `aplay -q -t raw -f S16_LE -r 16000 -c 1 --buffer-time=100000`.
  - `pub enum Listen { Off, Channel(usize), Talker(f32) }` on `App` (field `listen`), `App::set_listen(&mut self, l: Listen)`: same value again → `Off`; leaving `Channel` drops the player; leaving `Talker` calls `tp.listen(None)` (Task 5; until then a no-op).
  - Channel audio: in `drain` on `NetMsg::Out`, for `Listen::Channel(c)`, sample → `(x as f32 * scale * 32767.0 * 0.9).clamp(..) as i16` with the current pass's `scale` (playlist), or `x >> 8` without a playlist.
  - Side line "listening": `"off"`, `"ch3 board out"`, `"talker 2 · 211°"`, `"audio output unavailable"` (spawn failed or `!alive()`).
  - Top bar: after each channel button a small toggle (`"🔊"` if the font has it, else `"♪"`) selected when `Listen::Channel(c)`.

- [ ] **Step 1: Write failing tests**: `player::tests::player_writes_pcm` (`PDM_PLAYER`-style cmd `/bin/sh -c "cat > FILE"` via `Player::spawn`; push 1000 samples; drop; file has 2000 bytes, little-endian, same values); `player::tests::full_player_drops_not_blocks` (cmd `/bin/sh -c "sleep 5"`; 200 pushes of 16000 samples return within 200 ms); `player::tests::missing_player_is_an_error` (`spawn(["/no/such/player"])` is `Err`); `ui::tests::listen_channel_switches_and_toggles` (set_listen(Channel(3)) twice → `Off`; Channel(3) then Channel(5) → Channel(5)); `ui::tests::missing_player_says_unavailable` (player cmd `/no/such/player`; set_listen(Channel(0)) → side line "listening" == "audio output unavailable", `drain` of Out frames does not panic).
- [ ] **Step 2: Run** `cargo test --release --lib player listen`. Expected: FAIL.
- [ ] **Step 3: Implement** the Interfaces block (App holds `player_cmd: Vec<String>`, set by `with_player_cmd` for tests, default `player_cmd()`).
- [ ] **Step 4: Run** the full suite. Expected: all pass.
- [ ] **Step 5: By ear** on the emulator: toggle ch0 and ch5; hear the scene, in step with the spectrogram, no clicks every frame; toggle off = silence.
- [ ] **Step 6: Commit** `feat(pdm_audio_eth): live channel listening (board output) through aplay`.

### Task 4: Kokoro in the Scene Simulator, "Factory (booth)" scenario, sentence check

**Files (repo `~/GitHub/scenes`):** create `web/tools/build_kokoro.sh`, `web/tools/kokoro_entry.js`, `web/vendor/scene_kokoro.js`, `web/vendor/ort/` (onnxruntime-web wasm files); modify `web/src/robot_acoustic.template.html`, `web/src/scenarios.js`, `web/tests/ui_speech.py`, `README.md`/`CLAUDE.md` (features table).
**Files (this repo):** create `pctools/audio_pipe/tools/page_export.py`, `pctools/audio_pipe/tools/video_sentences.md`.

**Interfaces:**
- Produces:
  - `window.SceneKokoro = { speak(text, voice) -> Promise<{audio: Float32Array, sampling_rate: number}>, engine: "Kokoro 82M (kokoro-js), Apache-2.0" }`, model `onnx-community/Kokoro-82M-v1.0-ONNX`, dtype `q8`, wasm from `vendor/ort/`.
  - Speaker field `engine: "kokoro" | "espeak"` (default `"kokoro"`), select `k_${sid}_engine` shown with "Typed text"; `speechKey` includes the engine; `KOKORO_VOICE = { male, male_low, female, female_high }` → Kokoro voice ids (chosen in Step 6); Kokoro failure → status message naming the reason and "choose eSpeak".
  - Scenario `factory_booth` (label "Factory (booth)"): factory params, press at az 150, 9.5 s, speakers male 30° 1.5 m 0 dB 0.3 s; female 0° 1.5 m 0 dB 0.7 s; male_low 210° 1.8 m −1 dB 1.1 s; female_high 270° 2.5 m −3 dB 1.5 s; all `speech: "text"`, `engine: "kokoro"`, texts = one passing set from Step 8.
  - `page_export.py PAGE OUT --scene factory_booth --text SLOT=SENTENCE ... [--name NAME]`: serves the built page and `vendor/` locally (as `ui_speech.py`), selects the scenario, fills the texts, renders, saves the zip, writes `OUT/NAME/mic1..8.wav` + `settings.json` (flat, for `--playlist`).

- [ ] **Step 1: Write failing tests** in `ui_speech.py`: Kokoro typed text renders (speaker 1 text "Hi there, how are you doing today?", engine kokoro → zip has 8 mics, settings speaker 1 has `engine == "kokoro"` and the text; dry source RMS > 0); the `factory_booth` scenario loads with 4 typed-text speakers and the press at 150°.
- [ ] **Step 2: Run** `python tests/ui_speech.py` (built page). Expected: FAIL on the new cases (no engine, no scenario).
- [ ] **Step 3: Implement** the bundle (`build_kokoro.sh` as `build_tts.sh`: pinned `kokoro-js` and `esbuild` versions, copies the ort wasm files into `vendor/ort/`), the engine select and speech path (Kokoro output resampled with `toVoice16k`), the licence line in the page footer, the scenario. Rebuild `python build.py`.
- [ ] **Step 4: Run** `node tests/engine_regression.js`, `node tests/scenarios_test.js`, `python tests/ui_speech.py`. Expected: all pass (Kokoro needs the model download once: online).
- [ ] **Step 5: Commit (scenes repo)** `feat: Kokoro voices for typed text; Factory (booth) scenario`.
- [ ] **Step 6: Voice round on the board.** Two candidate Kokoro voices per slot (male, male_low, female, female_high); export with `page_export.py` 2 scenes (each candidate set) speaking the booth references (wer4 REF sentences, "The forklift is waiting at gate two." for female); loop on the board ≥ 6 passes each (`pdm-eth gui --iface enx9c69d3c13719 --playlist DIR --transcribe … --transcript-log LOG`), `booth_curate.py LOG DIR`. Expected: per slot one voice exact on every pass; set `KOKORO_VOICE`, rebuild, commit (scenes repo). A slot with no passing voice: try two more voices, then a moved position (±20°), then drop the slot (user decision: fewer talkers).
- [ ] **Step 7: Sentence round on the board.** Propose 8 sentences per slot (factory themes, 5-9 words, no numbers outside 2, 3, 4, 5, 10), 8 scenes of 4 (one per slot); export, loop ≥ 6 passes each, curate. Expected: a passing list per slot.
- [ ] **Step 8: Write** `video_sentences.md` (passing sentences per slot, the voice per slot, how to type them) and prefill `factory_booth` with one passing set; rebuild; re-export that set and confirm it passes on the board (≥ 6 passes). Commit both repos (`docs(pdm_audio_eth): checked sentences for the video`; scenes: `feat: prefill Factory (booth) with checked sentences`).

### Task 5: Talker listening

**Files:** Create `pctools/audio_pipe/src/player.rs`; modify `pctools/audio_pipe/src/live.rs`, `pctools/audio_pipe/src/main.rs`; modify `pc_app/src/transcribe.rs`, `pc_app/src/ui.rs`.

**Interfaces:**
- Consumes: `Listen`, `App::set_listen` (Task 3); `Stream { id, az, pcm: Vec<f32>, origin: i64 }` (audio_pipe `sep.rs`, `pcm` = separated 16 kHz output from sample `origin`).
- Produces:
  - audio_pipe `player.rs`: same contract as pc_app's (`spawn`, non-blocking `push`, `alive`, Drop), command from `PDM_PLAYER` or the same `aplay` default.
  - `live::handle_listen(line: &str) -> Option<Option<f32>>`: `{"listen_az":211.5}` → `Some(Some(211.5))`, `{"listen_az":null}` → `Some(None)`, anything else `None`; the run loop tries it before `handle_control`.
  - `live::choose(streams: &[(usize, f32)], az: f32) -> Option<usize>`: id of the stream with the smallest circular azimuth distance ≤ 20°.
  - `live::Tap { id: usize, upto: i64 }` and `Tap::take<'a>(&mut self, origin: i64, pcm: &'a [f32]) -> &'a [f32]`: the samples after `upto`, then `upto` = end. On a new choice `upto` = the stream's current end (history not played).
  - In `Live`: `listen_az: Option<f32>`, `tap: Option<Tap>`, `player: Option<Player>`; after each `pipe.process`: if `listen_az` is set and `tap` is none or its stream is gone, `choose` again; push `take(...)` × fixed gain 4.0 (×32767, clamped) to the player; `begin_pass` clears `tap`. A player spawn failure is reported once on stderr and as status detail, never an error.
  - pc_app `TextPipe::listen(&mut self, az: Option<f32>)`: writes `{"listen_az":…}`, remembers it, resends it after each restart. Lane label click → `set_listen(Listen::Talker(lane.az))`; `set_listen` to anything else sends `listen(None)`; choosing a talker drops the window's channel player.

- [ ] **Step 1: Write failing tests**: audio_pipe `listen_line_parses`; `choose_nearest_within_20_deg` (streams `[(1,30.0),(2,212.0),(3,355.0)]`: az 5 → 3; az 211 → 2; az 120 → None); `tap_skips_history_and_takes_new` (new tap on a stream with origin 0, len 1000 → upto 1000, take → empty; pcm grows to 1300 → take returns 300 samples, then empty); `listen_survives_pass_change` (Live with `--no-asr`, `PDM_PLAYER`-style fake player writing a file: listen 30.0, two passes → no panic, tap cleared at the second pass start). pc_app `transcribe::tests::listen_is_sent_and_resent_after_restart` (fake child `/bin/sh` that appends stdin to a file and exits after 1 s: `listen(Some(211.0))`, restart, file contains `"listen_az":211` twice); `ui::tests::talker_and_channel_exclude_each_other` (Channel(2) then Talker(211.0) → channel player dropped, listen == Talker; then Channel(2) → `tp.listen(None)` written).
- [ ] **Step 2: Run** `cargo test --release live::` (audio_pipe) and `cargo test --release --lib listen` (pc_app). Expected: FAIL.
- [ ] **Step 3: Implement** the Interfaces block. Lane label hit areas: one `ui.interact(rect, id, Sense::click())` per lane label rect in `draw_lanes`; the active lane's label drawn highlighted (white ring around the dot).
- [ ] **Step 4: Run** both full suites. Expected: all pass.
- [ ] **Step 5: By ear on the board** (factory_a): click each lane: one talker, clearly less noise than a channel; about 0.3 s behind; switching to a channel stops the talker; restarting audio_pipe (kill it) → after it is back, the talker plays again without a new click.
- [ ] **Step 6: Commit** `feat(pdm_audio_eth): live talker listening (separated stream) from audio_pipe`.

### Task 6: Docs, board check of the whole flow

**Files:** Modify `pdm_audio_eth/docs/demo_howto.md`, `pc_app/README.md`.

- [ ] **Step 1: Write** demo_howto section "7.9 Video: typed scene to transcript and listening": serve the page (`python -m http.server` in `~/GitHub/scenes/web`, online once for Kokoro), pick "Factory (booth)", type sentences from `video_sentences.md`, download the zip, "open scene…", toggles and lane clicks, `PDM_UI_SCALE` (try 1.5-2.0 on the 32" 4K monitor). pc_app README rows for `--ui-scale`, `--scene`, `PDM_PLAYER`.
- [ ] **Step 2: Full flow on the board**: page → zip → window (booth playlist running, `--transcribe`) → "open scene…" → ≥ 6 passes, `booth_curate.py` on the log (fresh log) says PASS; channel and talker listening by ear. Expected: PASS, both audible.
- [ ] **Step 3: Run** both suites; commit `docs(pdm_audio_eth): video flow (7.9), UI scale, listening`.

### Task 7: Talker distance estimate (added 2026-11-08 at the user's request)

The user asked for the talker distance next to the angle in the lane labels and chose an estimate from the audio
(not the scene's true distance) after being told the array cannot measure it directly.

**Files:** modify `pctools/audio_pipe/src/live.rs` (and the stream/covariance code it needs), `pc_app/src/transcribe.rs`
(`TextLine.dist_m: Option<f32>`), `pc_app/src/lanes.rs`, `pc_app/src/ui.rs`.

- Estimator: per utterance, the directness of the talker's spatial covariance in 1.5-4 kHz (principal eigenvalue /
  trace, the coherent share), mapped to DRR and to metres with d = d_c / sqrt(DRR), d_c calibrated on Scene
  Simulator renders at known distances (1, 1.5, 2, 2.5, 3, 4 m) in the factory hall.
- Gate: shown only if the calibration error on held-out renders and the booth scenes is within ±0.5 m (or ±25 %);
  displayed rounded to 0.5 m as "≈1.5 m". Otherwise not shown, and the user gets the numbers.
- Timebox: about 2 hours; the text line gains `"dist_m"` (absent when unknown), the lane label shows it.
- Tests: text line with and without `dist_m` parses; lane label with distance; estimator unit test on a synthetic
  rank-1 + diffuse covariance (directness rises with DRR, monotonic).
