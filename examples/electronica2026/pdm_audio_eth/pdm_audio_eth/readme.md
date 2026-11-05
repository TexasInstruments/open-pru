# PDM Audio over Gigabit Ethernet, 8 channels (electronica 2026)

## Introduction

The [pdm_audio](../../pdm_audio/pdm_audio/readme.md) chain, connected to a PC
over Gigabit Ethernet, in both directions, with PRU firmware only:

- the PC streams WAV files (one per channel) into the board;
- ICSSG1 PRU1 receives them and writes them into the MSRAM clip buffer that
  ICSSG0 RTU1 modulates (8 sigma-delta modulators), ICSSG0 PRU1 drives the PDM
  pins, the jumpers bring them back to ICSSG0 PRU0, which decodes 16 kHz PCM;
- ICSSG1 PRU0 sends every finished PCM half to the PC;
- the PC application `pdm-eth` (in [../pc_app](../pc_app/README.md)) shows the
  input and the output spectrograms side by side, in real time, and captures
  both to WAV files.

The same chain in the pru-simulator takes about 11 minutes per second of
audio. The R5F (r5fss0-0, FreeRTOS) only configures ICSSG0, ICSSG1, the pins,
MDIO and the PHY, loads and starts the PRU cores, and idles.

The ICSSG0 firmware is the board-tested `pdm_audio` firmware, byte-identical
(`tools/check_icssg0_copy.sh` checks it). The design is in
[docs/design.md](docs/design.md), the implementation plan in
[docs/plan.md](docs/plan.md).

**Status (2026-11-05): board run PASSED** on an LP-AM243 Rev A, all ten
board checks B0 to B9 (Ethernet test, clip buffer, counters, 600 s capture,
GUI, PC-to-board stream, latency); results under "Hardware results" and in
[docs/test_report.md](docs/test_report.md).

## Supported Combinations

| Processor | Board            | PRU cores                                       | Host              |
| --------- | ---------------- | ----------------------------------------------- | ----------------- |
| AM243x    | AM243x LaunchPad | ICSSG0 RTU1, PRU1, PRU0; ICSSG1 PRU1, PRU0      | r5fss0-0 FreeRTOS |

Build files exist for AM243x only. The other processors are `Yport`, as for
`pdm_audio`.

## Validated HW & SW

| Processor | Hardware        | Software |
| --------- | --------------- | -------- |
| AM243x    | LP-AM243 Rev A, board run PASSED 2026-11-05 | Builds with MCU+ SDK 12.01.00.33, CCS 21.0.1, ti-cgt-pru 2.3.3, ti-cgt-armllvm 5.1.1.LTS, SysConfig 1.28.1; PC: Ubuntu 26.04, Rust 1.99, Realtek RTL8125 (`enp129s0`) |

## How it works

| Core | Firmware | Job |
| ---- | -------- | --- |
| ICSSG0 RTU1 | `firmware/rtu1_mod.asm` (pdm_audio) | 8 modulators, reads the clip buffer at `CLIP_ADDR`, `CLIP_FRAMES` = 16384 frames (1.024 s, 256 KB at MSRAM 0x700C0000) |
| ICSSG0 PRU1 | `firmware/pru1_pdm.asm` (pdm_audio) | 3.072 MHz PDM clock and 8 data pins |
| ICSSG0 PRU0 | `firmware/pru0_rx.asm` (pdm_audio) | 8 SD channels to 16 kHz PCM, ping-pong halves in ICSSG0 shared RAM |
| ICSSG1 PRU1 | `firmware/eth_rx.asm` | Ethernet receive on port 1: frame filter, pings, throughput counters, PC audio into the clip buffer |
| ICSSG1 PRU0 | `firmware/eth_tx.asm` | Ethernet transmit on port 1: two 1118-byte frames per PCM half, pongs, throughput frames |

The clip buffer is also the PC input ring: without a PC stream the board
plays the booth clips (repeated to 1.024 s); with one, PRU1 writes the PC
audio 0.5 s ahead of RTU1. Its size is a power of two so PRU1 wraps an index
with an AND.

The frame protocol (raw Ethernet II, EtherType 0x88B5, board MAC
02:00:5D:4D:00:01) is defined once in `firmware/eth_proto.inc`, mirrored in
`mcuplus/eth_proto.h` and `../pc_app/src/proto.rs`; a test checks the three
agree. Each AUDIO_OUT frame carries a status block (HALF_SEQ, RTU1 position,
UNDERRUN_COUNT, SD_OVF_COUNT, the ICSSG1 RX counters, the IEP time the half was
seen), the PCM of 32 frames x 8 channels and a sample index for
reconstruction.

## Wiring and PC setup

- The nine PDM jumpers, SW6 and the "no BoosterPack" rule as in the
  [pdm_audio readme](../../pdm_audio/pdm_audio/readme.md).
- An Ethernet cable from the PC straight to the LP's ICSSG1 port 1 RJ45,
  connector J18 (RGMII2, PHY address 15; the port the HIL example used).
