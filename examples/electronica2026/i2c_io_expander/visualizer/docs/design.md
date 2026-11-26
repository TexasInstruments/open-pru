# I2C LED visualizer: design

Date: 2026-11-02. Status: reviewed, waiting for approval.

## Intent

At the electronica 2026 booth, an LP-AM243 runs the `i2c_io` example
(`../i2c_io`): ICSSG0 PRU0 bit-bangs a 1 Mbit/s I2C master and writes a
running-LED pattern to a TCA9538 IO expander. A PicoScope 3416E MSO captures
SCL/SDA, and PicoScope 7 shows the waveform and its I2C decode. There are no
physical LEDs on the booth setup.

The visualizer is a Rust desktop app that runs next to PicoScope 7, on the
same screen. It shows the 8 LEDs of the TCA9538 lighting up exactly as the
decoded bus traffic says, so a visitor sees the link: "these bits on the scope
are this LED".

## Goals

- Draw the TCA9538 board with 8 LEDs (P0-P7) and light the LED(s) given by
  the last decoded data byte written to the Output Port register (0x01).
- Below the board, print the last frame, for example
  `last frame: addr 0x23 · reg 0x01 · data 0x08`.
- Stay in step with PicoScope 7's screen. The LEDs must not lag behind the
  scope (the user's main concern, see Anti-goals).
- Work with PicoScope 7 owning the scope: get data only through PicoScope 7's
  MCP server, never through the Pico SDK or USB.
- Recover by itself when PicoScope 7 is closed, restarted, or stops sending
  frames.

## Non-goals

- No waveform drawing. PicoScope 7 does that.
- No generic I2C or multi-device support. One TCA9538 at 0x23, Output Port
  register only.
- No Windows or macOS support. Target is the booth's Linux desktop (the same
  PC that runs PicoScope 7 today).
- Not part of the open-pru make build.

## Anti-goals

- **LEDs lagging behind the scope.** This is the user's stated failure, even
  if everything else works. No smoothing, animation easing, fade-out timers,
  queues or replay of missed steps that would delay the newest value.
- Showing an LED state the bus did not carry. No prediction of the next step
  from the known pattern, and no filling in of steps PicoScope missed.
- Slowing PicoScope 7 down. Polling too fast was measured to make PicoScope
  skip more captures (see Measurements).

## Context and constraints

### Firmware (already done)

`../i2c_io/firmware/main.asm`, commit `e98966a5` on branch
`dev_tley_electronica2026_i2c_io`:

- After setup (Configuration register 0x03 = 0x00), each frame is
  START, 0x46 (addr 0x23, write), 0x01, data, STOP. Data walks
  0x01, 0x02, ..., 0x80, 0x01.
- Each frame is about 29 us long, followed by a 100 ms pause with the bus
  idle. One LED step is 100.03 ms.

### PicoScope 7 and its MCP server

- PicoScope 7 version 7.2.30 ("T&M Early Access") on Linux. The MCP server is
  a beta, hidden behind `PicoMCPEnabled` in the root-owned
  `/opt/picoscope/lib/PicoScope.GTK.dll.config`. It was set to `true` on
  2026-11-02 (backup `.orig` next to it). A PicoScope package update resets
  it. Then Preferences → MCP server (beta) → On for current session.
- Transport: MCP streamable HTTP, JSON-RPC 2.0 over `POST
  http://127.0.0.1:<port>/mcp`. Send `Accept: application/json,
  text/event-stream`; the response can be SSE (`data:` lines). Keep the
  `Mcp-Session-Id` response header and send it on later requests. Sequence:
  `initialize`, then the `notifications/initialized` notification, then
  `tools/call`.
- The port was auto-assigned (18123 on 2026-11-02). PicoScope 7 also offers a
  manual port in its MCP preferences.
- Scope wiring: digital D0 = SCL, D1 = SDA. The user's trigger: PulseWidth on
  a digital line, positive pulse > 1 us, Repeat, 20 % pre-trigger. It fires on
  the idle-high stretch before each START, so each capture holds one frame.

