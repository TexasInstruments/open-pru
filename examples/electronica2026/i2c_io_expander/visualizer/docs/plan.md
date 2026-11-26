# I2C LED visualizer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Rust desktop app that shows the TCA9538's 8 LEDs, driven live by PicoScope 7's I2C decode, next to the PicoScope 7 window.

**Architecture:** A polling thread talks JSON-RPC (MCP streamable HTTP) to PicoScope 7, feeds every reply into a pure state machine (`Tracker`), and publishes a `Snapshot` behind `Arc<Mutex<_>>`. The egui GUI only draws the latest snapshot and never touches the network. All protocol parsing and state logic is pure and unit-tested; HTTP is tested against a std `TcpListener` fake server.

**Tech Stack:** Rust stable (rustup), `eframe`/`egui`, `ureq` (blocking HTTP), `serde_json`, `clap` (derive). Use the current crates.io versions at build time.

**Spec:** `docs/design.md` (same folder). Read it first.

Crate root: `open-pru/examples/electronica2026/i2c_io_expander/visualizer/` (called `visualizer/` below). Branch `dev_tley_electronica2026_i2c_io`. Commit each task locally with `git -c user.name="Thomas Leyrer" -c user.email="t-leyrer@ti.com" commit`, message prefix `examples: electronica2026: visualizer:`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Never push.

## Global Constraints

- Data only through PicoScope 7's MCP server, `POST http://127.0.0.1:<port>/mcp`; never the Pico SDK or USB.
- Default URL `http://127.0.0.1:18123/mcp`, option `--url`.
- Poll interval default 50 ms, start-to-start, never overlapping, option `--interval-ms`.
- HTTP timeout 3 s. Reconnect attempts about once per second.
- `no I2C frames` after 2 s of replies with no valid frame.
- Address query (`groupBy:"Address"`) about once per second.
- Valid frame key: exactly two hex bytes, first is `01`, e.g. `"01 08 "`. Ignore anything else (`"01 "`, `"03 00 "`).
- Never change trigger, timebase, channels, or other decoders. Decoder add args exactly: `{"action":"Add","protocol":"I2C","name":"TCA9538","channels":{"Data":"D1","Clock":"D0"},"settings":{"Bus Speed":"Fast-mode Plus (1 Mbit/s)"},"enabled":true}`.
- No easing, fade, prediction, queue, or replay of missed steps. Repaint as soon as the value changes.
- Status texts exactly: `waiting for PicoScope 7 …`, `no I2C frames`, `last frame: addr 0x23 · reg 0x01 · data 0x08` (address shown as `addr ?` when unknown, never hard-coded).
- Tooltip on the chip: `I2C LED visualizer v{CARGO_PKG_VERSION}`. F11 toggles fullscreen.
- Linux (Wayland) only. Not added to `examples/makefile`. `.gitignore` has `target/`.

## Review Focus

1. PicoScope 7 not running at startup (connection refused): window opens at once, LEDs dark, `waiting for PicoScope 7 …`, retries every ~1 s, no busy loop. Test: Task 3 `poll_once_error_goes_waiting` + Task 4 `retry_delay_is_one_second_when_waiting`.
2. PicoScope 7 restarted mid-run (old `Mcp-Session-Id` rejected with HTTP 404/400): client re-initializes without an app restart. Test: Task 2 `reinitializes_after_session_rejected`.
3. A 1 s PicoScope stall: not a disconnect; LEDs hold. A reply after more than 3 s is an error. Test: Task 2 `slow_reply_under_timeout_ok` / `reply_over_timeout_is_error`; Task 3 `slow_reply_does_not_change_state`.
4. Odd decode keys (`"01 "`, `"03 00 "`, `"zz 01 "`, empty groups, two frames in one capture). Test: Task 1 `parse_*` and `pick_*` tests.
5. Decoder deleted by the user while running (summary returns `status` other than `Success`): re-run decoder setup, don't crash. Test: Task 3 `summary_rejected_reruns_setup`.

---

### Task 1: Toolchain, crate skeleton, frame parsing

**Files:**
- Create: `visualizer/Cargo.toml`, `visualizer/.gitignore`, `visualizer/src/main.rs` (stub `fn main() {}`), `visualizer/src/lib.rs`, `visualizer/src/decode.rs`

**Interfaces:**
- Produces (`decode.rs`, re-exported from `lib.rs`):
  - `pub fn parse_frame_key(key: &str) -> Option<u8>`: data byte of a valid output-register frame.
  - `pub fn parse_address_key(key: &str) -> Option<u8>`: `"23 "` → `Some(0x23)`.
  - `pub fn successor(v: u8) -> u8`: `v.rotate_left(1)`.
  - `pub enum Pick { New(u8), Keep, NoFrame }` (derive Debug, PartialEq, Clone, Copy).
  - `pub fn pick(prev: Option<u8>, candidates: &[u8]) -> Pick`.

