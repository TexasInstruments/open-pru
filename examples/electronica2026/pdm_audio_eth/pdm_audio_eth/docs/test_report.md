# Test report: pdm_audio_eth (PDM audio over Gigabit Ethernet, AM243x LP)

Date: 2026-11-04, 22:10 CET (off-board results re-run on the pushed head);
section 6 board runs 2026-11-05
Branch: `dev_tley_electronica2026_pdm_audio_eth` (open-pru), commit `18284abc`,
stacked on `dev_tley_electronica2026_pdm_audio`
Design: [design.md](design.md), plan: [plan.md](plan.md), board project:
[../readme.md](../readme.md), PC application: [../../pc_app/README.md](../../pc_app/README.md)

## 1. Status

| Part | Status |
| ---- | ------ |
| Off-board: builds, PRU assembler and linter, PC application tests, PC application against its board emulator | **PASS**, section 3 |
| On the board: ICSSG1 Ethernet (B0, B3) | **PASS** 2026-11-05, section 6 |
| On the board: link console line, clip buffer, RX counters, audio both ways (B1, B2, B4 to B9) | **not run yet**, section 6 (FIXME) |

The board runs were deferred by the user during the implementation ("Need to
skip for now. Save it for later verification and continue with next block if
possible."). The Ethernet test (B0, B3) ran on the board on 2026-11-05, after
two firmware fixes found there (section 6.1); the audio checks are still to
run. The ICSSG0
PDM chain is the board-tested `pdm_audio` firmware (board run PASSED
2026-11-04, see the
[pdm_audio porting report](../../../pdm_audio/pdm_audio/docs/porting_report.md));
this project uses it byte for byte.

There was no pru-simulator work for this project (user decision: "board is the
test bench for Ethernet. No simulator work here"). The ICSSG1 firmware
(`eth_rx.asm`, `eth_tx.asm`) is therefore checked only by the assembler, the
linter and the structure tests of section 3.3 off the board, and by the
board runs of section 6.

## 2. Setup

| Item | Value |
| ---- | ----- |
| Host PC | Intel Core Ultra 7 265F, Ubuntu 26.04, kernel 7.0.0-38, Realtek RTL8125 `enp129s0` (cable not connected for section 3; direct cable to the LP J18 (RGMII2) for section 6) |
| PC tuning (section 6) | `isolcpus=domain,managed_irq,6-7 nohz_full=6-7 rcu_nocbs=6-7 irqaffinity=0-5,8-19`; `enp129s0` interrupt on CPU 6 (systemd `pdm-eth-irq.service`), `pdm-eth` network thread on CPU 7 (`link::pin_for_nic`) |
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

- ICSSG1 bring-up: MII_RT, RGMII2 pads, MDIO, the PHY at address 15, and the
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

Wiring and PC setup are in [../readme.md](../readme.md). Run order: one PONG,
then pings, throughput, audio. B0 and B3 ran on 2026-11-05 with the firmware
of `23428c46`; the other checks are still FIXME.

| # | Check | Command | Pass criterion | Result |
| - | ----- | ------- | -------------- | ------ |
| B0 | Single PONG | `pdm-eth test --iface enp129s0 --pings 1 --seconds 0.1` | ping line 1/1 | **PASS**: 1/1, RTT 141 us, board turnaround 0.39 us |
| B1 | ICSSG0 header | CCS memory browser 0x30010000 | as in pdm_audio, CLIP_FRAMES 0x4000, UNDERRUN_COUNT 0, SD_OVF_COUNT 0 | **PASS** 2026-11-05, section 6.5 |
| B2 | Link | console line, `ethtool enp129s0` | `link 1000 FD on ICSSG1 port 1`; 1000 Mb/s, full duplex | **PASS** 2026-11-05 (firmware of `cb26d504`): console `PDM audio eth: link 1000 FD on ICSSG1 port 1 after 3610 ms, board MAC 02:00:5d:4d:00:01`; `ethtool`: 1000Mb/s, Full, link detected |
| B3 | Ethernet test | `pdm-eth test --iface enp129s0` | every line PASS: 0 lost of 10 000 pings, at least 500 Mbit/s each way, less than 0.1 % loss | **PASS** 3 of 3 runs, section 6.2 |
| B4 | Clip buffer untouched | save 0x700C0000, 0x40000 bytes after B3; `python3 tools/compare_clip_dump.py DUMP` | identical | **PASS** 2026-11-05: `identical: 262144 bytes`, section 6.4 |
| B5 | RX counters | ICSSG1 shared RAM 0x30090010 | RX_BAD not rising with RX_FRAMES | **PASS** 2026-11-05, section 6.4 |
| B6 | Board to PC | `pdm-eth capture --iface enp129s0 --seconds 600 --out DIR`, then `python3 tools/analyze_capture.py DIR` | lost 0, resyncs 0, tx_late 0; channels 3 to 7 at 1000, 3000, 500, 2000, 5000 Hz at the pdm_audio levels; CCS half A matches the capture | **PASS** 2026-11-05, section 6.6 |
| B7 | GUI | `pdm-eth gui --iface enp129s0` | channel 3 a 1 kHz line, channel 1 the chirp, at least 30 fps while switching channels, lost 0 over 10 min | FIXME: fps |
| B8 | PC to board | 8 WAVs of at least 60 s via `gui` and `stream --capture DIR` | input underrun/blocked/lost 0, UNDERRUN_COUNT and SD_OVF_COUNT 0, input and output spectrograms alike, `in_ch*.wav` and `out_ch*.wav` match (back-to-back AUDIO_IN frames both land) | FIXME |
| B9 | Board audio latency | GUI side panel during B8 | stable within 1 ms | FIXME: ms |

### 6.1 Fixes found on the board

| Commit | Finding |
| ------ | ------- |
| `6f4d0431` | the ICSSG IEP count is at offset 0x10/0x14; 0x0C is `slow_compen`, so every board time stamp was wrong |
| `82eea4c5` | the TPUT frame sent right after the second AUDIO_OUT frame of a pair never reached the wire (the MAC counted it TX_GOOD); PRU0 now waits 0.3 us (`AUDIO_TX_GUARD`) after an audio pair |
| `80ef1862` | 64-byte throughput test added (frame rate, not only Mbit/s) |
| `23428c46` | `sendmmsg` batching, and the network thread is pinned away from the NIC interrupt CPU: with both on one CPU the NIC dropped frames (`rx_missed`) |
| `303a981d`, `0ec2906e` | the console said link DOWN: the HIL example had the PHY addresses swapped (J18 / RGMII2 is PHY 15, not 3; MDIO_LINK read 0x00008000). The R5F now reads the PHY's PHYSTS register and prints speed, duplex and time to link (3610 ms on the board, so the old 5 s wait was not the cause) |
| `7236a9a3` | PRU0 waited 100 cycles after CRC + EOF (copied from the HIL example, which only waited before its halt); the TRM lets the next frame start 5 core clocks after TX_L2 drains, so it now waits 5 (`TX_EOF_GAP`) |
| `cb26d504` | the inter-frame gap comes from MII_RT `TX_IPG` (R5F), section 6.3; the HIL value 0x18 counts 333 MHz core clocks (72 ns nominal) |
| `9d95d473` | `pdm-eth test` takes the board-to-PC rate from the board's header time stamps, not from PC receive times |

### 6.2 B3 Ethernet test

Three runs of `pdm-eth test --iface enp129s0` with the tuning of section 2
(interrupt on CPU 6, network thread on CPU 7), firmware of `cb26d504`
(5-cycle post-EOF gap, `TX_IPG` 0x28):

| Run | Ping RTT min/mean/p99/max | Board turnaround | To board 1514 B | From board 1514 B | To board 64 B | From board 64 B |
| --- | ------------------------- | ---------------- | --------------- | ----------------- | ------------- | --------------- |
| 1 | 18/32/172/4006 us | 0.30-31.33 us | 984.4 Mbit/s | 976.0 Mbit/s | 0.598 Mframes/s | 1.459 Mframes/s (98 %) |
| 2 | 17/33/167/4055 us | 0.30-31.30 us | 984.4 Mbit/s | 976.0 Mbit/s | 0.598 Mframes/s | 1.459 Mframes/s (98 %) |
| 3 | 18/31/168/4066 us | 0.30-31.13 us | 984.4 Mbit/s | 976.0 Mbit/s | 0.598 Mframes/s | 1.459 Mframes/s (98 %) |

The same test with the firmware of `23428c46` (100-cycle post-EOF wait,
`TX_IPG` 0x18) gave 956.5 Mbit/s and 1.051 Mframes/s from the board; the
other columns were the same.

Every run: 10 000 of 10 000 pings, 0 frames lost and 0 sequence gaps in all
four throughput directions, NIC `rx_missed` unchanged.

- Board turnaround (PONG IEP time minus PING receive IEP time) is 0.3 us when
  PRU0 is idle. The maximum of about 32 us is a PONG that waits behind an
  AUDIO_OUT pair already on the wire (2 x 1118 B).
- The single pings of about 4 ms are on the PC: the board
  turnaround of the same runs stays below 33 us. They do not matter for audio,
  where the PC runs 0.5 s ahead.
- 64 B to the board, 0.6 Mframes/s, is the limit of the PC's NIC (one
  doorbell per frame). 64 B from the board, 1.459 Mframes/s, is set by the
  inter-frame gap (section 6.3): one frame per 685 ns = 576 ns of frame and
  preamble plus a ~109 ns gap. 1514 B from the board, 976.0 Mbit/s, is
  ~90 ns per frame under that: PRU0 loads 24 chunks per frame.
- Before the NIC interrupt was moved to CPU 6 (it stayed on CPU 17, because
  `pdm-eth-irq.service` ran before NetworkManager opened `enp129s0`), one of
  three runs lost 2688 frames of the 64 B board-to-PC test in the NIC
  (`rx_missed`); the board lost none. The service now waits for the
  interrupt to appear.

### 6.3 Inter-frame gap (MII_RT `TX_IPG`)

With the 5-cycle post-EOF wait PRU0 is no longer the limit for 64-byte frames,
and the gap between frames is the one MII_RT inserts. It was measured
indirectly: the gap is the board-to-PC frame period of the 64 B test minus
576 ns (64 B frame + 8 B preamble). Every setting gave the same number in
every run.

| `TX_IPG` counter clock | `TX_IPG` | Frame period | Gap | Result |
| ---------------------- | -------- | ------------ | --- | ------ |
| core clock (3 ns) | 0x20 (96 ns nominal) | 661 ns | 85 ns | too short (96 ns minimum) |
| TX interface clock (TXCFG bit 12) | 11 (TRM minimum for RGMII 1G) | 847 ns | 271 ns | slow |
| TX interface clock | 4 (below the TRM minimum) | 911 ns | 335 ns | slower; TRM: below the minimum "will get a jitter" |
| **core clock** | **0x28 (120 ns nominal, reset value)** | **685 ns** | **109 ns** | **kept** |

- In core-clock mode the gap is ~11 ns under nominal; 0x28 is the first
  setting in steps of 8 (as the TRM advises) above 96 ns.
- The TX-interface-clock mode costs ~175 ns per frame even at its minimum
  setting; why is not known.
- The gap is an average. RGMII gaps are whole byte times (8 ns), so 109 ns
  means about 104 ns and 112 ns. The PC NIC lost no frame even at 85 ns. A
  direct measurement needs a scope on RGMII2 TX_CTL. FIXME if a strict
  minimum gap must be shown.

### 6.4 B5 RX counters

CCS memory view of ICSSG1 shared RAM after the three B3 runs of section 6.2
(13:19, board reloaded 12:58). Each `pdm-eth test` zeroes RX_FRAMES ..
RX_BAD and the TPUT_RX block with CONTROL RESET_COUNTERS before its 64 B
PC-to-board burst, so they hold that burst (1 793 096 frames in run 3) plus
the two CONTROL frames after it (TPUT_REPORT_REQ, TPUT_TX).

| Offset | Word | Value | Expected |
| ------ | ---- | ----- | -------- |
| 0x10 | RX_FRAMES | 0x001B5C4A = 1 793 098 | 1 793 096 + 2 |
| 0x14 | RX_AUDIO | 0 | 0 (no AUDIO_IN in `test`) |
| 0x18 | RX_FOREIGN | 0 | 0 |
| 0x1C | RX_BAD | 0 | 0 |
| 0x20 | LAST_IN_END | 0 | 0 |
| 0x38, 0x3C | TPUT_TX_COUNT, TPUT_TX_LEN | 0, 60 | burst done, 64 B mode |
| 0x40, 0x44 | TPUT_RX_FRAMES, TPUT_RX_GAPS | 0x001B5C48 = 1 793 096, 0 | run 3: 1 793 096 |
| 0x48 | TPUT_RX_BYTES | 53 792 880 | 1 793 096 x 30 (header length of a 60 B frame) |
| 0x50, 0x58 | TPUT_RX_FIRST_IEP, _LAST_IEP | 0xA00FD416A, 0xAB3D31BDF | 3.000 s apart: 0.5976 Mframes/s |
| 0x64 | TX_AUDIO | 612 850, rising | 500/s x 20.4 min since the reload |
| 0x68, 0x6C | TX_PONGS, TX_LATE | 30 000, 0 | 3 runs x 10 000 pings, no late half |
| 0x70, 0x74 | CLIP_ADDR, CLIP_FRAMES | 0x700C0000, 0x4000 | as written by the R5F |

B4 (same session, 13:26): CCS Memory view, save 0x700C0000, 0x40000 bytes
(CCS Theia writes TI Data format 9: a `1651 9 700c0000 0 40000 10` header,
then one byte per line as 8 binary digits). `tools/compare_clip_dump.py`
reads that, raw binary and hex TI Data, and printed `identical: 262144
bytes (16384 frames x 8 channels)`: some 2 million PC-to-board frames per B3
run (TPUT_DATA, PING, CONTROL) left the clip buffer that RTU1 plays
unchanged. Only AUDIO_IN (stage 3, B8) may write it.

### 6.5 B1 ICSSG0 header

CCS memory view of 0x30010000 at 13:29, 31 min after the 12:58 reload, with
the B3 runs in between:

| Offset | Field | Value | Expected |
| ------ | ----- | ----- | -------- |
| 0x00 | MAGIC | 0x50444D38 | 'PDM8' |
| 0x04, 0x08, 0x0C | N_CH, FRAMES_PER_HALF, PCM_RATE | 8, 64, 16 000 | as in pdm_audio |
| 0x10, 0x14 | CLIP_ADDR, CLIP_FRAMES | 0x700C0000, 0x4000 | as written by the R5F |
| 0x18 | HALF_SEQ | 463 654 | 1854.6 s at 250/s |
| 0x1C | UNDERRUN_COUNT | 0 | 0 |
| 0x20 | BATCH_COUNT | 203 477 094 | 109 700/s (pdm_audio: ~110 000/s) |
| 0x24, 0x28 | RTU1_FRAME_POS, RTU1_LOOPS | 2468, 1811 | 1811 x 16 384 + 2468 = 29 673 892 frames; 1811 loops = 1854.5 s |
| 0x2C, 0x30 | VERSION, SD_OVF_COUNT | 1, 0 | 1, 0 |
| 0x40..0x5C | PCM_COUNT[0..7] | 29 673 891 .. 29 673 893 | RTU1 total +-1 (read while PRU0 updates) |

The PDM decoder output (PCM_COUNT) and the clip player (RTU1 frames) agree
within one sample per channel; HALF_SEQ x 64 = 29 673 856 is 36 frames
behind, the half being filled. No underrun and no SD overflow while the
ICSSG1 Ethernet tests ran at full rate.

### 6.6 B6 board to PC

`pdm-eth capture --iface enp129s0 --seconds 600`, 13:30:44 to 13:40:44,
firmware of `cb26d504`:

```
9599904 frames per channel written to DIR
lost 0 duplicates 0 resyncs 0; board: underrun 0 sd_ovf 0 tx_late 0 rx_foreign 0 rx_bad 0
board clock -25.8 ppm against the PC
```

NIC `rx_missed` unchanged. `python3 tools/analyze_capture.py DIR`:

| Ch | Clip | Frequency (fit) | Amplitude / 2^22 | pdm_audio board run | Min..max over 60 x 10 s | Fit residual |
| -- | ---- | --------------- | ---------------- | ------------------- | ----------------------- | ------------ |
| 3 | tone_1000 | 1000.0000 Hz | 1.000 | 1.000 | 1.000..1.000 | -89.7 dB |
| 4 | tone_3000 | 3000.0000 Hz | 0.942 | 0.942 | 0.942..0.942 | -87.1 dB |
| 5 | tone_500 | 500.0000 Hz | 0.999 | 0.999 | 0.999..0.999 | -87.9 dB |
| 6 | tone_2000 | 2000.0000 Hz | 0.981 | 0.981 | 0.981..0.981 | -84.6 dB |
| 7 | tone_5000 | 5000.0000 Hz | 0.850 | 0.850 | 0.850..0.850 | -86.3 dB |

- Loop check: RTU1 plays the 16 384-frame clip in a loop. For channels 3 to
  7 every loop of the capture (585) is bit for bit equal to the one before.
  The tones repeat within 32 frames, so this alone cannot see a shift by a
  whole 1 KB part (32 frames); a lost part is zero-filled by `capture` and
  would break it, and a shift would show in channels 0 to 2 below. With the
  capture's own counters (lost, duplicates, resyncs 0): no frame lost,
  repeated or out of place in 600 s.
- Channels 0 to 2 (speech, linear and log chirp) cannot repeat bit for bit
  (the modulator noise differs per loop): loop to loop they differ by
  -94 dB RMS (worst sample -76 dB of 2^22); a slipped frame would give about
  0 dB. Each correlates best with its own clip of `pdm_clips.h` (1.00, 0.91,
  0.98; the others 0.13 or less), so the channel order is right. Peaks 0.99,
  1.12, 1.07 x 2^22 (the pdm_audio CCS frames showed 0.33, 0.75, 0.84 in a
  few frames).
- Amplitudes equal the pdm_audio board run to three digits: the level drop
  of the higher tones is RTU1's zero-order hold, as predicted there.
- CCS spot check (13:44, 4 min after the capture): dump of 0x30010100,
  0x1000 bytes (half A and half B), `python3 tools/match_half_dump.py DUMP
  DIR`: both halves match the capture bit for bit in all five tone channels
  (512 common clip positions each, all at multiples of 32 frames, as the
  tones repeat within 32 frames): the PCM in ICSSG0 shared RAM is what
  reached the PC.

When the table is filled, close the matching FIXMEs in
[../readme.md](../readme.md) ("Validated HW & SW", "Hardware results") and
open the PR.
