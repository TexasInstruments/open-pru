# Test report: pdm_audio_eth (PDM audio over Gigabit Ethernet, AM243x LP)

Date: 2026-11-04, 22:10 CET (off-board results re-run on the pushed head)
Branch: `dev_tley_electronica2026_pdm_audio_eth` (open-pru), commit `18284abc`,
stacked on `dev_tley_electronica2026_pdm_audio`
Design: [design.md](design.md), plan: [plan.md](plan.md), board project:
[../readme.md](../readme.md), PC application: [../../pc_app/README.md](../../pc_app/README.md)

## 1. Status

| Part | Status |
| ---- | ------ |
| Off-board: builds, PRU assembler and linter, PC application tests, PC application against its board emulator | **PASS**, section 3 |
| On the board: ICSSG1 Ethernet, PC to board and board to PC audio | **not run yet**, section 6 (FIXME) |

Nothing of the Ethernet part has run on an LP-AM243 yet. The board runs were
deferred by the user during the implementation ("Need to skip for now. Save it
for later verification and continue with next block if possible."). The ICSSG0
PDM chain is the board-tested `pdm_audio` firmware (board run PASSED
2026-11-04, see the
[pdm_audio porting report](../../../pdm_audio/pdm_audio/docs/porting_report.md));
this project uses it byte for byte.

There was no pru-simulator work for this project (user decision: "board is the
test bench for Ethernet. No simulator work here"). The ICSSG1 firmware
(`eth_rx.asm`, `eth_tx.asm`) is therefore checked only by the assembler, the
linter and the structure tests of section 3.3. Its timing and its behaviour
on the real MII_RT, TX_L2 and RX_L2 hardware are not checked yet.

## 2. Setup

| Item | Value |
| ---- | ----- |
| Host PC | Intel Core Ultra 7 265F, Ubuntu 26.04, kernel 7.0.0-38, Realtek RTL8125 `enp129s0` (cable not connected for these runs) |
| PC toolchain | Rust 1.99.1, cargo 1.99.1 |
| Board toolchain | MCU+ SDK 12.01.00.33, CCS 21.0.1, ti-cgt-pru 2.3.3, ti-cgt-armllvm 5.1.1.LTS, SysConfig 1.28.1 |
| Board | LP-AM243 Rev A (for section 6) |

## 3. Off-board results

### 3.1 Summary

Run from `pdm_audio_eth/` (board project) and `pc_app/` at `18284abc`.

| # | Check | Command | Result |
| - | ----- | ------- | ------ |
| 1 | ICSSG0 firmware identical to pdm_audio | `tools/check_icssg0_copy.sh` | **PASS**: 7 of 7 files `same` |
| 2 | Clip buffer header up to date | `python3 tools/make_clip_buffer.py --check` | **PASS**: `mcuplus/pdm_clips.h: up to date` |
| 3 | ICSSG1 PRU1 assembles, lints | `clpru -v4 ... firmware/eth_rx.asm` and `lint_pru.py --openpru firmware/eth_rx.asm` | **PASS**: 0 errors, 0 warnings |
| 4 | ICSSG1 PRU0 assembles, lints | the same for `firmware/eth_tx.asm` | **PASS**: 0 errors, 0 warnings |
| 5 | make build, 5 PRU firmwares + R5F | `make -s DEVICE=am243x MCU_PLUS_SDK_PATH=... SYSCFG_NODE=...` | **PASS**: exit 0, `pdm_audio_eth.release.out` 1 324 116 bytes; one SysConfig notice (AM2434 part number selected) |
| 6 | CCS headless build, 6 project specs | `ccs-server-cli.sh ... projectCreate` / `projectBuild -ccs.configuration Release` | **PASS**: 6 of 6 projects 0 errors; PRU projects 0 warnings, R5F 1 warning (the same SysConfig part-number notice) |
| 7 | PC application unit tests | `cargo test` | **PASS**: 63 passed, 0 failed |
| 8 | Protocol mirror test | `cargo test` (tests/proto_mirror.rs) | **PASS**: 1 passed |
| 9 | PRU structure tests | `cargo test` (tests/asm_structure.rs) | **PASS**: 2 passed |
| 10 | Lints | `cargo clippy --all-targets -- -D warnings` | **PASS**: clean |
| 11 | Spectrogram time budget | `cargo test --release --lib spectro_8ch_realtime_budget` | **PASS**: 8 channels x 1 s of audio in less than 50 ms |
| 12 | Ethernet test against the emulator | `pdm-eth test --emulator` | **PASS**: 4 of 4 lines (section 3.4) |
| 13 | Capture against the emulator | `pdm-eth capture --emulator --seconds 5 --out DIR` | **PASS**: 8 WAVs, 79 968 frames each, lost 0, duplicates 0, resyncs 0 |
| 14 | GUI against the emulator | `env -u WAYLAND_DISPLAY pdm-eth gui --emulator --no-vsync --screenshot PNG` | **PASS**: 134 fps (requirement: at least 30), 499 AUDIO_OUT frames/s, lost/dup/resync 0/0/0 |

### 3.2 PC application tests by module

| Module | Tests | What they check |
| ------ | ----: | --------------- |
| proto | 8 | frame sizes (AUDIO_IN 1062 B, AUDIO_OUT 1118 B), every frame type round-trips, foreign EtherType, short or bad length rejected, byte order, padding, RTU1 sample count |
| emulator | 8 | ping answer, AUDIO_IN lands in the ring, 500 AUDIO_OUT/s, throughput counters and report, foreign EtherType ignored, silent until the PC says hello, board reset restarts the indices, TPUT_TX burst count |
| ethtest | 6 | lost pings counted, RTT statistics, the whole test passes on the emulator, PC and board loss split apart, kernel and NIC drops kept apart from board loss, PASS/FAIL thresholds |
| link | 4 | link speed and duplex from sysfs, a cable-less interface reads as no carrier, NIC drop counters summed, missing `setcap` gives a hint |
| rx | 7 | 8 x 24-bit WAV capture, board clock in ppm, gaps counted and zero-filled, duplicates ignored, frames placed by sample index, resync on an index jump back, resync when the board time goes back (reset) |
| pacing | 6 | holds the 0.5 s lead at 16 kHz, never overwrites unread ring audio, counts an underrun and recovers, end of stream is not an underrun, restarts after a board reset, starts one lead ahead |
| latency | 3 | known emulator delay found (2.5 ms within 0.2 ms), none without input, none for uncorrelated input |
| spectro | 6 | tone peak in the right bin, dB clamped, 124 columns per second, 10 s kept, difference column 0 for equal input, the 8-channel time budget (release only) |
| ui | 5 | newest column on the right and low frequencies at the bottom, colour map ends, network thread feeds output and latency from the emulator, input pane resumes after a board reset |
| session | 3 | capture survives a board reset, stream survives a board reset, stream times out after 3 s without a board |
| wavin | 5 | 48 kHz stereo float to 16 kHz mono, 8- and 24-bit files, peak 16384, short files padded with silence, bad files name the file |
| seam | 2 | aligned blocks reach a stage, shared-memory ring wraps and counts |
| **sum** | **63** | |

Integration tests: `inc_h_rs_agree` checks that the 98 protocol constants in
`firmware/eth_proto.inc`, `mcuplus/eth_proto.h` and `pc_app/src/proto.rs`
agree. `rx_handlers_do_not_fall_through` and `tx_routines_do_not_fall_through`
check that every frame handler in `eth_rx.asm` and every routine in
`eth_tx.asm` ends in a branch or return, so none can run into the code after it.

### 3.3 Final review fixes

A fresh reviewer read the whole branch after the 16 plan tasks. Each finding
below was fixed with a test that failed first and then passed:

| Severity | Finding | Test |
| -------- | ------- | ---- |
| Critical | the PING handler in `eth_rx.asm` ran into the AUDIO_IN store: every ping wrote 26 bytes into the clip buffer | `rx_handlers_do_not_fall_through` |
| Important | kernel and NIC drops on the PC counted as board loss in the throughput test | `nic_drops_sum_the_nic_counters`, `tput_verdict_splits_kernel_nic_and_unexplained_loss` |
| Important | `capture` stalled after a board reset | `capture_survives_board_reset` |
| Important | `stream` ran forever without a board | `stream_times_out_without_board`, `stream_survives_board_reset` |
| Important | a board reset was counted as lost audio | `rx_resync_when_board_time_goes_back` |
| Important | the GUI input pane froze and grew after a board reset | `input_pane_resumes_after_board_reset` |

### 3.4 Emulator output

```
$ pdm-eth test --emulator
PASS link: 1000/full
PASS ping: 10000/10000, rtt min/mean/p99/max 0/0/0/3 us, board turnaround 1.50-1.50 us
PASS to board: 91776196/91776196 frames, 370531.1 Mbit/s, 0 sequence gaps
PASS from board: 243822/243822 frames, 984.4 Mbit/s, 0 sequence gaps

$ pdm-eth capture --emulator --seconds 5 --out DIR
79968 frames per channel written to DIR
lost 0 duplicates 0 resyncs 0; board: underrun 0 sd_ovf 0 tx_late 0 rx_foreign 0 rx_bad 0
board clock +4.0 ppm against the PC
```

The emulator runs in the PC process, with no NIC and no cable. Its RTT,
turnaround and throughput numbers only show that the PC side counts and
reports correctly; they are not hardware numbers. A 5 s capture writes
79 968 frames, not 80 000, because the PCM half in flight at the start is
skipped.

## 4. What the off-board tests do not show

- ICSSG1 bring-up: MII_RT, RGMII2 pads, MDIO, the PHY at address 3, and the
  1000 Mbit/s full-duplex link.
- The TX paths of `eth_tx.asm` that the HIL example never used: back-to-back
  frames after CRC/EOF, multi-chunk frames with the 42-byte tail (TPUT_DATA),
  and the 30-byte tail (AUDIO_OUT). The HIL example sent one 64-byte frame and
  halted.
- The RX_L2 behaviour of `eth_rx.asm`: the double-buffer position in R18.b0,
  EOF on R31 bit 20, and the reset between frames. RX_BAD rising with every
  frame would mean the EOF flag is still set when the next frame starts.
- PRU timing: the throughput burst giving way to audio, the AUDIO_IN store
  keeping up with 250 frames/s, and TX_LATE staying 0.
- The real RTT, throughput and board audio latency, and whether the RTL8125
  keeps up at about 81 000 frames/s.
- The GUI at the display rate. The 134 fps above is drawn without vsync on
  the emulator; a visible window runs at the display rate.

## 5. Reproduce

```
cd examples/electronica2026/pdm_audio_eth/pdm_audio_eth
tools/check_icssg0_copy.sh
python3 tools/make_clip_buffer.py --check
make DEVICE=am243x MCU_PLUS_SDK_PATH=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33 \
     SYSCFG_NODE=$HOME/ti/ccs2101/ccs/tools/node/node
cd ../pc_app
cargo test && cargo clippy --all-targets -- -D warnings
cargo test --release --lib spectro_8ch_realtime_budget
cargo build --release
./target/release/pdm-eth test --emulator
./target/release/pdm-eth capture --emulator --seconds 5 --out /tmp/cap
env -u WAYLAND_DISPLAY ./target/release/pdm-eth gui --emulator --no-vsync --screenshot /tmp/gui.png
```

CCS headless: `~/ti/ccs2101/ccs/eclipse/ccs-server-cli.sh -workspace <new dir>
-application projectCreate -ccs.projectSpec <each of the 6 example.projectspec>
-ccs.overwrite full`. Then run `-application projectBuild -ccs.projects <name>
-ccs.configuration Release`, the five PRU projects first and the R5F last.

## 6. Board results

FIXME: not run yet. Wiring and PC setup are in [../readme.md](../readme.md).
Run the first session in this order (one PONG, then pings, throughput, audio):

| # | Check | Command | Pass criterion | Result |
| - | ----- | ------- | -------------- | ------ |
| B0 | Single PONG | `pdm-eth test --iface enp129s0 --pings 1 --seconds 0.1` | ping line 1/1 | FIXME |
| B1 | ICSSG0 header | CCS memory browser 0x30010000 | as in pdm_audio, CLIP_FRAMES 0x4000, UNDERRUN_COUNT 0, SD_OVF_COUNT 0 | FIXME |
| B2 | Link | console line, `ethtool enp129s0` | `link 1000 FD on ICSSG1 port 1`; 1000 Mb/s, full duplex | FIXME |
| B3 | Ethernet test | `pdm-eth test --iface enp129s0` | every line PASS: 0 lost of 10 000 pings, at least 500 Mbit/s each way, less than 0.1 % loss | FIXME: RTT min/mean/p99/max, turnaround, Mbit/s both ways |
| B4 | Clip buffer untouched | save 0x700C0000, 0x40000 bytes after B3; compare with `mcuplus/pdm_clips.h` | identical | FIXME |
| B5 | RX counters | ICSSG1 shared RAM 0x30090010 | RX_BAD not rising with RX_FRAMES | FIXME |
| B6 | Board to PC | `pdm-eth capture --iface enp129s0 --seconds 600 --out DIR` | lost 0, resyncs 0, tx_late 0; channels 3 to 7 at 1000, 3000, 500, 2000, 5000 Hz at the pdm_audio levels; CCS half A matches the capture | FIXME |
| B7 | GUI | `pdm-eth gui --iface enp129s0` | channel 3 a 1 kHz line, channel 1 the chirp, at least 30 fps while switching channels, lost 0 over 10 min | FIXME: fps |
| B8 | PC to board | 8 WAVs of at least 60 s via `gui` and `stream --capture DIR` | input underrun/blocked/lost 0, UNDERRUN_COUNT and SD_OVF_COUNT 0, input and output spectrograms alike, `in_ch*.wav` and `out_ch*.wav` match (back-to-back AUDIO_IN frames both land) | FIXME |
| B9 | Board audio latency | GUI side panel during B8 | stable within 1 ms | FIXME: ms |

When the table is filled, close the matching FIXMEs in
[../readme.md](../readme.md) ("Validated HW & SW", "Hardware results") and
open the PR.
