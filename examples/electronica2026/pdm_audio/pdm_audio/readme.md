# PDM Audio, 8 channels (electronica 2026)

## Introduction

An 8-channel PDM microphone chain that runs on one AM243x LaunchPad, wired back
to itself with jumper wires. No microphones are needed.

- **RTU1** reads 8 audio clips from MSRAM and runs 8 second-order sigma-delta
  modulators. It hands the bits to PRU1 through the IPC scratchpad.
- **PRU1** drives a cycle-exact 3.072 MHz PDM clock and one PDM data pin per
  channel (8 data pins).
- **PRU0** receives the 8 PDM streams on the sigma-delta (SD) inputs of ICSSG0
  with one shared clock. It decodes each channel to 16 kHz PCM: SINC3 at OSR 96,
  63-tap FIR with decimation by 2, 20 Hz DC blocker, 24-bit saturation.
- The PCM goes into a sample-interleaved ping-pong buffer in ICSS shared RAM.
  A later Ethernet PRU can read it and stream it to a PC. This project only
  defines that interface (see "Reader interface").
- **R5F** (r5fss0-0, FreeRTOS) sets the 333.33 MHz ICSSG0 clock, the pin mux
  and the ICSSG configuration, loads the three PRU firmwares, starts them and
  idles.

The firmware was developed and checked in TI's pru-simulator (up to 3 channels,
bit-exact against the Python reference) and then ported to the AM243x
LaunchPad as the full 8-channel design. The design is in
[docs/design.md](docs/design.md); the porting effort and the decisions are in
[docs/porting_report.md](docs/porting_report.md).

**Nothing has run on a board yet.** Every hardware statement in this readme is
either from the TRM and the schematic, or from a simulator or build result.
Items that need the board are marked `FIXME`.

## Supported Combinations

| Processor | Board            | PRU cores                   | Host              |
| --------- | ---------------- | --------------------------- | ----------------- |
| AM243x    | AM243x LaunchPad | ICSSG0 RTU1, PRU1 and PRU0  | r5fss0-0 FreeRTOS |

Refer to open-pru/examples/readme.md > Supported processors per-project for
porting information. This project is AM243x only: the pin table, the 333.33 MHz
clock and the MSRAM clip region are specific to the LP-AM243.

## Validated HW & SW

| Processor | Hardware        | Software |
| --------- | --------------- | -------- |
| AM243x    | LP-AM243 Rev A (FIXME: not run yet) | Builds with MCU+ SDK 12.01.00.33, CCS 21.0.1, ti-cgt-pru 2.3.3, ti-cgt-armllvm 5.1.1.LTS, SysConfig 1.28.1 (build and simulator only; FIXME: hardware validation) |

The open-pru release notes list MCU+ SDK 11.1 - 12.0 as validated for AM243x,
so 12.01 is one minor version newer than validated.

## Overview

### Data flow

```
 MSRAM 0x700C0000          IPC scratchpad            ICSSG0 pins (jumpers)          ICSS shared RAM
 8 clips, int16     RTU1 ----------------> PRU1 ----> clock + 8 data ----> PRU0 ----> 0x30010000
 (128 000 B, loops) 8 x sigma-delta         28 periods   BP.11 + 8 pins    8 x SD      header + 2 halves
                                            per batch                      chain       (64 frames x 8 ch)
```

### Clocks and rates

| Item                    | Value                                                   |
| ----------------------- | ------------------------------------------------------- |
| ICSSG0 core clock       | 333 333 333 Hz (`example.syscfg`, `coreClk`)            |
| PDM clock               | 3.072 MHz = 108 + 73/144 cycles per period (71 periods of 108 and 73 of 109 per 144) |
| PCM rate                | 16 kHz (OSR 96 SINC3, then FIR / 2)                     |
| Ping-pong half          | 64 frames x 8 channels = 4 ms, 250 halves per second    |
| Batch                   | 28 PDM periods, about 109 700 batches per second        |

The firmware is cycle-counted for 333.33 MHz. At 200 MHz one PDM period is 65
cycles, and RTU1 cannot run 8 modulators in that time.

### Clips and channel order

Each channel plays a different clip, so a channel swap or crosstalk is visible
in the memory browser. The clips are 0.5 FS (peak 16384 of 32768). The clip
buffer is one frame-interleaved array (8 x int16 per frame, 8000 frames = 0.5 s)
that loops. Shorter clips repeat from their start inside the buffer.