- PC, once: `sudo nmcli device set enp129s0 managed no` (Ubuntu must not send
  its own traffic on the cable; the board drops and counts it anyway).
- PC, after every build of `pdm-eth`:
  `sudo setcap cap_net_raw,cap_net_admin+ep ../pc_app/target/release/pdm-eth`.

## Build and run

From this directory (never from the open-pru root):

```
make DEVICE=am243x \
     MCU_PLUS_SDK_PATH=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33 \
     SYSCFG_NODE=$HOME/ti/ccs2101/ccs/tools/node/node
```

This builds the five PRU firmwares and the R5F application
`mcuplus/am243x-lp/r5fss0-0_freertos/ti-arm-clang/pdm_audio_eth.release.out`.
With CCS: import the six project specs (three `icss_g0_*`, two `icss_g1_*`,
the R5F), build the PRU projects first, then the R5F; or load the make-built
`.out` on r5fss0-0 directly. After a SW1 (MCU PORZ) reset, reconnect CCS and
run the R5F. The console prints
`PDM audio eth: link 1000 FD on ICSSG1 port 1, board MAC 02:00:5d:4d:00:01`,
then the `PDM audio:` line of pdm_audio.

PC application (see [../pc_app/README.md](../pc_app/README.md)):

| Stage | Command |
| ----- | ------- |
| 1. Ethernet test | `pdm-eth test --iface enp129s0` (link, 10 000 pings, 3 s throughput each way) |
| 2. Board to PC | `pdm-eth gui --iface enp129s0` (booth clips), `pdm-eth capture --iface enp129s0 --seconds 600 --out DIR` |
| 3. PC to board | `pdm-eth gui --iface enp129s0 --ch0 a.wav ... --ch7 h.wav`, `pdm-eth stream --iface enp129s0 --ch0 ... --capture DIR` |

The ICSSG0 checks of pdm_audio (CCS memory browser at 0x30010000) still
apply; CLIP_FRAMES now reads 0x4000.

## Hardware results

LP-AM243 Rev A, cable from J18 to the PC's RTL8125, 2026-11-05. Details,
commands and raw numbers: [docs/test_report.md](docs/test_report.md),
section 6.

| Check | Result |
| ----- | ------ |
| B0 single PONG | 1/1, RTT 141 us, board turnaround 0.39 us |
| B1 ICSSG0 header | as in pdm_audio, CLIP_FRAMES 0x4000; after 31 min UNDERRUN_COUNT 0, SD_OVF_COUNT 0, PCM_COUNT of all 8 channels = RTU1 frames +-1 |
| B2 link | console `link 1000 FD on ICSSG1 port 1 after 3610 ms`; `ethtool` 1000/full |
| B3 `pdm-eth test` | 10 000 of 10 000 pings (turnaround 0.3 us idle); 1514 B: 984.4 Mbit/s to the board, 976.0 Mbit/s from it; 64 B: 0.598 Mframes/s to the board (PC NIC limit), 1.459 Mframes/s from it (98 % of line rate); no loss, 3 of 3 runs |
| B4 clip buffer | identical to `mcuplus/pdm_clips.h` after B3 (`tools/compare_clip_dump.py`) |
| B5 RX counters | RX_FRAMES = frames of the last burst exactly, RX_BAD 0, RX_FOREIGN 0 |
| B6 capture 600 s | lost / duplicates / resyncs 0, board counters 0; tones 1000, 3000, 500, 2000, 5000 Hz exact, at the pdm_audio levels, -85 to -90 dB residual; CCS half A/B dump found bit for bit in the capture (`tools/analyze_capture.py`, `tools/match_half_dump.py`) |
| B7 GUI | 11 min, 56 to 60 fps, lost 0 |
| B8 PC to board | 70 s, 8 channels: 17 500 of 17 500 AUDIO_IN blocks landed, no underrun or stale block, output = input through the chain to -85 to -94 dB (`tools/make_b8_inputs.py`, `tools/analyze_stream.py`); spectrograms alike in the GUI |
| B9 latency | board audio latency 1.08 ms, constant |

Fixed during the board run: the ICSSG IEP count offset, a frame lost after
an audio pair (0.3 us guard), the PHY address of port 1 (15, not 3, the
console said link DOWN), the post-EOF wait (100 to 5 cycles) and the
inter-frame gap (`TX_IPG` 0x28 core clocks, 109 ns), plus spectrogram axes
in the GUI (test_report.md 6.1). Open: the GUI's board-clock ppm figure is
noisy and disagrees with the capture's (no effect on the audio).

The checks as planned (docs/plan.md, deferred board checks):

1. Header check as in pdm_audio, CLIP_FRAMES = 0x4000, UNDERRUN_COUNT and
   SD_OVF_COUNT 0.
