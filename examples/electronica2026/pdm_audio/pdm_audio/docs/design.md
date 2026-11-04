# Design: 8-channel PDM audio on AM243x LP (open-pru port of the pru-simulator PDM demo)

Date: 2026-11-03 · Status: implemented, board run passed 2026-11-04 (LP-AM243 Rev A) · Target: open-pru `examples/electronica2026/pdm_audio/pdm_audio`, LP-AM243 Rev A (PROC109A), ICSSG0

## 1. Intent

Port the pru-simulator PDM microphone demo (`pru_simulator/source/pdm`, PR #106) to real
silicon as an open-pru example, in the same style as the other electronica2026 ports
(`uart_rx_frame`, `uart_print`, `i2c_io_expander`). The demo scales from 2 to **8 channels**,
and its PCM output is laid out so that a later **Ethernet PRU** can stream it to a PC.

The board is wired back to itself. ICSSG0 PRU1 drives a PDM clock and **one PDM data pin per
channel**, and the samples come from audio clips. Jumper wires connect those pins to the
PRU0 sigma-delta (SD) inputs. PRU0 decodes each channel to 16 kHz PCM. The PCM goes into a
sample-interleaved ping-pong buffer in ICSS shared RAM. Success is checked in the **CCS
memory browser**.

The Ethernet/PC visualisation is the next project. This one must hand it a clean,
documented buffer interface.

## 2. Goals

1. Run up to **8 channels** at a **333.33 MHz** ICSSG0 core clock, with the cycle budgets
   checked on all cores.
2. **PRU1** drives the cycle-exact 3.072 MHz PDM clock and 8 data pins.
3. **RTU1** reads the clips from MSRAM and runs all 8 2nd-order ΣΔ modulators. It passes
   the bits to PRU1 through the **IPC scratchpad** (XFR ID 15). RTU1 cannot drive pins.
4. **PRU0** takes all channels on **one shared SD clock input** and runs the audio_pipe
   chain per channel: SINC3 at OSR 96, 63-tap FIR ↓2 to 16 kHz, 20 Hz DC blocker, sat24.
5. **PCM output**: ping-pong, **64 samples per channel per half**, sample-interleaved
   across the channels. A reader (the future Ethernet PRU) reads one half from start to
   end while PRU0 fills the other, then they swap.
6. The **R5F** (r5fss0-0 FreeRTOS, CCS project) sets up the 333 MHz clock, pinmux and GPCFG,
   puts the clips in MSRAM, fills the shared-RAM header, loads and starts PRU0, PRU1 and
   RTU1, and then idles.
7. **Simulator validation** of up to 3 channels, bit-exact against the Python references,
   before the board run.
8. The same deliverables as the other ports: readme with wiring table and run steps,
   `docs/porting_report.md` (prompt, model, tokens, time, decisions, verification), make
   and CCS builds.

## 3. Non-goals

- The Ethernet PRU and streaming to a PC (next project).
- PicoScope measurements, and real microphones.
- Extending the simulator beyond 3 SD channels (user decision). Channels 3–7 are checked
  only on the board.
- 200 MHz operation. It is only noted: at 200 MHz a PDM period is 65 cycles, and RTU1
  cannot run 8 modulators in that time.
- R5F-side signal checks or console output. The board check is the CCS memory browser.
- Changing the pru-simulator UI/Audio view.

## 4. Anti-goals

- **A PDM clock with jitter caused by memory latency.** PRU1's edge timing must not depend
  on MSRAM, on the scratchpad hand-off, or on bit values.
- **Silent underruns.** If RTU1 is late, PRU1 must flag it in the header and never output
  stale bits without saying so.
- **A half that a reader can see torn.** A half is published only when all channels have
  written all 64 frames into it.
- **Pins that break the board.** No pin from the "not usable" list (§6): Ethernet PHY,
  eQEP, CAN, LEDs.

## 5. Facts this rests on (researched 2026-11-03; sources in brackets)

- **SD:** 9 channels per PRU. In SD mode (GPCFG0 `PRU0_GP_MUX_SEL` = 3), SDn_CLK = GPI(2n)
  and SDn_D = GPI(2n+1). `SD_CLK_SELi` = 0 selects the sd8_clk pin, GPI16, for **any**
  channel, with no grouping limit. CLK_INV is bit 2 and ACC_SEL bits [5:4]. OSR is
  `SD_SAMPLE_SIZE` + 1. [TRM spruim2h Tables 6-417/418, 6-597; Motor Control SDK
  sdfm_api.h "common SDFM clock pin for all channels"]
- **The IPC scratchpad** is XFR ID 15: 32 bytes mapped to R2..R9, one copy per slice
  (PRU1↔RTU1), with **no hardware arbitration**, so the firmware defines which bytes each
  core owns. Banks 10–12 are **not** shared between PRU1 and RTU1 on silicon.
  open-pru `logic_scope` uses `xout; nop; nop; nop; xin` across PRU0/RTU0. [TRM s6.4.6.2.4,
  Table 6-433; open-pru icss_xfer_defines.inc `PRU_RTU_SPAD_XID`]
- **RTU1:** same ISA as PRU (LOOP, MAC). It reaches MSRAM at 0x7000_0000 and shared RAM
  at 0x10000 locally. Its R30 does not drive pins. Its MSRAM read latency on silicon has
  not been measured. [TRM; open-pru logic_scope, multicore_scheduler readme]
- **Clock:** 333.33 MHz = PLL0_HSDIV9. The R5F SysConfig sets it with
  `pruicss1.coreClk = 333333333`, as `pru_emif` and `spi_loopback` do. All ICSSG0 cores run
  at this clock. [MCU+ SDK pruicss_g_v0.syscfg.js; open-pru examples]
- **MSRAM** from PRU0, measured on the LP: LBBO 4/16/32/64 B = 21/24/28/36 cycles.
  [pru_simulator config/memory_am243x.cfg]
- **Cross-ICSSG access:** a core in ICSSG1 can read ICSSG0 shared RAM at 0x3001_0000 and
  MSRAM at 0x7000_0000. R31 events do not reach the other ICSSG's PRUs. [TRM s6.4.4.4]
- **Simulator:**
  - 3 SD channels.
  - It models the IPC SPAD (XFR 15) as one copy shared by all cores. That is fine while
    only PRU1 and RTU1 use it.
  - Its SD `CLK_SEL` = 0 currently means "no clock from the PdmLink", so the sim needs a
    small change (§10).

## 6. Pins and wiring (LP-AM243 Rev A; schematic PROC109A(001)_SCH sheets 9/11/13/19)

The PRU1 pads use mux mode 0 (GPO); the PRU0 pads use mode 1 (GPI) with the receiver
enabled. **SW6 (BPSERVO_MUX_SEL) must be in default mode (LD17 off)**, because the
SD5/6/7 data and PRU1_GPO16 paths go through the TMUX1574s. No BoosterPack may be plugged in.

| Channel | PRU1 output (pin) | → | PRU0 input (pin) | SD |
|---|---|---|---|---|
| clock | GPO0 (BP.11) | → | GPI16 (BP.7) | SD8_CLK, shared |
| 0 | GPO1 (BP.67) | → | GPI1 (BP.32) | SD0_D |
| 1 | GPO2 (BP.68) | → | GPI3 (BP.19) | SD1_D |
| 2 | GPO6 (BP.69) | → | GPI5 (BP.13) | SD2_D |
| 3 | GPO11 (BP.70) | → | GPI7 (BP.44) | SD3_D |
| 4 | GPO12 (BP.72) | → | GPI11 (BP.43) | SD5_D (SD4 is skipped: its GPI9 goes to the RGMII PHY) |
| 5 | GPO13 (BP.71) | → | GPI13 (BP.15) | SD6_D |
| 6 | GPO15 (BP.59) | → | GPI15 (BP.12) | SD7_D |
| 7 | GPO16 (BP.57) | → | GPI17 (BP.18) | SD8_D |

Ground reference: BP.20 / BP.22 / BP.60 / BP.62. Eight channels is the hard maximum,
because only 9 PRU1 GPOs reach the headers. Do not enable PRU0 SD_CLKOUT or shift-out,
because the TRM routes the PRU0 SD clock onto PRU1_GPO1 in that mode.

## 7. Cores and timing at 333.33 MHz

One PDM period is 333.33 MHz / 3.072 MHz = **108 + 73/144 cycles**: 71 short (108) and
73 long (109) periods per 144, using an integer fractional adder as in `pdm_gen.asm`. One
SINC3 window is 96 SD clocks = **10 416.7 cycles**.

**PRU1, the clock and pin driver. It is cycle-exact; every path has a fixed length.**
- Each half period it drives the clock edge and the 8 data bits in **one R30 write**. The
  8 data pins are GPO bits 1, 2, 6, 11, 12, 13, 15, 16. The byte → pin-word mapping uses
  a 256-entry lookup table in PRU1 DRAM (one LBCO per period), or another fixed-cost
  method; the builder chooses.
- All 8 channels are sampled at the **same clock edge**, since all SD channels share
  sd8_clk with CLK_INV = 0. So data changes once per period, in the half opposite to the
  sampling edge, with a setup margin of at least 26 cycles (§12).
- Every 28 periods it takes the next batch from the IPC SPAD (§8).
- Budget estimate: under 30 busy cycles per period. The rest is LOOP/NOP padding.
- **The timed path touches no shared memory (my call, from the builder check).** PRU1 and
  RTU1 share DRAM1, and PRU0 writes shared RAM, so a lookup table or header store in the
  timed path could stall by a variable amount. So the byte → pin mapping is done in
  registers with fixed-cost instructions. Header stores (underrun count, batch counter)
  either happen only in a fixed slot padded for the worst-case stall, or move to RTU1.

**RTU1, the modulators. Not cycle-exact; it runs ahead of real time.**
- 8 independent 2nd-order ΣΔ modulators, the same integer algorithm as
  `pru_simulator/source/pdm/reference.py::sigma_delta2`: FS = 32768, input held for 192
  periods per 16 kHz sample (ZOH).
- One frame of 8 × int16 is fetched from MSRAM every 192 periods (16 B LBBO, ~24 cycles).
  The clips play in a **loop** (§9).
- Estimate: about 9 cycles per channel-period, so about 72 cycles per period, plus packing
  and hand-off. For 28 periods that is about 2 100 cycles against PRU1's 3 038, i.e.
  ~30 % headroom. The builder measures it in the simulator and reports it.

**PRU0, the receiver.**
- Port `pru_simulator/source/pdm/pdm_rx.asm` with `N_CH` = 8 and a per-channel **SD
  channel map**: {0,1,2,3,5,6,7,8} on hardware and {0,1,2} in the simulator.
- `CLK_SEL` = 0 and `CLK_INV` = 0 on all channels.
- Keep the round-robin and the even/odd FIR stagger. Known effect, already documented:
  odd channels' PCM is 1/32 kHz later. It is irrelevant here, but a note for the later
  mic-array project.
- Budget: ~4 300 of 10 417 cycles worst-case window (~42 %), extrapolated from the 2-ch
  measurement. Re-measure it in the simulator at 3 channels.
  *Refined in implementation (Task 4, final review fix M4):* measured at 3 channels,
  w_comb 49 (48 + the R31[29] test of `SD_OVF_COUNT`), w_fir 920, spin 1, wrap 3 cycles,
  so 8 channels = 8·49 + 4·920 + 7·1 + 3 = **4 082 cycles (39 %)**; pessimistic (every FIR
  input negative, shadows one PDM period apart) 4 447 (43 %).
  `HALF_SEQ` is published by whichever channel is the N_CH-th to write frame 63 of the
  half (a per-half done counter in PRU0 DRAM0 0x7F8), so the rule holds for 3 and 8
  channels: with the stagger the last one is the last odd channel, not channel N_CH−1.

**Budget checkpoint and fallback (decided by the user).** Before the PRU0, R5F and board
work starts, the builder measures RTU1's cycles per 28-period batch at 8 channels in the
simulator. Because MSRAM reads aren't modelled for RTU1, the measurement gets a margin
of at least 25 %.
- If it doesn't fit, **move modulator work to PRU1 first**. PRU1 has about 75 spare
  cycles per period, so for example 4 channels on PRU1 and 4 on RTU1, or more on PRU1
  with RTU1 only fetching frames. PRU1's added modulators must have fixed cycle counts.
- **Only if that still doesn't fit**, make the channel count a build option and ship the
  largest count that fits.
- The same order applies if PRU0 overflows on the board at 8 channels: first rebalance,
  then reduce the channel count. *Refined in implementation (final review fix M4):* the
  header counter `SD_OVF_COUNT` (+0x30) shows it. PRU0 tests R31[29]
  (shadow_update_flag_ovf) at every spin exit, before it clears the flag, and counts each
  lost SD sample; the count is sticky, so one read on the board covers the whole run.
- Record the result and the choice in the porting report.

## 8. IPC scratchpad hand-off (RTU1 → PRU1)

The builder may refine this protocol, but must keep its properties: no torn reads,
underruns detected, and PRU1 timing independent of RTU1.

- The 32 bytes are **R2..R9**.
  - **R2..R8** (28 B) belong to RTU1: one byte per PDM period, bit k = channel k's data
    bit, for 28 periods.
  - **R9.w0** belongs to RTU1: the batch sequence number.
  - **R9.b3** belongs to PRU1: the acknowledged sequence number.
  - Each core only XOUTs its own bytes: RTU1 writes R2..R9.w0 (30 B), PRU1 writes R9.b3 (1 B).
- RTU1 computes batch n+1 while PRU1 plays batch n from its own registers. When it is
  done, RTU1 waits until `ack == n`, then XOUTs batch n+1 with `seq = n+1`.
- At the end of a batch, PRU1 XINs R2..R9.w0. If `seq` is the expected value, it copies
  the batch and XOUTs `ack = seq`. Otherwise it sets the **underrun flag** and count in the
  header and replays silence bits for that batch, keeping its timing fixed. The silence
  pattern is the alternating 0/1 pattern a ΣΔ modulator produces for x = 0.
- At startup PRU1 waits for batch 0 before the first clock edge. The IPC SPAD keeps its
  contents across reruns, so both cores clear their seq/ack bytes at start: RTU1 first,
  then PRU1 waits for a fresh seq 0.
- The XOUT → XIN visibility delay between cores is a few cycles (logic_scope uses 3 NOPs).
  The builder reads the TRM and the logic_scope example, and documents the assumption.

**Refined in implementation (Task 3).** The authoritative text is in
`firmware/pdm8_shared.inc`.
- **Start: nonce and echo.** Each core clears only its own bytes, so a stale ack from an
  earlier run could release batch 0 too early. So:
  - RTU1 reads the stale ack e and XOUTs seq 0 with a start nonce c in R2.b0. c is 2,
    or 3 when e = 2, so it is never 0, 1 or e.
  - RTU1 publishes batch 0 (seq 1) only once R9.b3 == c.
  - PRU1 clears R9.b2/b3. While seq == 0 it echoes R2.b0 into R9.b3.
  - PRU1 takes seq 1 only after it has seen seq 0, so a stale seq 1 is never played.

  The R5F halts and restarts **both** cores, RTU1 first. Restarting one core alone does
  not recover.
- **R9.b2 belongs to PRU1:** its underrun count, mod 256. PRU1 touches no memory, so it
  writes no header field.
- **RTU1 writes UNDERRUN_COUNT and BATCH_COUNT** after each batch it publishes:
  - UNDERRUN_COUNT accumulates the deltas of R9.b2;
  - BATCH_COUNT is the number of batches PRU1 acked;
  - both lag by up to one batch.

**Refined in implementation (final review fix I1): lost scratchpad writes.** With no
hardware arbitration, a same-cycle XOUT by RTU1 and PRU1 may lose one of the two writes
(the TRM does not say they are merged). At the start, PRU1 used to echo every 6 cycles, so
about one start in six could lose RTU1's nonce and hang; in the batch loop a late RTU1
publish can meet PRU1's underrun XOUT. Now every write is repeated until it is there:
- RTU1 XINs its seq (at the start also the nonce) back after each XOUT and writes again
  while it differs. The retry is unbounded: RTU1 alone writes bytes 0..29, so the first
  XOUT that no PRU1 XOUT hits ends it.
- PRU1's start loop XINs all 32 bytes and XOUTs R9.b2/b3 only when they are not
  (0, nonce): once per new nonce, again if lost. The loop is not timed (clock idle), but
  every pass is 9 cycles, so the first edge comes at the same time after a clean or a
  stale start.
- Each batch slot XOUTs both R9.b2 and R9.b3 (same cycle count as before), so the next
  slot repeats a lost one; a lost ack costs one silence batch, counted.
pru-simulator drops single XOUTs to check it (`test_lost_spad_write_at_start_recovers`,
`test_lost_spad_write_on_underrun_recovers`). RTU1's worst batch grew by 2 cycles (1329).

## 9. Memory map

**ICSS shared RAM (0x10000 locally, 0x3001_0000 globally), 64 KB.**

| Offset | Size | Contents | Writer |
|---|---|---|---|
| 0x0000 | 256 B | Header (below) | R5F / cores |
| 0x0100 | 2 KB | Half A: 64 frames × 8 ch × int32 | PRU0 |
| 0x0900 | 2 KB | Half B | PRU0 |

Frame f of half h, channel c, is at `0x0100 + h·0x800 + f·32 + c·4`. That is the sample-
interleaved order the reader consumes: s0·ch0..ch7, s1·ch0..ch7, …

The PCM is signed 24-bit in int32 (±(2^23−1)), 16 kHz.

Header contents (exact offsets are the builder's, kept in one `.inc` plus a C header):
- magic/version, N_CH = 8, frames per half = 64, PCM rate = 16000 (refined in
  implementation, final review fix I3: `VERSION` = 1 at +0x2C, written by the R5F; a reader
  checks MAGIC and VERSION before it uses anything else);
- MSRAM clip buffer address, clip length in frames;
- `HALF_SEQ`: a u32 counting completed halves. A half is ready when `HALF_SEQ` increments.
  The ready half is `(HALF_SEQ − 1) & 1`; `HALF_SEQ` is written after the half's last sample;
- PRU1 counters (written by RTU1 for PRU1; refined in implementation, Task 3): underrun
  count and batch counter. There is no run flag: a rising BATCH_COUNT shows that RTU1 and
  PRU1 are live. A frozen BATCH_COUNT means RTU1 has stalled, or PRU1 is halted or never
  started, or their start handshake did not finish: RTU1 writes BATCH_COUNT only after
  PRU1 acked a batch, so these look the same (UNDERRUN_COUNT does not tell them apart);
- RTU1: frame position, loop count;
- PRU0: per-channel sample counts (diagnostic), and `SD_OVF_COUNT` at +0x30 (final
  review fix M4: SD samples lost because PRU0 was late, sticky; §7).

An R31 event per completed half (to the R5F) is optional, and the builder may add it. The
Ethernet PRU will poll `HALF_SEQ`.

**MSRAM clip buffer.** The R5F builds **one frame-interleaved buffer**: 8 × int16 per frame,
16 B per frame, 0.5 s = 8 000 frames = **128 KB**. Shorter clips loop within the buffer.
RTU1 wraps at the end. The buffer goes in an uncached MSRAM region defined in the R5F
SysConfig. The R5F writes it before starting the cores, so cache coherence is not an issue
after start. The builder confirms the region's MPU attributes.

*Refined in implementation (Task 6):* the region is `PDM_CLIPS`, 0x700C_0000..0x700D_FFFF
(128 KB, right after the R5F's `MSRAM` region, clear of the shared regions at 0x701D_0000 and
the SBL's MSRAM; the template's unused `NON_CACHE_MEM` region, which had no MPU entry, is
dropped). `pdm_clip_buffer` (section `.pdm_clips`, 128 000 B) is
linked there and loaded with the ELF, so the R5F writes no clip data at run time. MPU region
`CONFIG_MPU_REGION5`: 128 KB, Normal non-cacheable (TEX 001, C 0, B 0), shareable, no execute,
supervisor RW / user RO; it overrides the cached 2 MB MSRAM region.

**Clips:** the 5 shipped clips (`speech_librivox`, two chirps, 1 kHz and 3 kHz tones), plus
3 extra tones (e.g. 500 Hz, 2 kHz, 5 kHz) generated with `make_clips.py`. Normalised to
0.5 FS like the simulator loader. They are converted at build time to a C array, or
another reproducible form, with a generator script in the project. Each channel gets a
different clip, so crosstalk and channel swaps are visible in the memory browser.

## 10. Simulator validation (up to 3 channels)

- **Small pru-simulator change** on its own branch/PR, **stacked on PR #106** because
  PdmLink exists only there: let the PdmLink clock drive the SD `CLK_SEL = 0`
  (sd8_clk/GPI16) path, which today gets no clock. It is covered by tests like
  `tests/test_sd_pin_input.py`. No 9-channel expansion.
- **open-pru ships only the 8-channel firmware (decided by the user).** There is no
  3-channel version in the open-pru project. The simulator harness assembles the same
  `pdm_rx.asm` source with its `N_CH` and SD-map build options overridden at load time
  to 3 channels {0,1,2}, the way audio_pipe's tests set build options. The 8-channel
  build would otherwise spin forever on the SD channels the simulator lacks. So the
  firmware keeps `N_CH` and the SD map as plain-literal build options, and the rule for
  which channel publishes `HALF_SEQ` must hold for both 3 and 8 channels (FIR stagger).
- In the simulator, the test harness runs RTU1 + PRU1 + PRU0 together in lockstep, with
  PRU1 as the lead. This is the loop with feedback, so run-ahead doesn't apply.
- Bit-exact checks with the `source/pdm/reference.py` functions:
  - the PRU1 data bits per channel equal `sigma_delta2(clip)`;
  - the clock edges follow the 333 MHz fractional schedule exactly;
  - the PRU0 interleaved PCM equals `pcm_chain_for_channel(sampled_bits, ch)`;
  - the ping-pong halves and `HALF_SEQ` are published only when complete;
  - an injected RTU1 delay produces an underrun flag, not a corrupted clock.
- **Where the harness lives:** in **pru-simulator**, not in open-pru. It goes in
  `tests/test_pdm_open_pru_port.py` on the simulator-change branch. It finds the open-pru
  checkout through an `OPEN_PRU_PATH` environment variable (default `~/GitHub/open-pru`)
  and skips if the checkout is missing. The open-pru readme links to it under
  "Verification".
- This is a validation run, not a demo, so simulator speed doesn't matter. Keep runs to a
  few ms of audio.

## 11. R5F application

Copy the uart_rx/uart_print project structure (mcuplus, FreeRTOS, SysConfig):
- `coreClk = 333333333`;
- pinmux for the 18 pads in §6;
- GPCFG0 = SD mode, GPCFG1 = GP;
- load PRU0, PRU1 and RTU1 firmware;
- write the clip buffer and the header;
- start RTU1, then PRU1, then PRU0, in an order that guarantees PRU1 sees batch 0 and
  PRU0 sees a running clock;
- idle.

*Refined in implementation (Task 6),* `mcuplus/pdm_audio_example.c`: halt RTU1, PRU1 and PRU0
(final review fix M3: also RTU0, TX_PRU0 and TX_PRU1, whose DRAM0 share PRU0 uses);
write GPCFG0 (PRU0 mux SD, every other field at reset), GPCFG1 (all reset: GP mux, parallel
GPO) and PRU0_SD_CFG (SHARE_EN, MAN_SD_EN, MAN_DATA_NV_EN, CH_SEL = 0) as whole registers, PRU0
CTBIR0 C24 = 0; zero the 64 KB shared RAM and both DRAMs; write MAGIC .. CLIP_FRAMES and VERSION; load the
three IRAMs with the cores halted; set CTRL PCTR_RST_VAL = 0 on all three, reset and enable
RTU1, PRU1, PRU0 in that order; idle. Nothing depends on what an earlier demo left in these
registers (a CCS reload without System Reset). Both RTU1 and PRU1 always restart together.
`mcuplus/pdm8_shared.h` mirrors `pdm8_shared.inc`; the pru-simulator test
`test_c_header_matches_inc` checks it.

Follow the existing ports for the R5F loader of PRU firmware arrays (`*_load_bin.h`), and
the open-pru make and CCS projectspec conventions (examples/makefile and readme entries).
The other electronica2026 ports are not on open-pru `origin/main` yet (PRs #188/#189/#190).
So copy the project structure from their branches, and expect `examples/makefile`/readme
conflicts with whichever merges first.

## 12. Board check (CCS memory browser)

The readme gives step-by-step instructions:
1. **HALF_SEQ** increases about 250 times per second (16 000 / 64).
2. In a half, each channel's column (stride 32 B) shows a clean waveform of its clip.
   Tone channels show the expected period: a 1 kHz tone repeats every 16 frames.
3. Underrun count stays 0.
4. BATCH_COUNT keeps increasing, about 110 000 per second (refined in implementation, Task 3).
5. SD_OVF_COUNT stays 0 (refined in implementation, final review fix M4).
6. Expected level: about ±(2^22) for 0.5 FS clips.

Optionally, a CCS memory-graph setup shows one channel's column as a waveform (stride 32 B,
int32).

## 13. Decided by the user

- If 8 channels don't fit: move modulator work to PRU1 first, reduce the channel count only after that (§7).

- Target open-pru and the AM243x LP, like the other demos.
- **One PDM output per channel** (no L/R pairing).
- **Up to 8 channels.** Max clock **333 MHz**.
- **RTU1 may be used.** RTU1 cannot drive pins; it loads from RAM and shares through the
  scratchpad (IPC_SP).
- Use MSRAM if shared RAM is too small (it is too small for the clips).
- **Single shared SD clock** for all channels.
- PCM in **ping-pong halves of 64 samples per channel**, all channels arranged so the
  Ethernet PRU reads a half in sequence, then switches to the other half.
- No PicoScope. No R5F console check: the board check is the **CCS memory browser**.
- **The simulator stays at 3 SD channels.**
- Clips come from MSRAM, written by the R5F at start-up.

## 14. Calls made by the designer (check these)

- RTU1 runs all 8 modulators. PRU1 only drives pins, with a cycle-exact clock.
- The IPC SPAD protocol of §8: 28-period batches, seq/ack bytes, silence + flag on underrun.
- Sample-interleaved halves in ICSS shared RAM, with a `HALF_SEQ` counter as the ready
  signal. The event is optional.
- One frame-interleaved 128 KB clip buffer in MSRAM, looping. 8 different clips (3 new
  tones).
- Logical channel 4..7 → SD5..SD8 (SD4 skipped), with the pin table of §6.
- Simulator change: CLK_SEL = 0 driven by the PdmLink clock.
- The design doc and the porting report live in the open-pru project's `docs/`, on branch
  `dev_tley_electronica2026_pdm_audio` from `origin/main`.
- PRU1's timed path uses no shared memory: register-only pin mapping, and header stores
  in a padded slot or on RTU1.
- The simulator change and the 3-channel simulator harness are both in pru-simulator,
  stacked on PR #106. open-pru carries only the 8-channel firmware.

## 15. Left to the builder

- The exact header offsets.
- The PRU1 byte → pin mapping method.
- Register maps.
- RTU1 loop structure.
- The XFR visibility margin.
- The MSRAM region address.
- The clip conversion tool.
- The CCS memory-graph presets.

Each must be documented where it is decided.