- [ ] **Step 1: Install Rust if `cargo` is missing.** Run `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y --profile minimal`, then `source ~/.cargo/env && cargo --version`. Expected: a version line.
- [ ] **Step 2: Create the crate.** `Cargo.toml`: package `i2c_led_visualizer`, version `0.1.0`, edition 2021, `[lib] name = "viz"`, binary name `i2c_led_visualizer`. Dependencies `eframe`, `ureq`, `serde_json`, `clap` (feature `derive`). `.gitignore`: `target/`. Add the BSD-3-Clause SPDX header plus TI copyright line used in `../i2c_io/firmware/main.asm` to every `.rs` file.
- [ ] **Step 3: Write failing tests** in `decode.rs` `#[cfg(test)]`:

```rust
#[test] fn parse_valid_frame() { assert_eq!(parse_frame_key("01 08 "), Some(0x08)); assert_eq!(parse_frame_key("01 80"), Some(0x80)); }
#[test] fn parse_rejects_partial() { assert_eq!(parse_frame_key("01 "), None); }
#[test] fn parse_rejects_config_frame() { assert_eq!(parse_frame_key("03 00 "), None); }
#[test] fn parse_rejects_three_bytes_and_junk() { assert_eq!(parse_frame_key("01 08 09 "), None); assert_eq!(parse_frame_key("zz 01 "), None); assert_eq!(parse_frame_key(""), None); }
#[test] fn parse_address() { assert_eq!(parse_address_key("23 "), Some(0x23)); assert_eq!(parse_address_key("x"), None); }
#[test] fn successor_wraps() { assert_eq!(successor(0x01), 0x02); assert_eq!(successor(0x80), 0x01); }
#[test] fn pick_none() { assert_eq!(pick(Some(0x04), &[]), Pick::NoFrame); }
#[test] fn pick_single() { assert_eq!(pick(Some(0x04), &[0x08]), Pick::New(0x08)); assert_eq!(pick(None, &[0x20]), Pick::New(0x20)); }
#[test] fn pick_single_same_is_new() { assert_eq!(pick(Some(0x08), &[0x08]), Pick::New(0x08)); }
#[test] fn pick_many_takes_successor() { assert_eq!(pick(Some(0x04), &[0x40, 0x08]), Pick::New(0x08)); }
#[test] fn pick_many_without_successor_keeps() { assert_eq!(pick(Some(0x04), &[0x40, 0x10]), Pick::Keep); assert_eq!(pick(None, &[0x01, 0x02]), Pick::Keep); }
```

- [ ] **Step 4: Run** `cargo test --lib decode`. Expected: compile errors / FAIL.
- [ ] **Step 5: Implement** the five items. Hex bytes are split on whitespace, each must parse as exactly 2 hex digits.
- [ ] **Step 6: Run** `cargo test --lib decode`. Expected: all pass. `cargo build` builds (pulls eframe; first build takes minutes).
- [ ] **Step 7: Commit** `Cargo.toml Cargo.lock .gitignore src/` (commit `Cargo.lock`: it is an application).

### Task 2: MCP client over HTTP

**Files:**
- Create: `visualizer/src/mcp.rs`, `visualizer/tests/mcp_http.rs` (fake server helper inside the test file)

**Interfaces:**
- Produces:
  - `pub trait ToolCaller { fn call_tool(&mut self, name: &str, args: serde_json::Value) -> Result<serde_json::Value, McpError>; }`. Returns the inner tool JSON (the parsed `result.content[0].text`), e.g. `{"result":{...},"status":"Success",...}`.
  - `pub enum McpError { Connect(String), Timeout, Http(u16), Protocol(String) }` (Debug, Display).
  - `pub struct McpClient`; `pub fn McpClient::new(url: &str, timeout: Duration) -> Self`. Lazily runs `initialize` (protocolVersion `"2025-06-18"`, clientInfo name `i2c_led_visualizer`, version from `CARGO_PKG_VERSION`) + `notifications/initialized` on first call; keeps `Mcp-Session-Id`.
  - `impl ToolCaller for McpClient`.