Tools used (names exactly as the server lists them):

| Tool | Use |
| ---- | --- |
| `get_serial_decoder_list` | Find an I2C decoder with Data = D1, Clock = D0. Returns `id`, `protocol`, `channels`, `settings`. |
| `configure_serial_decoder` | If none exists: `{"action":"Add","protocol":"I2C","name":"TCA9538","channels":{"Data":"D1","Clock":"D0"},"settings":{"Bus Speed":"Fast-mode Plus (1 Mbit/s)"},"enabled":true}` |
| `get_serial_decoder_summary` | `{"id":<id>,"groupBy":"Data","topK":5}`. Never returns raw rows, but the group keys are the data bytes of the frames in the current capture. |

Tool results arrive as `result.content[0].text`, a JSON string, which holds
`{"result":{...},"status":"Success",...}`.

Example of the summary result (trimmed), one frame in the capture:

```json
{"result":{"id":"103","totalFrames":1,"invalidFrames":0,
  "captureStart":-5.0e-07,"captureEnd":2.82e-05,
  "groupBy":"Data","groups":[{"key":"01 08 ","count":1,"errorCount":0}]},
 "status":"Success"}
```

The key is the bytes after the address, as space-separated hex with a
trailing space: register 0x01, then data 0x08. A frame cut off at the edge of
the capture shows as `"01 "` and must be ignored. Other valid group-by
columns: Buffer Number, Packet, Start Time, End Time, Address Bits, Address,
Address + R/W, R/W, Address ACK, Data, Data ACK, Max Voltage, Min Voltage,
Voltage Delta, Packet Time.

### Measurements (2026-11-02, on the booth hardware)

- 50 ms polling, quiet PicoScope: 24 changes in 2.3 s, every step in order
  01 ... 80, about 100 ms apart.
- Later runs of 6 s each, back to back: median call time 1.4-2.9 ms, but
  p95 70-510 ms and max up to about 1 s.

  | Poll interval | Changes seen | Skipped steps |
  | ------------- | ------------ | ------------- |
  | 10 ms         | 36           | 11            |
  | 20 ms         | 16           | 7             |
  | 33 ms         | 26           | 11            |
  | 50 ms         | 28           | 4             |

  The runs are noisy (the earlier quiet 50 ms run had 0 skips), but nothing
  below 50 ms was better than 50 ms.
- User's observation of PicoScope 7's own screen: "it shows all steps most of
  the time. sometimes it stop shortly and jumps over the next steps faster."
  So PicoScope itself stalls and catches up. The visualizer should mirror
  that, not hide it.

## Behaviour