| Channel | Clip                       | Content                          |
| ------- | -------------------------- | -------------------------------- |
| 0       | `speech_librivox.wav`      | speech (LibriVox)                |
| 1       | `chirp_lin_100_7000.wav`   | linear chirp 100 - 7000 Hz       |
| 2       | `chirp_log_100_7000.wav`   | logarithmic chirp 100 - 7000 Hz  |
| 3       | `tone_1000.wav`            | 1 kHz sine                       |
| 4       | `tone_3000.wav`            | 3 kHz sine                       |
| 5       | `tone_500.wav`             | 500 Hz sine                      |
| 6       | `tone_2000.wav`            | 2 kHz sine                       |
| 7       | `tone_5000.wav`            | 5 kHz sine                       |

Provenance and how the tones were generated: [clips/SOURCES.md](clips/SOURCES.md).
`tools/make_clip_buffer.py` converts the clips into `mcuplus/pdm_clips.h`
(`--check` compares the committed header with the clips).

### PRU resources

| Resource        | Setting |
| --------------- | ------- |
| Cores           | ICSSG0 RTU1, PRU1, PRU0 (RTU0, TX_PRU0, TX_PRU1, RTU_PRU0 not loaded) |
| GPCFG0 (PRU0)   | GP_MUX_SEL = 3 (SD), set by the R5F |
| GPCFG1 (PRU1)   | GP_MUX_SEL = 0 (GP, direct GPO), set by the R5F |
| SD channels     | `CLK_SEL` = 0 (sd8_clk = GPI16), `CLK_INV` = 0, ACC3, `SD_SAMPLE_SIZE` = 95 (OSR 96) |
| IPC scratchpad  | XFR ID 15 (R2..R9), RTU1 to PRU1 |
| PRU1 DRAM1      | RTU1 scratch at 0x1700..0x17FF (PRU1's timed path touches no memory) |
| PRU0 DRAM0      | Receiver state 0x07F8..0x19FF (publish block at 0x07F8) |
| Shared RAM      | 64 KB at 0x10000 (local) = 0x30010000 (R5F / CCS global address) |
| MSRAM           | Clip buffer `.pdm_clips` at 0x700C0000: 128 000 B (in a 128 KB region), non-cacheable (MPU region 5) |

### Cycle budgets (pru-simulator, 333.33 MHz)

| Core | Measured | Limit | Share |
| ---- | -------- | ----- | ----- |
| RTU1, worst case per 28-period batch | 1327 cycles | 2278 | 58 % |
| PRU1, busy per batch | about 418 of 3038 cycles | fixed | about 14 % busy, the rest is padding |
| PRU1, setup before the sampling edge | 54 cycles | at least 26 | |
| PRU0, 8-channel worst window (extrapolated) | 4074 cycles | 10 417 | 39 % |
| PRU0, pessimistic bound | 4439 cycles | 10 417 | 43 % |

RTU1's MSRAM read latency on silicon is not measured. The simulator reads
MSRAM in about 24 cycles. The spare per batch is 2278 - 1327 = 951 cycles; with
the 24 simulated cycles of the fetch that are already in the 1327, a fetch of
up to about 975 cycles per frame would still fit in the RTU1 budget.

## Steps to Run the Example

### Hardware setup

Jumper wires connect PRU1 outputs (PDM clock and data) to PRU0 inputs (the SD
channels). Pin numbers are BoosterPack pin numbers as printed on the LP
headers.

| Signal  | PRU1 output (pin)  |     | PRU0 input (pin)  | SD                       |
| ------- | ------------------ | --- | ----------------- | ------------------------ |
| clock   | GPO0 (BP.11)       | to  | GPI16 (BP.7)      | SD8_CLK, shared by all channels |
| ch0     | GPO1 (BP.67)       | to  | GPI1 (BP.32)      | SD0_D                    |
| ch1     | GPO2 (BP.68)       | to  | GPI3 (BP.19)      | SD1_D                    |
| ch2     | GPO6 (BP.69)       | to  | GPI5 (BP.13)      | SD2_D                    |
| ch3     | GPO11 (BP.70)      | to  | GPI7 (BP.44)      | SD3_D                    |
| ch4     | GPO12 (BP.72)      | to  | GPI11 (BP.43)     | SD5_D (SD4 is skipped: its GPI9 goes to the RGMII PHY) |
| ch5     | GPO13 (BP.71)      | to  | GPI13 (BP.15)     | SD6_D                    |
| ch6     | GPO15 (BP.59)      | to  | GPI15 (BP.12)     | SD7_D                    |
| ch7     | GPO16 (BP.57)      | to  | GPI17 (BP.18)     | SD8_D                    |

The SysConfig pads are `PRG0_PRU1_GPO<n>` (mux mode 0, GPO) for the outputs
and `PRG0_PRU0_GPO<n>` (mux mode 1, GPI with the receiver on) for the inputs.

**Ground pins:** BP.20, BP.22, BP.60 and BP.62. The jumpers above are all on
one board and need no ground wire. Use one of these pins as the reference when
you probe a signal with a scope or logic analyser.

**Board settings (check these before powering up):**

- **SW6 (BPSERVO_MUX_SEL) must be in the default position, with LD17 off.**
  The SD5, SD6 and SD7 data paths and PRU1_GPO16 go through TMUX1574 switches
  that SW6 controls.
- **No BoosterPack may be plugged in.** The pins above are the BoosterPack
  header pins.
- Do not use a pin from the "not usable" list in the design (Ethernet PHY,
  eQEP, CAN, LEDs). The pin table above avoids them.

The PRU1 pads are driven push-pull (GPO mode, pull disabled in SysConfig). They
are expected to be 3.3 V pads, as on the other electronica2026 demos.
FIXME: confirm the I/O voltage of these pads against the LP-AM243 schematic
(PROC109A); it is not stated in this repo or in the SDK docs. Keep the jumpers
short; the clock is 3.072 MHz.

### Before you start

This is the one-time setup for a new PC and board. The repo and SDK docs
describe each step, so they are linked instead of repeated.

1. **Tools.** Install MCU+ SDK for AM243x (this project was built with
   12.01.00.33) and CCS (built with 21.0.1), as described in
   [docs/getting_started_mcuplus.md](../../../../docs/getting_started_mcuplus.md)
   ("Install the SDK", "Install the tools"). The ti-cgt-pru 2.3.3,
   ti-cgt-armllvm 5.1.1.LTS and SysConfig 1.28.1 used here came with that CCS
   and SDK. On a Linux PC follow the SDK's "CCS Linux Host Support" steps that
   page points to. (`docs/getting_started_linux.md` is about the Linux SDK for
   A53 cores and is not needed here.)
2. **imports.mak.** Copy `imports.mak.default` to `imports.mak` in the open-pru
   root and edit it as the `UPDATE` comments say
   ([same page](../../../../docs/getting_started_mcuplus.md), "Set up
   imports.mak"). Or leave it and pass the `make` overrides shown under "Build
   and run".
3. **CCS target configuration for the LP.** Create it as in the MCU+ SDK docs
   (MCU+ SDK docs > Getting Started > Download, Install, and Setup CCS,
   section "Create Target Configuration"). Do **not**
   bypass the ICSS / ICSS_G cores: the repo says the CCS debugger cannot
   connect to a PRU core that has been bypassed
   ([docs/getting_started_mcuplus.md](../../../../docs/getting_started_mcuplus.md),
   "Install the tools"). This demo is loaded by the R5F, but you will want to
   look at the PRU cores in CCS.
4. **Board connections, power and boot mode.** Follow the MCU+ SDK docs page
   "EVM Setup" (`docs/api_guide_am243x/EVM_SETUP_PAGE.html` in the SDK), section
   AM243X-LP:
   - power: 5 V / 3 A on the USB Type-C connector J10 (LD9 red means a power
     error);
   - JTAG: the USB cable to the on-board XDS110;
   - boot mode: set the boot switches to **DEV BOOT** as shown in that
     page, and run the "SOC Initialization" step it describes. The page
     says that step is needed every time the LP is power-cycled.

   FIXME: the exact switch positions, and whether the SOC initialization
   script is needed for this demo, are not written here because they are not
   stated in this repo and nothing has run on a board yet.
5. **Connect, load, run.** Launch the target configuration, connect to
   `MAIN_Cortex_R5_0_0` (the SDK docs call it R5F0-0, r5fss0-0 here), load
   `pdm_audio_am243x-lp_r5fss0-0_freertos_ti-arm-clang` and run.
6. **Console (optional).** The R5F prints one `DebugP_log` line at start-up
   (see "Build and run"). The AM243X-LP has a single UART port from the XDS110,
   used for the console; open it in a terminal at 115200 baud, 8 data bits, no
   parity, 1 stop bit (SDK "EVM Setup", "UART Terminal"). The demo uses the SDK
   default UART console (`CONFIG_UART_CONSOLE` on USART0, SDK default 115200
   baud, see `example.syscfg`). FIXME: not checked on this board. The demo
   works without the console.

### Build and run

All paths are relative to this project directory
(`examples/electronica2026/pdm_audio/pdm_audio`).

**With make** (from this directory, never from the open-pru root: a root build
dirties `pru_i2s`):

```
make DEVICE=am243x \
     MCU_PLUS_SDK_PATH=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33 \
     SYSCFG_NODE=$HOME/ti/ccs2101/ccs/tools/node/node
```

This builds the three PRU firmwares (`clpru` and `hexpru`), runs SysConfig,
builds the R5F application and generates `pdm_audio.release.appimage.hs_fs`.
`make clean` with the same overrides removes the build output. The overrides
are only needed when `imports.mak` does not point at your SDK and node.

**With CCS:**

1. Import the four project specs (Project > Import CCS Projects, or the
   `ccs-server-cli.sh projectCreate` command):
   - `firmware/am243x-lp/icss_g0_rtu_pru1_fw/ti-pru-cgt/example.projectspec`
   - `firmware/am243x-lp/icss_g0_pru1_fw/ti-pru-cgt/example.projectspec`
   - `firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/example.projectspec`
   - `mcuplus/am243x-lp/r5fss0-0_freertos/ti-arm-clang/example.projectspec`
2. Build the three PRU projects first (they generate `rtupru1_load_bin.h`,
   `pru1_load_bin.h` and `pru0_load_bin.h`), then the R5F project
   `pdm_audio_am243x-lp_r5fss0-0_freertos_ti-arm-clang`.
3. Load and run the R5F project on the r5fss0-0 core. The console prints
   `PDM audio: ICSSG0 RTU1, PRU1, PRU0 running; 8 channels, clips at 0x700c0000 (8000 frames), PCM header at 0x30010000`.

The clips are linked into the R5F image (section `.pdm_clips` at 0x700C0000),
so the ELF load writes them. The R5F writes no clip data at run time.

**Restart rule.** RTU1 and PRU1 must always be restarted together. The IPC
scratchpad handshake only recovers from a joint restart. Restarting one of the
two leaves the pair stuck (PRU1 plays silence forever, or waits for a start
that never comes). The R5F does this: it halts all three cores, reloads them
and starts RTU1, then PRU1, then PRU0. So **after a CCS reload, or whenever you
change a PRU firmware, restart all cores through the R5F** (run the R5F project
again). Do not restart a single PRU core from the CCS register view.

The R5F writes the whole GPCFG0, GPCFG1 and SD_CFG registers and the PRU
program counter reset value before it starts the cores. So the demo also runs
after another ICSSG0 demo (uart_print, i2c_io, uart_rx) was loaded from CCS
without a System Reset (by design; FIXME: not tried on a board).

### Check in the CCS memory browser

The R5F runs in its idle loop, and the PRU cores run on their own. Connect to
the r5fss0-0 core, open View > Memory Browser, and enable continuous refresh.
FIXME: reading the shared RAM while the R5F is running (continuous refresh) has
not been tried on this board. If CCS refuses or shows stale values, halt the
R5F core: the PRU cores run on their own and keep going (also not tried), and
refresh the view by hand.
The ICSS shared RAM is at 0x30010000. All addresses below are global
addresses.

| Address      | Field            | Expected |
| ------------ | ---------------- | -------- |
| 0x30010000   | MAGIC            | 0x50444D38 (`PDM8`) |
| 0x30010004   | N_CH             | 8 |
| 0x30010008   | FRAMES_PER_HALF  | 64 |
| 0x3001000C   | PCM_RATE         | 16000 |
| 0x30010010   | CLIP_ADDR        | 0x700C0000 |
| 0x30010014   | CLIP_FRAMES      | 8000 |
| 0x30010018   | HALF_SEQ         | rising, about 250 per second |
| 0x3001001C   | UNDERRUN_COUNT   | stays 0 |
| 0x30010020   | BATCH_COUNT      | rising, about 110 000 per second |
| 0x30010024   | RTU1_FRAME_POS   | clip frame being modulated (0 to 7999) |
| 0x30010028   | RTU1_LOOPS       | rises once per 0.5 s clip loop |
| 0x30010040 ..5F | PCM_COUNT[0..7] | all rising |
| 0x30010100   | half A           | 64 frames x 8 ch x int32 |
| 0x30010900   | half B           | same |

Steps:

1. **Header.** Read MAGIC, N_CH, FRAMES_PER_HALF, PCM_RATE, CLIP_ADDR and
   CLIP_FRAMES. They are written by the R5F before the cores start.
2. **HALF_SEQ** rises about 250 times per second (16 000 / 64). Read it twice a
   second apart.
3. **BATCH_COUNT** rises about 110 000 times per second (3.072 MHz / 28). This
   is the liveness check of the whole chain: a frozen BATCH_COUNT with
   UNDERRUN_COUNT 0 means RTU1 has stalled, because RTU1 writes both counters.
4. **UNDERRUN_COUNT** stays 0. Each underrun is one batch that PRU1 replaced
   by silence because RTU1 was late. The clock does not change when that
   happens.
5. **PCM_COUNT[0..7]** (0x30010040 + 4 * channel): read all eight and check
   that every one rises. Channels 3 to 7 could only be verified statically
   before the board run (the simulator has 3 SD channels), so this step is the
   first check of them. The counts should be close to each other: the odd
   channels run one SD window (31.25 us, half a PCM frame) behind the even ones
   because of the FIR stagger, and PCM_COUNT[c] is 64 * HALF_SEQ when a half is
   published (this is what the simulator test checks), so the eight counts
   differ by one or two, plus the skew of reading them one after another.
   FIXME: not seen on a board yet; a count that is far from the others (or
   stuck) is the sign of a problem.
6. **Waveforms.** Read half A (0x30010100) and half B (0x30010900) and look at
   one channel's column: sample (frame f, channel c) is at `half + f * 32 + c * 4`,
   so a channel's samples are 32 bytes apart. Read the memory as 32-bit signed
   integers. The tone channels show a clean sine with these periods:

   | Channel | Clip    | Period in frames (16 kHz) | Frames per half |
   | ------- | ------- | ------------------------- | --------------- |
   | 3       | 1 kHz   | 16                        | 4 periods       |
   | 4       | 3 kHz   | about 5.33                | about 12 periods |
   | 5       | 500 Hz  | 32                        | 2 periods       |
   | 6       | 2 kHz   | 8                         | 8 periods       |
   | 7       | 5 kHz   | 3.2                       | 20 periods      |

   The level is about +-2^22 (+-4 194 304, 0x00400000) for the 0.5 FS clips.
   Channel 0 is speech, and channels 1 and 2 are chirps, so they change over
   time. A frozen column that never moves between reads of a changing
   HALF_SEQ means that channel's data is not arriving.
7. **Which half is ready.** The half that `HALF_SEQ` just completed is
   `(HALF_SEQ - 1) & 1`. Read that half (0 = A, 1 = B). PRU0 is writing the
   other half, so read the ready half, not the other one.
8. **Optional: memory graph of one channel.** FIXME: the dialog and field names
   below are written from memory of the CCS Graph Properties dialog and were
   not checked against CCS 21.0.1 or a board; the values are what is meant.
   Tools > Graph > Single Time, then set:
   - start address: `0x30010100 + 4 * channel` (for example 0x3001010C for
     channel 3);
   - acquisition buffer size: 64;
   - display data size: 64;
   - DSP data type: 32-bit signed integer;
   - index increment: 8 (8 words = 32 bytes, the channel stride);
   - sampling rate: 16000 Hz.

   A graph of half A and a second graph of half B (start address 0x30010900 +
   4 * channel) show both halves.

FIXME: record the hardware results here after the first board run (HALF_SEQ
rate, BATCH_COUNT rate, PCM_COUNT[0..7], per-channel periods and the level).

There is no reliable single-point check of the SD shadow-overflow flag
(R31[29]) from the CCS register view: PRU0 clears the valid and overflow bits
of every channel in each window, so a halted PRU0 shows at most the current
window's flag. Use PCM_COUNT, HALF_SEQ and the waveforms instead.

### If something looks wrong

| Symptom | Likely cause |
| ------- | ------------ |
| All PCM_COUNT stay 0 | PRU0 gets no clock or no SD routing. Check GPCFG0 mux = 3 (SD), the pin mux of GPI16 (BP.7) and the clock jumper BP.11 to BP.7, and that SW6 is in the default position. |
| One channel's PCM_COUNT is 0, or its column is flat | That channel's jumper (PRU1 GPO to PRU0 GPI pin). |
| Channels 4 to 7 flat, 0 to 3 fine | SW6 or LD17: the SD5, SD6 and SD7 data paths and PRU1_GPO16 go through the TMUX1574 switches. Also check that no BoosterPack is plugged in. |
| UNDERRUN_COUNT rises | RTU1 is late. The likely cause is MSRAM latency on silicon: the simulator reads in about 24 cycles and cannot show the real value. The clock stays correct, and the affected batches are silence. |
| BATCH_COUNT frozen, UNDERRUN_COUNT 0 | RTU1 has stalled. Restart all cores through the R5F. |
| HALF_SEQ frozen, BATCH_COUNT rising | PRU0 stopped, or an SD channel does not deliver. Check PCM_COUNT per channel (step 5). |
| Everything frozen after a CCS reload | A core was restarted alone. Restart all cores through the R5F. |
| Waveform shape wrong but counters fine | Check the clock for glitches. Look at the half-period of the PDM clock in period 27 of each 28-period batch: that is where PRU1 does its IPC scratchpad XIN and XOUT, so contention there would show as one stretched half period. The simulator keeps all half periods at 54/55 cycles. |

## Reader interface (for the Ethernet PRU)

The PCM buffer is in ICSS shared RAM at local address 0x10000 (global
0x30010000 from the R5F, and from a core in ICSSG1 too). It is the interface
the next project reads.

| Offset | Size  | Contents                             | Writer      |
| ------ | ----- | ------------------------------------ | ----------- |
| 0x0000 | 256 B | Header (below)                       | R5F / cores |
| 0x0100 | 2 KB  | Half A: 64 frames x 8 channels x int32 | PRU0      |
| 0x0900 | 2 KB  | Half B: same                         | PRU0        |

- Sample (frame f, half h, channel c) is at
  `0x0100 + h * 0x800 + f * 32 + c * 4`.
- Each sample is a signed 24-bit value stored in an int32 (+-(2^23 - 1)),
  16 kHz. Within a half the order is s0.ch0 .. s0.ch7, s1.ch0 .. s1.ch7, and
  so on. A reader copies one half of 2048 bytes in order.
- `HALF_SEQ` (u32 at +0x18) counts completed halves. PRU0 writes it after the
  last sample of every channel of a half. **The ready half is
  `(HALF_SEQ - 1) & 1`.** PRU0 fills the other half. The ready half does not
  change for 4 ms, until PRU0 has written the next half.
- A reader should poll `HALF_SEQ`, and when it changed, copy the half
  `(HALF_SEQ - 1) & 1`, then read `HALF_SEQ` again. If it advanced by 2 or
  more, a half was missed (the reader was slower than 4 ms).
- Header (all u32, offsets from the start of shared RAM). The authoritative
  list is `firmware/pdm8_shared.inc` (and its C mirror `mcuplus/pdm8_shared.h`).

  | Offset      | Field            |
  | ----------- | ---------------- |
  | 0x00        | MAGIC `PDM8` = 0x50444D38 |
  | 0x04        | N_CH = 8         |
  | 0x08        | FRAMES_PER_HALF = 64 |
  | 0x0C        | PCM_RATE = 16000 |
  | 0x10        | CLIP_ADDR (global address of the MSRAM clip buffer) |
  | 0x14        | CLIP_FRAMES (8000) |
  | 0x18        | HALF_SEQ         |
  | 0x1C        | UNDERRUN_COUNT   |
  | 0x20        | BATCH_COUNT      |
  | 0x24        | RTU1_FRAME_POS   |
  | 0x28        | RTU1_LOOPS       |
  | 0x40 - 0x5F | PCM_COUNT[0..7]  |

- There is no R31 event per half. The reader polls `HALF_SEQ`.

## Limitations and notes

- **Odd channels are late.** The FIR stagger on PRU0 (even and odd channels
  alternate) delays the PCM of the odd channels by 1/32 kHz (31.25 us)
  relative to the even channels. Irrelevant for this demo, but it matters for
  later beamforming across channels. Within a frame the odd channels' sample is
  not simultaneous with the even channels'.
- **Modulator input range.** The RTU1 modulators use int32 integrators.
  Clips with a peak of 0.5 FS are safe (the clips are normalised to that).
  Sustained input near full scale wraps the integrators. Keep the input well
  below FS if you replace the clips.
- **DRAM0 overlap.** PRU0's receiver state uses DRAM0 0x07F8..0x19FF (publish block at 0x07F8). The
  range 0x1000..0x1A00 is the open-pru default share of RTU0 and TX_PRU0. This
  demo does not run those cores. A later demo that adds RTU0 or TX_PRU0 would
  collide.
- **iepClk** stays at its default (200 MHz). The demo does not use the IEP.
  Ethernet-reader work may want to set it.
- **SysConfig GUI.** `example.syscfg` was edited by hand and validated with
  the SysConfig command line and both builds. Open it once in the SysConfig GUI
  and save (FIXME, open item).
- **Eight channels is the hard maximum**, because only 9 PRU1 GPO pins reach
  the headers (clock plus 8 data).
- **Do not enable PRU0 SD_CLKOUT or shift-out.** The TRM routes the PRU0 SD
  clock onto PRU1_GPO1 in that mode, which is channel 0's data pin.
- **Silicon-only unknowns:** the RTU1 MSRAM latency, the XOUT to XIN visibility
  between RTU1 and PRU1 (assumed one cycle each, as in the simulator), and pad
  delays. The PDM clock needs no check if the counters behave. If you want to
  measure it: the average period is 325.5 ns with a 54/54 cycle duty.
- Build outputs (`*_load_bin.h`, `generated/`, `.out`, `.appimage`) are not
  committed.

## Verification

The firmware was checked in TI's pru-simulator, on the branch
`feat/pdm-open-pru-check` (stacked on PR #106, the PDM demo). The simulator
stays at 3 SD channels, so the harness loads the same 8-channel sources with the
plain-literal build options `N_CH` and the SD map overridden to 3 channels
{0, 1, 2} at load time. The 8-channel build itself is assembled (simulator and
clpru) but not run.

The harness is `tests/test_pdm_open_pru_port.py` in pru-simulator. It finds
this project through the environment variable `OPEN_PRU_PATH` (default
`~/GitHub/open-pru`) and skips itself if
`examples/electronica2026/pdm_audio/pdm_audio` is missing there.

```
cd pru_simulator
git checkout feat/pdm-open-pru-check
OPEN_PRU_PATH=/path/to/open-pru env -u PYTHONPATH .venv/bin/python -m pytest tests/test_pdm_open_pru_port.py -q
```

(`env -u PYTHONPATH` is only needed on a PC with a ROS environment.) 44 tests,
about 80 s. The tests, by core:

- **RTU1 alone** (a Python stand-in acks the batches): `test_shared_inc_names`,
  `test_rtu1_clears_spad_at_start`, `test_rtu1_start_nonce_differs_from_stale_ack`,
  `test_rtu1_holds_batch0_until_nonce_echo`, `test_rtu1_publishes_pru1_counters`,
  `test_rtu1_batches_match_reference` (8 channels, bit-exact against
  `reference.sigma_delta2`), `test_rtu1_int16_extremes`,
  `test_rtu1_no_clip_plays_silence`, `test_clip_wraps_mid_batch`,
  `test_rtu1_waits_for_ack`, `test_rtu1_budget_checkpoint`.
- **RTU1 + PRU1 in lockstep:** `test_reference_clock_333mhz`,
  `test_clock_follows_333mhz_schedule` (edges equal the 108 + 73/144 schedule),
  `test_pins_carry_reference_bits`, `test_setup_margin`,
  `test_pru1_waits_for_first_batch`, `test_rtu1_started_first_does_not_deadlock`,
  `test_stale_spad_does_not_deadlock` (5 stale states),
  `test_underrun_keeps_clock_and_flags`, `test_rerun_clears_handshake`,
  `test_pru1_timed_path_has_no_shared_memory_access`,
  `test_pru1_leaves_rtu1_scratch_alone`, `test_seq_wraps_at_16_bits`.
- **All three cores in lockstep:** `test_fir_coefs_regenerate_identically`,
  `test_interleaved_pcm_matches_reference` (PCM bit-exact against
  `reference.pcm_chain_for_channel`), `test_half_published_only_when_complete`,
  `test_half_seq_rate`, `test_rx_writes_only_its_memory`,
  `test_rx_idles_without_clock`, `test_worst_window_budget_8ch`,
  `test_8ch_build_assembles`, `test_rx_build_options_guarded`.
- **Project files:** `test_clip_buffer_header_matches_loader` (the clip header
  equals `pdm_layout.load_clip`), `test_c_header_matches_inc` (`pdm8_shared.h`
  equals `pdm8_shared.inc`).

Measured budgets (333.33 MHz):

| Core | Result |
| ---- | ------ |
| RTU1 | worst 1327 of 2278 cycles per 28-period batch (mean 1254, min 1241) |
| PRU1 | about 418 of 3038 cycles busy per batch (about 14 %); setup before the sampling edge 54 cycles (needs at least 26), hold 54/55 cycles; one R30 write per half period |
| PRU0 | 3-channel measurement: w_comb 48, w_fir 920, spin 1, wrap 3. 8-channel extrapolation: 8 x 48 + 4 x 920 + 7 + 3 = 4074 of 10 417 cycles (39 %). Pessimistic bound 4439 (43 %) |

Build results (2026-11-04, this PC):

| Check | Result |
| ----- | ------ |
| `make DEVICE=am243x ...` in this directory | exit 0, 0 errors; the only warning is the inherited SysConfig part-number notice |
| CCS 21.0.1 headless import and Release build, 4 projects | RTU1, PRU1, PRU0: 0 errors, 0 warnings; R5F: 0 errors, 1 warning (the same SysConfig notice) |
| `clpru -v4` on `rtu1_mod.asm`, `pru1_pdm.asm`, `pru0_rx.asm` | 0 errors, no diagnostics |
| clpru image size | RTU1 0x69C B (8 KB IRAM), PRU1 0x9F4 B (12 KB IRAM), PRU0 0x794 B (12 KB IRAM) |

Final-verification numbers for the full pru-simulator suite (3 known failures, as on the base branch) are in
[docs/porting_report.md](docs/porting_report.md), section 7.

**Not yet verified (FIXME, needs the board):**

- Hardware run of the whole chain, with the memory-browser check above.
- RTU1 MSRAM latency, `UNDERRUN_COUNT` on silicon.
- Channels 3 to 7 (only verified statically, with the simulator at 3 channels).
- The SD routing with GPCFG0 mux 3 and the start-up order on silicon.
- PDM clock and data on a scope or logic analyser (optional).

## Project layout

```
examples/electronica2026/pdm_audio/pdm_audio/
  makefile                 am243x only: three PRU cores + R5F
  readme.md                this file
  docs/design.md           design, memory map, protocol, board check
  docs/porting_report.md   prompt, model, decisions, verification, open items
  docs/plan.md             implementation plan
  clips/                   8 booth clips, SOURCES.md
  tools/                   make_clip_buffer.py (clips -> pdm_clips.h), gen_fir_coefs.py
  firmware/
    pdm8_shared.inc        shared map: header, SPAD protocol, constants
    rtu1_mod.asm           RTU1: 8 sigma-delta modulators
    pru1_pdm.asm           PRU1: cycle-exact clock and pin driver
    pru0_rx.asm            PRU0: 8-channel SD receiver and PCM chain
    fir_coefs.inc, decimation_fir.h   FIR coefficients (from pru-simulator)
    am243x-lp/icss_g0_{rtu_pru1,pru1,pru0}_fw/ti-pru-cgt/   projectspec, makefile, linker.cmd
  mcuplus/
    pdm_audio_example.c    R5F: clock/ICSSG config, load, start
    pdm8_shared.h          C mirror of pdm8_shared.inc
    pdm_clips.h            generated clip buffer (.pdm_clips section)
    am243x-lp/r5fss0-0_freertos/   example.syscfg, main.c, ti-arm-clang/
```