- [ ] **Step 1: Write failing tests** with a fake server: `fn fake_server(handler: impl Fn(Request) -> Response + Send + 'static) -> String` (binds `127.0.0.1:0`, thread, returns the URL; parses HTTP/1.1 POST with `Content-Length`, records method + `Mcp-Session-Id`). Tests:
  - `initializes_then_calls_tool`: server answers `initialize` with header `Mcp-Session-Id: s1`, `202` to the notification, and a `tools/call` with `{"jsonrpc":"2.0","id":2,"result":{"content":[{"type":"text","text":"{\"status\":\"Success\",\"result\":{\"x\":1}}"}]}}`. Assert return `["result"]["x"] == 1`, and that the `tools/call` request carried `Mcp-Session-Id: s1`.
  - `parses_sse_response`: same tool reply but as `Content-Type: text/event-stream`, body `event: message\ndata: {...}\n\n`. Same assertion.
  - `reinitializes_after_session_rejected`: first `tools/call` with session `s1` → `404`; the client must re-`initialize` (server now gives `s2`) and retry once; assert success and that the retry carried `s2`.
  - `slow_reply_under_timeout_ok`: server sleeps 1 s before the tool reply; timeout 3 s; assert `Ok`.
  - `reply_over_timeout_is_error`: server sleeps 4 s; timeout 3 s; assert `Err(McpError::Timeout)` (or `Connect` if ureq reports it that way; pick one mapping and assert it).
  - `connection_refused_is_connect_error`: URL to a closed port → `Err(McpError::Connect(_))`.
  - `jsonrpc_error_is_protocol_error`: reply `{"jsonrpc":"2.0","id":2,"error":{"code":-32602,"message":"bad"}}` → `Err(McpError::Protocol(_))`.
- [ ] **Step 2: Run** `cargo test --test mcp_http`. Expected: FAIL.
- [ ] **Step 3: Implement** `mcp.rs` with `ureq` (global timeout = given duration; treat non-2xx as `Http(code)`). Session-rejected = `Http(400 | 404)` on a call with a session id: drop the session, initialize, retry once.
- [ ] **Step 4: Run** `cargo test --test mcp_http`. Expected: all pass.
- [ ] **Step 5: Smoke test against the real PicoScope** (PicoScope 7 open, MCP on): add a `#[ignore]` test `real_picoscope_lists_decoders` calling `get_serial_decoder_list` on `http://127.0.0.1:18123/mcp` (or env `PICO_MCP_URL`). Run `cargo test --test mcp_http -- --ignored`. Expected: pass, or a `Connect` error if PicoScope's port is not 18123: then read the port with `ss -ltnp | grep PicoScope.GTK` and rerun with `PICO_MCP_URL`.
- [ ] **Step 6: Commit.**

### Task 3: Decoder setup and the Tracker state machine

**Files:**
- Create: `visualizer/src/setup.rs`, `visualizer/src/tracker.rs`

**Interfaces:**
- Consumes: `ToolCaller`, `McpError` (Task 2); `parse_frame_key`, `parse_address_key`, `pick`, `Pick`, `successor` (Task 1).
- Produces:
  - `setup.rs`: `pub fn ensure_decoder(c: &mut dyn ToolCaller) -> Result<String, McpError>` (decoder id). Rules from the spec (Behaviour 10): first enabled `protocol == "I2C"` with role `Data` on `D1` and `Clock` on `D0`; if only a disabled match, call `configure_serial_decoder` `{"action":"Configure","id":<id>,"enabled":true}`; if none, the exact Add args from Global Constraints and read `decoderId` from the reply.
  - `tracker.rs`:
    - `pub enum Conn { Waiting, Live, NoFrames }`
    - `#[derive(Clone)] pub struct Snapshot { pub conn: Conn, pub value: Option<u8>, pub address: Option<u8> }` with `pub fn lit(&self) -> [bool; 8]` (all false when `Waiting`; else bit i of `value` → index i, P0 = index 0) and `pub fn status_text(&self) -> String` (exact texts from Global Constraints; `Live` and `NoFrames`-before-any-frame details below).
    - `pub struct Tracker`; `Tracker::new() -> Self` (starts `Waiting`, no value).
    - `pub fn poll_once(&mut self, c: &mut dyn ToolCaller, now: Instant) -> PollResult` with `pub struct PollResult { pub changed: bool, pub skipped: bool }`. Does: ensure the decoder id is known (call `ensure_decoder` if not); call `get_serial_decoder_summary` `{"id":id,"groupBy":"Data","topK":5}`; parse group keys with `parse_frame_key`; apply `pick`; update `conn`/`value`; every ≥ 1 s (by `now`) also query `groupBy:"Address"` and set `address` from the first parsable key. On any `Err`: `conn = Waiting`, forget the session's decoder id. On a reply whose `status != "Success"`: forget the decoder id (next poll re-runs setup), state unchanged.
    - `pub fn snapshot(&self) -> Snapshot`.
  - Status text when `Live`/`NoFrames` with a value: `last frame: addr 0x23 · reg 0x01 · data 0x08` (`addr ?` if no address). `NoFrames`: `no I2C frames`. `Live` before any value: `no I2C frames` is not shown until 2 s pass; show `waiting for I2C frames …` meanwhile *(plan-level choice; spec silent)*.
  - `skipped` is true when the new value is not `successor(prev)` and `prev` was `Some`.

