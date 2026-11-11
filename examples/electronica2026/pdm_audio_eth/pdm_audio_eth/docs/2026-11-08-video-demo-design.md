# Video demo: typed scene to board, transcript and live listening

Design, 2026-11-08. Builds on the booth transcripts
(`2026-11-07-booth-transcripts-design.md`, implemented on branch
`dev_tley_electronica2026_pdm_audio_eth`). Calls marked *(builder's call)*
were made by me, the author, not by the user.

## 1. Intent

The user records a video that explains the demo in one take:

1. In the **Scene Simulator** (`~/GitHub/scenes`, repo `accio-sunshine/scenes`,
   page `web/robot_acoustic.html`) type a sentence for each talker, with a
   natural-sounding voice, and click "Download .zip".
2. In the **pdm-eth window** open that zip: it loops through the LP-AM243
   board, the spectrograms run, and each talker's sentence appears in its
   text lane.
3. **Listen live**: the noisy capture of one mic channel (board output), or
   the separated stream of one talker, one at a time.

The window runs on a 32" 4K monitor at the booth; today the top bar
(channel buttons) and the transcript text are too small to read there.

## 2. Goals, non-goals, anti-goals

**Goals**
- **Bigger text on a 4K screen**: the whole window scales evenly, and the top
  bar and the transcript text are larger than today at any scale.
- **Kokoro voices in the Scene Simulator** (decided by the user): "Typed
  text" can be spoken by Kokoro (82M, `kokoro-js`, Apache-2.0) instead of
  eSpeak. eSpeak stays as the offline fallback.
- **Open a scene in the window**: a zip from the Scene Simulator (or the
  folder it unpacks to) replaces the playlist and loops, transcribed as the
  booth scenes are.
- **Live listening** (decided by the user: live, not replay):
  - a channel: that channel's **board output** (the noisy mic capture
    after the PDM round trip), in step with the spectrogram;
  - a talker: that talker's **separated stream** from audio_pipe (the
    signal whisper hears), about 0.3 s behind the picture;
  - one source at a time; choosing another switches, choosing the active
    one again stops; out of the PC's default audio output.
- **A "Factory (booth)" preset in the Scene Simulator** (decided by the
  user): the booth layout (press at 150°, the four talker places, levels and
  start times of `make_booth_scenes.sh`), each talker on "Typed text" with
  its Kokoro voice. On camera: click the preset, type, export.
- **Pre-checked sentences** (decided by the user: fixed sentences, chosen
  from many proposed): the builder proposes many short factory sentences
  (about 30), checks each in its talker slot of the booth preset on the
  board, and gives the user the list that passes to pick from.
- **Everything before Tuesday** (decided by the user): all four pieces are
  ready before the booth opens on 2026-11-10. Order of work: fonts and
  scale (the booth needs them), "open scene…", Kokoro + preset + sentence
  check, listening. The user is told early if listening slips.

**Non-goals**: recording video or audio from the window; choosing the audio
output device; several scenes opened from the window at once; changing what
the booth loop does when no scene is opened; replay of a past pass.

**Anti-goal**: a wrong transcript on screen in the video, as at the booth
(see 6, voice check). A second anti-goal: listening must never stall the
spectrograms or the transcription (an audio device that is missing or busy
gives a message, nothing else).

## 3. Scene Simulator: Kokoro

- A choice next to "Typed text": engine **Kokoro** or **eSpeak**. Default
  Kokoro *(builder's call)*; if Kokoro cannot load (offline, first use) the
  page says so and offers eSpeak.
- Built like eSpeak today: a browser bundle in `web/vendor/` made by a
  script in `web/tools/` (as `build_tts.sh` + `scene_tts_entry.js`), loaded
  on first use. The model (`onnx-community/Kokoro-82M-v1.0-ONNX`, quantised,
  about 90 MB) is fetched from Hugging Face on first use and cached by the
  browser; it is not committed. Licence line on the page as for eSpeak.
- **"Factory (booth)" preset**: next to the page's scenarios, the Factory
  scene with the stamping press at 150° and the booth talkers (male 30°
  1.5 m 0 dB 0.3 s; female 0° 1.5 m 0 dB 0.7 s; male_low 210° 1.8 m -1 dB
  1.1 s; female_high 270° 2.5 m -3 dB 1.5 s; 9.5 s), speech "Typed text",
  engine Kokoro, prefilled with checked sentences. The stock Factory preset
  stays as it is.
- Voice slots: the page's four speech voices (male, male_low, female,
  female_high) each map to one Kokoro voice *(builder's call which, picked by
  the board check in 6)*. Output resampled to 16 kHz by the page's existing
  `toVoice16k`.
- `settings.json` records the engine and Kokoro voice per talker, and the
  typed text (it already records the text), so a scene can be scored and
  re-rendered.
- The repo's own rules apply: edit `web/src/*`, rebuild with
  `python build.py`, never edit `robot_acoustic.html` by hand; its tests
  (`tests/ui_speech.py`, the regression tests) stay green and gain a Kokoro
  case.