2. Console link line; `ethtool enp129s0`: 1000 Mb/s, full duplex, link.
3. `pdm-eth test --iface enp129s0`: every line PASS (link 1000/full, 0 lost
   pings, at least 500 Mbit/s each way with less than 0.1 % loss); record the
   RTT and the board turnaround. Then save the clip buffer (CCS memory
   browser, 0x700C0000, 0x40000 bytes) and compare it with `mcuplus/pdm_clips.h`:
   pings and throughput frames must not touch it. Watch RX_BAD against
   RX_FRAMES (ICSSG1 shared RAM 0x30090010): RX_BAD rising with every frame
   would mean the EOF flag is still set when the next frame starts.
4. `pdm-eth capture --seconds 600`: lost 0, resyncs 0, tx_late 0; tones of
   channels 3 to 7 at 1000, 3000, 500, 2000, 5000 Hz at the pdm_audio levels;
   one CCS read of half A against the capture.
5. `pdm-eth gui`: channel 3 a 1 kHz line, channel 1 the chirp, at least 30 fps
   while switching channels, lost 0 over 10 minutes.
6. Eight WAVs of at least 60 s through `gui` and `stream --capture`: input
   underruns, blocked and lost 0, UNDERRUN_COUNT and SD_OVF_COUNT 0, input and
   output spectrograms alike, the board audio latency stable within 1 ms;
   record it. Back-to-back AUDIO_IN frames must both land (compare
   `in_ch*.wav` and `out_ch*.wav`).

First board run, in this order, because these TX paths were never exercised
by the HIL example (it sent one 64-byte frame and halted): a single PONG
(`pdm-eth test --pings 1 --seconds 0.1`), then the pings, then the throughput
bursts (multi-chunk frames with the 42-byte tail, back-to-back frames after
CRC/EOF), then audio (the 30-byte tail).

## Known limits

- **If the PC falls more than the 0.5 s lead behind**, RTU1 replays old ring
  audio until the PC catches up; the PC counts it as an input underrun and
  restarts the lead.
- **Throughput loss on the PC side**: at full rate the RTL8125 or the socket
  buffer can drop frames; `pdm-eth test` reports kernel and NIC drops as PC
  loss, not as board loss. On the test PC the NIC interrupt has its own
  isolated CPU and the network thread another (test_report.md section 2);
  with both on one CPU the NIC dropped frames.
- **Inter-frame gap** is set by MII_RT `TX_IPG` in core clocks (0x28 = about
  109 ns on the wire at 333 MHz); the TX-interface-clock mode was slower on
  the board (test_report.md 6.3). The gap was derived from frame rates, not
  measured with a scope.
- **Fixed frame sizes**: XOUT sizes are immediates, so PONG and TPUT_REPORT
  are 64 B, TPUT_DATA 1514 B and AUDIO_OUT 1118 B.
- **Board audio latency** is the best case over recent frames (youngest
  sample of each frame): the pipeline plus the send, without the wait inside
  a 4 ms half.
- **Changes against the HIL example**: the RGMII2/MDIO pads are set with the
  SDK Pinmux_config (the HIL kick writes targeted other registers), the HIL
  step that enabled PRU0 before its firmware was loaded and the PHY table
  store at ICSSG1 shared RAM + 0x04 are dropped, the PHY addresses of the two
  ports are swapped (port 1 / J18 is PHY 15), `TX_IPG` is 0x28 instead of
  0x18 (0x18 was 96 ns at 250 MHz) (`mcuplus/eth_init.c`), and PRU0 waits 5
  cycles after CRC + EOF instead of 100 (`firmware/eth_tx.asm`).

## Project layout

```
examples/electronica2026/pdm_audio_eth/
  pdm_audio_eth/              board project (this directory)
    makefile                  5 PRU firmwares + R5F
    docs/                     design.md, plan.md, test_report.md, screen_mockup.html, GUI screenshots
    firmware/
      rtu1_mod.asm pru1_pdm.asm pru0_rx.asm pdm8_shared.inc fir_coefs.inc decimation_fir.h   from pdm_audio, unchanged
      eth_proto.inc           frame protocol + ICSSG1 shared-RAM map
      eth_rx.asm              ICSSG1 PRU1
      eth_tx.asm              ICSSG1 PRU0
      am243x-lp/icss_g{0,1}_*_fw/ti-pru-cgt/   projectspec, makefile, linker.cmd
    mcuplus/
      pdm_audio_eth_example.c R5F: ICSSG0 as pdm_audio, ICSSG1 Ethernet, start order
      eth_init.c/.h           ICSSG1 MII_RT / RGMII / IEP / MDIO / PHY init (from the HIL example)
      eth_proto.h pdm8_shared.h pdm_clips.h
      am243x-lp/r5fss0-0_freertos/   example.syscfg, main.c, ti-arm-clang/
    tools/                    make_clip_buffer.py (--frames, default 16384), gen_fir_coefs.py, check_icssg0_copy.sh
    clips/                    booth clips (as pdm_audio)
  pc_app/                     Rust crate pdm_eth, binary pdm-eth
```