- [ ] **Step 1: Write failing tests** with `struct FakeCaller { replies: VecDeque<Result<Value, McpError>>, calls: Vec<(String, Value)> }`:
  - `ensure_uses_enabled_match`: list has a disabled D1/D0 decoder id "7" and an enabled one id "9" → returns "9", no configure call.
  - `ensure_enables_disabled_match`: only disabled id "7" → one `configure_serial_decoder` call with `{"action":"Configure","id":"7","enabled":true}`, returns "7".
  - `ensure_adds_when_missing`: list empty → Add call with the exact Global Constraints args, reply `{"status":"Success","decoderId":"103"}` → "103".
  - `ensure_ignores_other_channels`: an I2C decoder on D2/D3 only → Add.
  - `poll_once_goes_live_with_value`: summary groups `[{"key":"01 08 "}]` → snapshot `Live`, `value == Some(0x08)`, `lit()[3]` true and others false, `changed == true`.
  - `poll_once_same_value_not_changed`: two polls with `"01 08 "` → second `changed == false`, still `Live`.
  - `poll_once_counts_skip`: `"01 02 "` then `"01 08 "` → second `skipped == true`.
  - `no_frames_after_two_seconds`: `"01 08 "`, then replies with `[{"key":"01 "}]` at now+1.0 s (still `Live`) and now+2.1 s → `NoFrames`, `value` still `Some(0x08)`, `lit()[3]` still true.
  - `poll_once_error_goes_waiting`: `Err(McpError::Connect(..))` → `Waiting`, `lit()` all false, status `waiting for PicoScope 7 …`.
  - `slow_reply_does_not_change_state`: tracker logic has no reply-duration input; assert a `Live` tracker stays `Live` across a poll whose `now` is 1.2 s after the previous one with a valid frame (documents that stalls are mirrored).
  - `summary_rejected_reruns_setup`: summary reply `{"status":"Rejected","message":"Unknown decoder"}` → next `poll_once` calls `get_serial_decoder_list` again.
  - `address_queried_once_per_second`: polls at t=0, 0.05, 1.05 s → exactly two `groupBy:"Address"` calls; with key `"23 "` the status text contains `addr 0x23`; with no address reply, `addr ?`.
- [ ] **Step 2: Run** `cargo test --lib`. Expected: FAIL.
- [ ] **Step 3: Implement** `setup.rs` and `tracker.rs`.
- [ ] **Step 4: Run** `cargo test`. Expected: all pass.
- [ ] **Step 5: Commit.**

### Task 4: Poller thread, stats, CLI, headless mode

**Files:**
- Create: `visualizer/src/poller.rs`, `visualizer/src/stats.rs`, `visualizer/src/cli.rs`
- Modify: `visualizer/src/main.rs`

**Interfaces:**
- Consumes: `McpClient`, `Tracker`, `Snapshot`, `PollResult`.
- Produces:
  - `cli.rs`: `#[derive(clap::Parser)] pub struct Args { #[arg(long, default_value = "http://127.0.0.1:18123/mcp")] pub url: String, #[arg(long, default_value_t = 50)] pub interval_ms: u64, #[arg(long)] pub stats: bool, #[arg(long)] pub no_gui: bool }`.
  - `poller.rs`: `pub fn next_delay(conn: Conn, interval: Duration, elapsed: Duration) -> Duration` (Waiting → 1 s; else `interval.saturating_sub(elapsed)`, so a reply slower than the interval is followed at once). `pub fn spawn(args: &Args, shared: Arc<Mutex<Snapshot>>, on_change: Box<dyn Fn() + Send>) -> JoinHandle<()>`: loop `poll_once` → store snapshot → call `on_change` when `changed` or `conn` changed → feed stats → sleep `next_delay`.
  - `stats.rs`: `pub struct Stats`; `record(&mut self, reply_time: Duration, r: PollResult)`; `fn report_due(&self, now: Instant) -> Option<String>` once per second: `replies N | reply ms med X p95 Y max Z | changes C | skips S`.
  - `--no-gui`: run the poller on the main thread, print each value change (`HH:MM:SS.mmm data 0x08`) and, with `--stats`, the stats line; `Ctrl-C` exits.