## 4. pdm-eth window

**Scale and fonts**
- `pdm-eth gui --ui-scale F` (and `PDM_UI_SCALE` in `pdm-eth-demo`), the egui
  zoom factor, default 1.0. *(builder's call: the user sets it once for the
  booth monitor; 1.5-2.0 expected for 32" 4K)*
- Independently of the scale: top-bar text (title, channel buttons, view
  switch) and the lane text (labels and sentences) grow by about 1.5x over
  today; the lane height follows.

**Open a scene**
- An **"open scene…"** button (side panel, above the locked Sources table)
  takes a Scene Simulator zip, or a folder holding `mic1..8.wav` directly or
  under `mics/` (the zip's layout). It is also a CLI option
  `--scene PATH` *(builder's call)*.
- The opened scene becomes a one-scene playlist (same 1 s gap, pass numbers,
  transcription, lanes). It replaces the booth playlist until the window is
  restarted. A scene that is not 8 mics of 16 kHz is refused with a message;
  the playlist keeps running.
- Available whenever the window runs a playlist (booth mode). The Sources
  table stays locked as today.

**Listening**
- Top bar: each channel button gets a small speaker toggle; on = hear that
  channel's board output. Lanes: clicking a lane's label (colour dot, number,
  direction) = hear that talker. The active source is highlighted; a side
  line "listening" names it ("ch3 board out", "talker 2 · 211°", "off").
- **Channel audio** is played by the window from the board output it already
  receives (24-bit, scaled to a comfortable fixed level).
- **Talker audio** is played by audio_pipe, which has the separated streams:
  the window sends a control line `{"listen_az": <deg>}` or
  `{"listen_az": null}` on audio_pipe's stdin. audio_pipe plays the stream
  of the current pass nearest that azimuth (within 20°, the lane-reuse rule),
  and picks it again at every pass start, because stream ids are new each
  pass. No stream there = silence, not an error.
- One at a time: choosing a channel sends `listen_az: null` first; choosing a
  talker stops the window's own channel output.
- **Audio output** *(builder's call)*: a child process `aplay` (or `pw-play`)
  fed raw 16 kHz mono S16_LE on stdin, with a short buffer (about 0.1 s), in
  both programs. No new audio library in pc_app (constraint of the booth
  design). If the player is missing or exits, the "listening" line says
  "audio output unavailable" and nothing else changes.
- Separated-stream level: the stream as whisper gets it, with a fixed gain
  or a slow AGC so quiet talkers are audible *(builder's call)*.

## 5. Constraints

- As the booth design: Rust, egui/eframe, `pc_app` builds and tests without
  CUDA, a board or audio hardware; one `pdm-eth` feeds the board; booth PC =
  this PC.
- The booth loop (no scene opened) must behave exactly as curated: same
  scenes, same transcription.
- Deadline: the booth opens Tuesday 2026-11-10; the video is recorded before
  or at the booth.
- Scene Simulator changes go to its own repo (`~/GitHub/scenes`), as their own
  commits; nothing pushed without the user asking.

## 6. Checking it

- **Unit tests (no GPU, no audio device)**: scene zip/folder loading (both
  layouts, 8-mic check); `listen_az` control parsing and stream choice by
  azimuth across a pass change; one-source-at-a-time switching; a missing
  player gives the message and no panic; UI scale applied.
- **Scene Simulator**: its existing tests plus a Kokoro typed-text case.
- **Sentence check on the board**: about 30 proposed sentences, each spoken
  by its slot's Kokoro voice in the "Factory (booth)" preset, rendered as
  4-talker scenes, looped through the board and scored with
  `booth_curate.py` (references from `settings.json` "text"); the booth
  rule: 5 passes in a row, every talker exact. The user gets the passing
  sentences per slot. The Kokoro audio for the check comes from the same
  engine and voices the page uses *(builder's call: in node or headless
  browser)*.
- **If few pass** (decided by the user): film with fewer talkers, keeping
  the slots and sentences that pass.
- **By ear**: the channel and talker audio are listened to once on this PC;
  the talker stream must sound like one person, clearly less noisy than the
  channel.

## 7. Settled by the builder (from the builder check)

- Filming runs on this PC, the page served locally (`python -m http.server`,
  as typed speech needs today), online once so Kokoro is cached;
  onnxruntime-web's wasm files are served from `web/vendor/`, not a CDN.
- audio_pipe picks the talker stream at each pass start and also when a new
  stream appears within 20° of the chosen azimuth; the window sends the
  listening choice again after an audio_pipe restart.
- A new stream's start-up audio (its replayed history) is not played.
- Page zips (clipped at ±1, not normalised) are loaded as capture sets
  today: one gain for the set, peak-normalised.

## 8. Left to the builder

Which Kokoro voice per slot; the Kokoro bundle build; the "open scene…"
placement and the speaker-toggle drawing; the exact font sizes; the player
command and buffer; the separated-stream gain.