1. **Connect.** Use the MCP URL from a command-line option (default
   `http://127.0.0.1:18123/mcp`; the readme tells the user to set PicoScope
   7's MCP port to manual 18123). *(my call)*
2. **Find or add the decoder** (table above). Never change the trigger, the
   timebase or the channels.
3. **Poll** `get_serial_decoder_summary` every 50 ms (start of one request to
   start of the next), one request at a time, never overlapping. If a reply
   takes longer than the interval, send the next request as soon as it
   returns. The interval is a command-line option. *(my call: the
   measurements above show no gain below 50 ms, only more skips)*
4. **Pick the value.** Accept only keys with exactly two bytes and register
   0x01. If the capture holds no valid frame, keep showing the last value.
   If it holds more than one valid frame (not expected with the current
   trigger), keep the last shown value unless one of them is its successor in
   the pattern; this is the builder's call as long as it never invents a
   value.
5. **Draw at once.** When the value changes, repaint immediately (request a
   repaint from the polling thread). No easing, no fade.
6. **Address.** The `Data` grouping does not include the address. Query
   `groupBy:"Address"` at a low rate (about once per second) for the status
   line. *(my call)* If that is not reliable, show the address as unknown,
   never as a hard-coded 0x23.
7. **Slow replies are not lost connections.** PicoScope stalls of up to
   about 1 s are normal and are mirrored: the LEDs simply hold their state
   until the reply arrives. HTTP timeout 3 s. *(my call)*
8. **Lost connection.** If the connection is refused, a request times out
   (3 s), PicoScope returns an error, or the session expires: dark all LEDs,
   show `waiting for PicoScope 7 …` in the status line, and retry
   (re-initialize the session) about once per second. *(my call)*
9. **No frames.** If PicoScope answers but the replies contain no valid frame
   (no two-byte key with register 0x01) for 2 s, keep the last LED and show
   `no I2C frames`. If PicoScope keeps returning the same frame (acquisition
   stopped), that is not "no frames": the LED and status stay frozen, like
   the scope screen. *(my call)*
10. **Decoder handling.** Use the first enabled I2C decoder with Data = D1
   and Clock = D0. If the only match is disabled, enable it. If none matches,
   add one (table above). Do not change a matching decoder's other settings,
   and do not touch other decoders. Startup and reconnect run the same
   check; PicoScope 7 restores its last session, so after a restart the
   decoder is normally already there. *(my call)*

## Lag requirement and how it is checked

- After a reply carrying a new value arrives, the LEDs show it in the next
  drawn frame (under 20 ms). On top of that, the polling adds at most one
  interval (50 ms) behind PicoScope's own decoder. PicoScope's own stalls are
  not counted; they are mirrored. *(my call)*
- `--stats` prints, once per second: replies, median/p95/max reply time,
  value changes, and skipped steps (a change that is not the successor in
  the pattern). This is how the poll interval is checked on the booth
  hardware, which is connected to this PC. *(my call)*
- Final check by the user at the booth setup: watch PicoScope 7's decode and
  the LED window side by side. The LED must not visibly trail the scope.

## Look

Chosen by the user from three mockups: **A, board look**.

- A dark window. A green PCB with a chip label `TCA9538 @0x23`, a row of 8
  round LEDs, labelled P0 ... P7 left to right, with P0 = data bit 0. A lit
  LED is bright red with a glow; an unlit one is dark red.
- Under the board, one status line: `last frame: addr 0x23 · reg 0x01 · data
  0x08`, or the waiting/no-frames text from Behaviour.
- A normal resizable window that the user places on the right of the screen
  next to PicoScope 7. The board scales with the window. F11 toggles
  fullscreen. *(my call)*
- Hovering the chip shows the tooltip `I2C LED visualizer vX.Y.Z` (version
  from `Cargo.toml`), as in the pru_simulator UI. The desktop runs Wayland,
  where the OS title bar is not drawn by egui, so the tooltip lives on the
  chip, not the title bar. *(my call)*

## How it is built

- Rust, stable toolchain installed with rustup. *(user asked for Rust; rustup
  is my call)*
- GUI: `eframe`/`egui`. *(my call)* HTTP: a small blocking client
  (for example `ureq`) on a polling thread, sharing the newest state with the
  GUI. *(my call)*
- Location: `open-pru/examples/electronica2026/i2c_io_expander/visualizer/`,
  a Cargo project on branch `dev_tley_electronica2026_i2c_io`. *(user)*
  Add a `.gitignore` for `target/`. Not added to `examples/makefile`.
- `visualizer/readme.md`: what it is, the PicoScope 7 setup (MCP flag,
  preferences, manual port 18123, D0/D1, trigger), `cargo run --release`, and
  the known limits (PicoScope stalls are mirrored). Link it from
  `../i2c_io/readme.md`.
- open-pru is mostly C and assembly. Keep the crate small and self-contained.

## Left to the builder

- Module layout, threading details, exact poll interval tuning (within the
  rules above), drawing code, and test approach. A fake MCP server for tests
  is suggested, since the real one needs the board and scope.
- Handling of SSE versus plain JSON responses, retry back-off details.