- [ ] **Step 1: Write failing tests:**
  - `retry_delay_is_one_second_when_waiting`: `next_delay(Conn::Waiting, 50ms, 2ms) == 1s`.
  - `delay_fills_interval`: `next_delay(Conn::Live, 50ms, 12ms) == 38ms`.
  - `slow_reply_polls_immediately`: `next_delay(Conn::Live, 50ms, 800ms) == 0`.
  - `stats_report_format`: record replies of 2, 3, 400 ms and one skip; report string contains `replies 3`, `max 400`, `skips 1`.
- [ ] **Step 2: Run** `cargo test`. Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `cargo test`. Expected: all pass.
- [ ] **Step 5: Hardware check** (board running, PicoScope 7 open with MCP on): `cargo run --release -- --no-gui --stats` for 10 s. Expected: values walk `0x01 … 0x80` about every 100 ms; stats lines show skips at or below what the spec measured at 50 ms (4 per 28 changes). Then close PicoScope 7 for a few seconds: expect waiting output and recovery after it reopens (ask the user to do the close/reopen, or skip and note it).
- [ ] **Step 6: Commit.**

### Task 5: GUI (board look)

**Files:**
- Create: `visualizer/src/ui.rs`
- Modify: `visualizer/src/main.rs`

**Interfaces:**
- Consumes: `Snapshot::lit()`, `Snapshot::status_text()`, `poller::spawn`, `Args`.
- Produces: `pub struct VizApp { shared: Arc<Mutex<Snapshot>> }` implementing `eframe::App`.

- [ ] **Step 1: Write failing test** `board_layout_scales`: `pub fn led_centers(rect: egui::Rect) -> [egui::Pos2; 8]` returns 8 points left to right (x strictly increasing, P0 leftmost) inside `rect`, evenly spaced; for a rect twice as wide, spacing doubles.
- [ ] **Step 2: Run** `cargo test`. Expected: FAIL.
- [ ] **Step 3: Implement** per spec "Look": dark background; green PCB rounded rect; chip label `TCA9538 @0x23` with hover tooltip `I2C LED visualizer v{CARGO_PKG_VERSION}`; 8 round LEDs (lit: bright red + glow; unlit: dark red) with labels `P0`…`P7`; status line under the board. Everything scales with window size. F11 toggles `ViewportCommand::Fullscreen`. Window title `I2C LED visualizer`. `main.rs` (GUI mode): spawn the poller with `on_change` = `ctx.request_repaint()` (get the `egui::Context` from eframe's creation context). No repaint timer is needed beyond that.
- [ ] **Step 4: Run** `cargo test`. Expected: pass. `cargo clippy -- -D warnings`. Expected: clean.
- [ ] **Step 5: Visual check:** `cargo run --release` with the board running; take a screenshot (`grim` or `gnome-screenshot -f`, whichever exists) and inspect it: one LED lit, status line `last frame: addr 0x23 · reg 0x01 · data 0x..`. Ask the user to judge lag against PicoScope 7 side by side.
- [ ] **Step 6: Commit.**

### Task 6: Readme, links, report

**Files:**
- Create: `visualizer/readme.md`
- Modify: `../i2c_io/readme.md` (link in Introduction; mark the pause build as run on hardware in "Validated HW & SW"), `../i2c_io/docs/porting_report.md` (section 9: tick "run the 100 ms pause build on hardware"; section 11: Hardware row → done 2026-11-02, decode walks 01…80)

- [ ] **Step 1: Write `readme.md`:** what it is (one paragraph); requirements (PicoScope 7 7.2.30+, Rust via rustup); PicoScope 7 setup: set `PicoMCPEnabled` to `true` in `/opt/picoscope/lib/PicoScope.GTK.dll.config` with the `sudo sed -i '/PicoMCPEnabled/s/false/true/' …` command and backup note (a package update resets it), Preferences → MCP server (beta) → On for current session, port manual 18123; D0 = SCL, D1 = SDA; trigger PulseWidth, positive pulse > 1 µs, Repeat; run `cargo run --release` (options `--url`, `--interval-ms`, `--stats`, `--no-gui`); known limits (PicoScope stalls are mirrored, skipped steps happen when PicoScope misses a capture); link to `docs/design.md`.
- [ ] **Step 2: Update** the i2c_io readme and porting report as listed above.
- [ ] **Step 3: Verify** `cargo test` passes and `git status` shows only intended files (no `target/`).
- [ ] **Step 4: Commit.**
