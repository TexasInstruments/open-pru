# pdm_audio (8-channel PDM on AM243x LP) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port the pru-simulator PDM demo to open-pru as `examples/electronica2026/pdm_audio/pdm_audio`. The port runs 8 channels at 333.33 MHz on ICSSG0, split across three cores:
- RTU1: 8 ΣΔ modulators fed from MSRAM clips;
- PRU1: a cycle-exact PDM clock plus 8 data pins;
- PRU0: 8-channel SD decode into sample-interleaved 64-frame ping-pong halves in ICSS shared RAM.

**Architecture:** RTU1 → PRU1 hand-off uses the IPC scratchpad (XFR 15) in 28-period batches with seq/ack bytes. PRU1 → PRU0 goes over jumper wires: one shared SD clock (sd8_clk/GPI16, `CLK_SEL` = 0) plus SD0-3 and SD5-8 data. The R5F (FreeRTOS) sets the clock, pinmux, GPCFG and the clip buffer, then loads and starts the cores. The firmware is checked bit-exact in pru-simulator at 3 channels by a harness that lives in pru-simulator. The board check is the CCS memory browser.

**Tech Stack:** PRU assembly (clpru 2.3.3, `-v4`), MCU+ SDK 12.01 FreeRTOS + SysConfig, CCS 21.0.1 (headless CLI), open-pru make, Python 3 stdlib (clip tool), pru-simulator (pytest harness).

**Spec:** `examples/electronica2026/pdm_audio/pdm_audio/docs/design.md` (open-pru branch `dev_tley_electronica2026_pdm_audio`). §-numbers below refer to it.

## Global Constraints

- **Two repos.**
  - **open-pru** `~/GitHub/open-pru`, branch `dev_tley_electronica2026_pdm_audio` (exists, holds the spec, upstream unset; push later with `-u origin <branch>`). Commit with `git -c user.name="Thomas Leyrer" -c user.email="t-leyrer@ti.com" commit …`.
  - **pru-simulator** `~/GitHub/pru_simulator`, new branch `feat/pdm-open-pru-check` created from `feat/pdm-audio-demo`; it is stacked on PR #106.
- **Clocks:** ICSSG0 core clock 333 333 333 Hz. PDM clock 3.072 MHz = **108 + 73/144** cycles per period (71 periods of 108, 73 of 109, per 144; integer fractional adder). SD OSR 96, so a SINC3 window is 10 416.67 cycles. PCM is 16 kHz.
- **Pins (§6).** PRU1 pads mode 0 (GPO), PRU0 pads mode 1 (GPI, receiver on):

  | Signal | PRU1 GPO | PRU0 GPI | SD |
  |---|---|---|---|
  | clock | 0 | 16 | SD8_CLK |
  | ch0 | 1 | 1 | SD0_D |
  | ch1 | 2 | 3 | SD1_D |
  | ch2 | 6 | 5 | SD2_D |
  | ch3 | 11 | 7 | SD3_D |
  | ch4 | 12 | 11 | SD5_D |
  | ch5 | 13 | 13 | SD6_D |
  | ch6 | 15 | 15 | SD7_D |
  | ch7 | 16 | 17 | SD8_D |

  GPCFG0 PRU0 mux = 3 (SD); GPCFG1 PRU1 mux = 0 (GP). All channels `CLK_SEL` = 0, `CLK_INV` = 0, ACC3, `SD_SAMPLE_SIZE` = 95.
- **IPC SPAD (XFR ID 15, R2..R9):**
  - R2..R8 = 28 bytes from RTU1, one byte per PDM period, with bit k = channel k;
  - R9.w0 = RTU1's batch seq;
  - R9.b3 = PRU1's ack;
  - each core XOUTs only its own bytes;
  - both cores clear seq/ack at start.
- **PRU1's timed path touches no memory shared with RTU1 or PRU0** (DRAM1, shared RAM). The bit → pin mapping is done in registers. Every path through a half period has a fixed cycle count.
- **ΣΔ algorithm = `pru_simulator/source/pdm/reference.py::sigma_delta2`.** FS = 32768, ZOH 192 periods per 16 kHz sample, fb from the new bit, start fb = −FS. Clips are normalised to peak 16384.
- **PRU0 DSP = `pru_simulator/source/pdm/pdm_rx.asm`.** Comb mod 2^28, Q27 FIR from `fir_coefs.inc`, Q32 DC blocker, sat24, even/odd FIR stagger. Reference: `reference.pcm_chain_for_channel`.
- **Output:**
  - shared RAM 0x10000: header at +0x0000 (256 B), half A at +0x0100, half B at +0x0900;
  - sample (frame f, half h, ch c) at `0x0100 + h*0x800 + f*32 + c*4`, int32 signed 24-bit, 64 frames per half;
  - `HALF_SEQ` (u32) is written after a half's last sample of every channel; the ready half is `(HALF_SEQ−1) & 1`.
- **Clip buffer:** MSRAM, uncached, frame-interleaved 8 × int16 (16 B per frame), 8000 frames (0.5 s, 128 KB), looping. The header holds its global address and length.
- **open-pru ships only the 8-channel firmware.** `N_CH` and the SD map stay plain-literal `.set` build options. The 3-channel simulator variant exists only as load-time overrides in the pru-simulator harness.
- **Simulator stays at 3 SD channels.** Run pytest with `env -u PYTHONPATH .venv/bin/python -m pytest` (in pru_simulator). Never stage pru_simulator `memory.cfg`. Never run `ui/server.py` against the repo `memory.cfg`.
- **Builds:**
  - open-pru make: `make DEVICE=am243x MCU_PLUS_SDK_PATH=$HOME/ti/mcu_plus_sdk_am243x_12_01_00_33 SYSCFG_NODE=$HOME/ti/ccs2101/ccs/tools/node/node`, from the project dir; never at the repo root, because that dirties `pru_i2s`.
  - CCS headless: `~/ti/ccs2101/ccs/eclipse/ccs-server-cli.sh` with a fresh workspace dir per call.
  - clpru: `~/ti/ti-cgt-pru_2.3.3/bin/clpru -v4`.
- The simulator assembler has no `.emsg`: a failing guard shows as "unknown directive", and the load still fails. Keep `.if/.emsg` guards for clpru.
- **Commit trailers:** `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>` and `Claude-Session: https://claude.ai/code/session_01DTi2tBzusmG62Tr8jkRsj9`. No pushes without the user.

## Review Focus

1. **Rerun without a power cycle:** the IPC SPAD and shared RAM keep stale seq/ack and `HALF_SEQ`. Expected: a second start behaves exactly like the first, with no false underrun and no stale half. Test in Task 3 (`test_rerun_clears_handshake`).
2. **RTU1 late, e.g. MSRAM stalls:** expected `UNDERRUN_COUNT`++ and silence bits for that batch, while the PRU1 clock edge times are unchanged. Test in Task 3 (`test_underrun_keeps_clock_and_flags`).
3. **Cores started in a different order** (PRU1 or PRU0 before RTU1): expected PRU1 holds the clock idle until batch 0, and PRU0 produces nothing until the clock runs. Test in Task 3 (`test_pru1_waits_for_first_batch`) and in Task 4 (`test_rx_idles_without_clock`).
4. **Clip wrap inside a batch:** expected the bits to continue the reference across the wrap, which is `sigma_delta2` of the looped clip. Test in Task 2 (`test_clip_wraps_mid_batch`).
5. **A reader polling `HALF_SEQ` mid-write:** expected that the half `HALF_SEQ` points at is fully written for all channels (the odd-channel stagger last), and that the other half is the one being written. Test in Task 4 (`test_half_published_only_when_complete`).

---

### Task 1: Simulator: `CLK_SEL = 0` driven by the PdmLink clock (pru-simulator)

**Files:**
- Branch: `git -C ~/GitHub/pru_simulator switch -c feat/pdm-open-pru-check feat/pdm-audio-demo`
- Modify: `pru_io/sd_filter.py` (`sampling_edge` ~line 157), `simulator.py` (`attach_pdm_link`: add `clock_net: int = 0`, forwarded to `PdmLink`)
- Test: `tests/test_sd_pin_input.py`

**Interfaces:**
- Produces:
  - `sampling_edge`: `CLK_SEL = 0` selects **net 8** (sd8_clk = GPI16 = SD8_CLK, TRM Table 6-418) for any channel.
  - `Simulator.attach_pdm_link(producer="pru1", consumer="pru0", clock_pin=0, data_pins={0: 1, 1: 1}, keep_history=False, clock_net=0) -> PdmLink`. Later tasks call it with `clock_net=8`.

- [ ] **Step 1: Write failing tests** in `tests/test_sd_pin_input.py`:
  - `test_clk_sel_0_follows_the_sd8_clock_net`: ch0, ch1 and ch2 all `CLK_SEL = 0`, pin mode, link `clock_net=8`. Each channel's `acc3` equals the CIC3 integral of the bits at the rising edges; `CLK_INV = 1` on ch1 gives the falling-edge bits.
  - `test_clk_sel_0_ignores_other_nets`: `clock_net=0` with `CLK_SEL = 0` gives no ticks.
  - The existing tests still pass, unchanged.
- [ ] **Step 2: Run, expect FAIL.** `env -u PYTHONPATH .venv/bin/python -m pytest tests/test_sd_pin_input.py -v`
- [ ] **Step 3: Implement.** In `sampling_edge`, `clk_sel == 0 → selected = 8`. Update the docstring. Add the `clock_net` passthrough.
- [ ] **Step 4: Verify.** The same file plus the SD suites (`tests/test_sd_*.py`, `tests/test_pdm_*.py`, `tests/test_idle_skip.py`) all PASS.
- [ ] **Step 5: Commit** in pru_simulator: `feat(sd): CLK_SEL=0 (sd8_clk) follows the PdmLink clock net`.

---

### Task 2: RTU1 modulator firmware + project skeleton (open-pru) + RTU1 harness (pru-simulator)

**Files:**
- Create (open-pru, project root `examples/electronica2026/pdm_audio/pdm_audio/`):
  - `makefile`, copied from uart_print (`git show origin/dev_tley_electronica2026_uart_print:examples/electronica2026/uart_print/uart_print/makefile`), with `PROJECT_NAME := pdm_audio`;
  - `firmware/pdm8_shared.inc`: shared-RAM map, the IPC SPAD layout and the clock constants as plain `.set` literals;
  - `firmware/rtu1_mod.asm`;
  - `firmware/am243x-lp/icss_g0_rtu_pru1_fw/ti-pru-cgt/{makefile,makefile_projectspec,example.projectspec,linker.cmd}`: copy the structure from `examples/multicore_scheduler/firmware/am243x-lp/icss_g0_rtu_pru1_fw/ti-pru-cgt/` and the uart_print PRU0 files, with names swapped.
- Create (pru-simulator): `tests/test_pdm_open_pru_port.py` with `OPEN_PRU = Path(os.environ.get("OPEN_PRU_PATH", "~/GitHub/open-pru")).expanduser()` and a module-level skip when the project dir is missing. It also holds the shared helpers used by Tasks 3 and 4.

**Interfaces:**
- Produces:
  - `pdm8_shared.inc` names, with the `PDM8_` prefix:
    - `SHARED_BASE` 0x10000;
    - `HDR_*` offsets: magic `0x50444D38` 'PDM8', `N_CH`, `FRAMES_PER_HALF` 64, `PCM_RATE`, `CLIP_ADDR` (u32, global), `CLIP_FRAMES`, `HALF_SEQ`, `UNDERRUN_COUNT`, `BATCH_COUNT`, `RTU1_FRAME_POS`, `RTU1_LOOPS`, `PCM_COUNT[8]`;
    - `HALF_A` 0x0100, `HALF_B` 0x0900, `HALF_BYTES` 0x800, `FRAME_BYTES` 32;
    - `SPAD_XID` 15, `BATCH_PERIODS` 28;
    - `CFG_T_SH_CYC` 108, `CFG_FRAC_ACC` 73, `CFG_FRAC_MAX` 144, `PCM_PERIODS` 192.
  - `rtu1_mod.asm`, entry `RTU1_MOD`:
    - At start it clears the IPC SPAD R2..R9.w0 to 0. It reads `CLIP_ADDR`/`CLIP_FRAMES` from the header and fetches 16-byte frames from MSRAM by LBBO.
    - It runs 8 modulators, packs 28 period-bytes into R2..R8, waits for `ack == seq` (seq starts at 0 = "batch 0 pending"; see Step 3), then XOUTs R2..R9.w0 with `seq + 1`.
    - It updates `RTU1_FRAME_POS`/`RTU1_LOOPS`, and loops the clip forever.
  - Harness helpers in `test_pdm_open_pru_port.py`:
    - `load_fw(sim, core, name, overrides=None)` assembles open-pru firmware with `.set` overrides, like audio_pipe's `set_option`;
    - `write_header(sim.memory, clip_frames, clip_addr=0x7000_0000)`;
    - `write_clip_buffer(sim.memory, frames: list[list[int]])`;
    - `spad_batches(sim, rtu1_only=True)`: a fake PRU1 that acks;
    - `ref_bits(clips, n_periods)` → per channel `sigma_delta2`.

- [ ] **Step 1: Write failing tests** (pru-simulator, `tests/test_pdm_open_pru_port.py`), at 333.333 MHz via the `sim_config` fixture (`pru_clock_mhz=333.333, pru1_clock_mhz=333.333`), RTU1 only, with a Python stand-in acking every batch:
  - `test_rtu1_batches_match_reference`: 8 different tone clips, 20 batches. Bit k of byte j of batch n equals `sigma_delta2(clip_k)[28n + j]`, for all 8 channels.
  - `test_clip_wraps_mid_batch`: `CLIP_FRAMES` = 3. The bits across the wrap equal the reference of the looped clip, and `RTU1_LOOPS` increments.
  - `test_rtu1_waits_for_ack`: no ack means RTU1 never XOUTs a seq beyond the first.
  - `test_rtu1_budget_checkpoint`: cycles per batch at 8 channels, measured between XOUTs with the stand-in acking at once. **Assert ≤ 2278**, which is 75 % of 28 × 108.5. Print the number.
    - **If the assertion fails, do not continue to Task 3. Apply the spec's fallback (§7):** move modulator work to PRU1 first, e.g. 4 channels on each core, by adding `N_RTU_CH` / `N_PRU1_CH` build options. Re-plan Task 3 with the controller.
- [ ] **Step 2: Run, expect FAIL** (missing firmware). `env -u PYTHONPATH .venv/bin/python -m pytest tests/test_pdm_open_pru_port.py -v`
- [ ] **Step 3: Implement.**
  - First write `pdm8_shared.inc`, then `rtu1_mod.asm`.
  - Registers: 8 × (i1, i2) = 16 registers, plus x for 8 channels, gets tight. Hold x as 4 registers of 2 × int16 and sign-extend into a scratch register per step, or spill to RTU1 DMEM. Document the register map and cycle count in the file header.
  - Handshake: RTU1 publishes batch 0 with seq = 1, then waits for ack == 1 before publishing seq = 2, and so on. PRU1 acks with the seq it consumed.
  - Build the project skeleton far enough that clpru assembles `rtu1_mod.asm`.
- [ ] **Step 4: Verify.**
  - Tests PASS.
  - `~/ti/ti-cgt-pru_2.3.3/bin/clpru -v4 -I firmware -I ~/ti/ti-cgt-pru_2.3.3/include firmware/rtu1_mod.asm` (in a scratch dir) gives 0 errors.
  - Record the budget number in the task report.
- [ ] **Step 5: Commit both repos.** open-pru: `feat(electronica2026): pdm_audio RTU1 8-channel sigma-delta modulator`. pru-simulator: `test(pdm): open-pru pdm_audio RTU1 harness`.

---

### Task 3: PRU1 clock and pin driver (open-pru) + RTU1+PRU1 harness

**Files:**
- Create (open-pru): `firmware/pru1_pdm.asm`, `firmware/am243x-lp/icss_g0_pru1_fw/ti-pru-cgt/*` (same pattern as Task 2).
- Modify (pru-simulator): `tests/test_pdm_open_pru_port.py`.

**Interfaces:**
- Consumes: Task 2's `pdm8_shared.inc`, `rtu1_mod.asm` and helpers.
- Produces: `pru1_pdm.asm`, entry `PRU1_PDM`:
  - At start it clears R9.b3 and waits for batch seq == 1 before the first clock edge.
  - Each period: the clock edges on GPO0, and the data bits for the 8 channels on GPO 1, 2, 6, 11, 12, 13, 15, 16 in **one R30 write per half period**. Data changes in the half after the rising edge (samples at the rising edge, setup ≥ 26 cycles).
  - Every 28 periods, in a fixed slot: XIN R2..R9.w0. If seq == expected, copy it to the play registers and XOUT ack. Otherwise play the silence pattern, add 1 to `UNDERRUN_COUNT` in a padded slot, and do not ack.
  - `BATCH_COUNT` is updated in a padded slot or by RTU1.

- [ ] **Step 1: Write failing tests.** RTU1 + PRU1 lockstep (`step_paced_many("pru1", ["rtu1"], …)`), 333.333 MHz, `PdmLink(keep_history=True)` from PRU1 GPO to an idle PRU0 with all SD channels `"off"`, or a `gpo_listeners` probe:
  - `test_clock_follows_333mhz_schedule`: the clock edge cycles over 3 × 144 periods follow the 108/109 fractional schedule exactly. Write a `pdm_clock_edges`-style reference for 108 + 73/144 in the harness.
  - `test_pins_carry_reference_bits`: for each of the 8 channels, the data pin level at each rising edge equals `ref_bits(...)[k]`, for 10 batches.
  - `test_setup_margin`: the data change is ≥ 26 cycles before every rising edge.
  - `test_pru1_waits_for_first_batch`: start PRU1 alone for 50 000 cycles. No clock edge occurs. Then start RTU1, and edges begin.
  - `test_underrun_keeps_clock_and_flags`: stall RTU1 (e.g. stop stepping it for 5 batches). The clock edges stay on schedule, `UNDERRUN_COUNT` > 0, the silence pattern is output, and the run then recovers.
  - `test_rerun_clears_handshake`: run, stop, reload both firmwares without clearing the SPAD/shared RAM, and run again. The second run is identical to the first and `UNDERRUN_COUNT` = 0.
  - `test_pru1_timed_path_has_no_shared_memory_access`: wrap `sim.memory.read/write` while PRU1 runs, and assert PRU1 makes no access outside its padded slots. Record (pc, cycle) and check them against the slot labels.
- [ ] **Step 2: Run, expect FAIL.**
- [ ] **Step 3: Implement** `pru1_pdm.asm`. Reuse the clock and fractional-adder structure of `pru_simulator/source/pdm/pdm_gen.asm`. The pin mapping is register-only, for example 8 `qbbc`/`set` pairs balanced to a fixed cost, or a shift/or sequence. Put the budget table in the header.
- [ ] **Step 4: Verify.** Tests PASS, and clpru gives 0 errors.
- [ ] **Step 5: Commit both repos.**

---

### Task 4: PRU0 8-channel receiver with interleaved ping-pong (open-pru) + 3-channel full-pipeline harness

**Files:**
- Create (open-pru):
  - `firmware/pru0_rx.asm`, ported from `pru_simulator/source/pdm/pdm_rx.asm`;
  - `firmware/fir_coefs.inc`, `firmware/decimation_fir.h`, `tools/gen_fir_coefs.py`, copied from `pru_simulator/source/pdm/`, with paths fixed;
  - `firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/*`.
- Modify (pru-simulator): `tests/test_pdm_open_pru_port.py`.

**Interfaces:**
- Consumes: Task 3 (the full generator pair), `reference.pcm_chain_for_channel`, `pdm_link.sampled_bits`.
- Produces: `pru0_rx.asm`, entry `PRU0_RX`:
  - `N_CH .set 8` and SD map literals `SD_CH_k` = 0,1,2,3,5,6,7,8, with `.emsg` guards.
  - All channels `CLK_SEL` = 0.
  - It writes sample n of channel c to `half = (n // 64) & 1`, `frame = n % 64`, then `PCM_COUNT[c]`.
  - It writes `HALF_SEQ` once all channels have written frame 63 of that half. Track completion with a per-half channel mask or the minimum count, so it is correct for the stagger and for N_CH = 3 and 8.

- [ ] **Step 1: Write failing tests** (all three cores in lockstep; PRU0 loaded with overrides `N_CH=3` and SD map {0,1,2}; link `clock_net=8`, data pins {0: GPO1, 1: GPO2, 2: GPO6}; at 333.333 MHz):
  - `test_fir_coefs_regenerate_identically`.
  - `test_interleaved_pcm_matches_reference`: ≥ 192 frames, i.e. three halves. For each channel c in 0..2, the samples read from the halves in order equal `pcm_chain_for_channel(link.sampled_bits[c], c)`.
  - `test_half_published_only_when_complete`: stop PRU0 at each `HALF_SEQ` store. At that instant all 3 channels' frame 63 of the published half equal the reference, and the other half's frames are not yet complete.
  - `test_half_seq_rate`: `HALF_SEQ` increases once per 64 PCM samples, i.e. once per 4 ms of simulated time.
  - `test_rx_idles_without_clock`: PRU0 alone for 1 M cycles leaves `PCM_COUNT` all 0, `HALF_SEQ` 0, and no fault.
  - `test_worst_window_budget_8ch`: measure w_comb and w_fir at 3 channels and assert the 8-channel extrapolation `8*w_comb + 4*w_fir + 7*spin + overhead ≤ 10 417`. Print it.
  - `test_8ch_build_assembles`: the unmodified 8-channel `pru0_rx.asm` loads in the simulator (errors == []) and assembles with clpru (skip if clpru is missing).
- [ ] **Step 2: Run, expect FAIL.**
- [ ] **Step 3: Implement** `pru0_rx.asm`. Keep the audio_pipe DSP blocks byte-for-byte, and change only the I/O, state indexing and publish logic. Put the register, memory and budget maps in the header.
- [ ] **Step 4: Verify.** Tests PASS, clpru gives 0 errors, and `fir_coefs.inc` is byte-identical.
- [ ] **Step 5: Commit both repos.**

---

### Task 5: Clip buffer tool and clips (open-pru)

**Files:**
- Create (open-pru):
  - `tools/make_clip_buffer.py` (stdlib): it reads 8 WAVs, loads them like `pru_simulator/source/pdm/pdm_layout.load_clip` (16 kHz, 16-bit, normalised to peak 16384), loops shorter clips to 8000 frames, and writes `mcuplus/pdm_clips.h` (`const int16_t pdm_clip_buffer[8000*8]` frame-interleaved, `#define PDM_CLIP_FRAMES 8000`);
  - `clips/*.wav`: the 5 shipped clips copied from `pru_simulator/source/pdm/clips/` plus `tone_500.wav`, `tone_2000.wav` and `tone_5000.wav`, generated by `tools/make_clip_buffer.py --make-tones`;
  - `clips/SOURCES.md`, copied and extended;
  - `mcuplus/pdm_clips.h` (generated, committed).
- Test: a self-test inside the tool (`--check`) plus a pytest in pru-simulator, `test_clip_buffer_header_matches_loader`, which compares the header to `pdm_layout.load_clip`.

**Interfaces:**
- Produces: `pdm_clips.h` and the channel order: ch0 speech, ch1 chirp_lin, ch2 chirp_log, ch3 1 kHz, ch4 3 kHz, ch5 500 Hz, ch6 2 kHz, ch7 5 kHz. Write this into the readme too.

- [ ] **Step 1:** Write the pytest. It parses `pdm_clips.h` and expects frame 0..7999 × ch 0..7 to equal the normalised, looped clips. Run it and expect FAIL.
- [ ] **Step 2:** Implement the tool, generate the tones and the header, and run the test: PASS.
- [ ] **Step 3: Commit both repos.**

---

### Task 6: R5F application, SysConfig, builds (open-pru)

**Files:**
- Create (open-pru) `mcuplus/` by copying the uart_print branch's `mcuplus/` tree (`example.syscfg`, `main.c`, `ti-arm-clang/*`) with names swapped. Then:
  - `mcuplus/pdm_audio_example.c`: start-up sequence;
  - `mcuplus/pdm8_shared.h`: C mirror of `pdm8_shared.inc`, checked by a test.
- Modify: `example.syscfg`:
  - `pruicss1.coreClk = 333333333`;
  - pinmux for the 18 pads with the modes in Global Constraints;
  - an uncached MSRAM memory region `PDM_CLIPS` of 128 KB, placed where it doesn't overlap the existing MSRAM, NON_CACHE_MEM, USER_SHM_MEM, LOG_SHM_MEM and vring regions; the MPU entry is non-cacheable;
  - linker placement of `pdm_clip_buffer` there.

**Interfaces:**
- Consumes: Task 2–5 firmware names and `pdm_clips.h`.
- Produces: the start-up order, documented in the code:
  1. halt all three cores;
  2. set GPCFG0 to SD mode and GPCFG1 to GP;
  3. clear the header and the IPC SPAD state (through the cores' first instructions; the R5F zeroes the header);
  4. write `CLIP_ADDR` (the global address of `pdm_clip_buffer`) and `CLIP_FRAMES`;
  5. load the firmware arrays;
  6. start RTU1, then PRU1, then PRU0;
  7. the R5F loops idle.

  The PRU firmware headers come from the make flow (`*_load_bin.h`), as in the other ports.

- [ ] **Step 1:** pytest in pru-simulator, `test_c_header_matches_inc`: it parses `pdm8_shared.h` and `pdm8_shared.inc` and expects every `PDM8_*` value to match. Run it: FAIL.
- [ ] **Step 2:** Implement. Build with the open-pru make command (Global Constraints) from the project dir: expect 0 errors. Build with CCS headless: `projectCreate` for all 4 projectspecs (3 PRU + R5F) in a fresh workspace, then `projectBuild` Release: expect 0 errors. Record the warnings. The inherited SysConfig part-number warning is OK.
- [ ] **Step 3:** Run the test: PASS.
- [ ] **Step 4:** Clean up build side effects (`git status` shows only intended files; see the open-pru-linux-build notes), then commit.

---

### Task 7: Readme, porting report, examples index (open-pru)

**Files:**
- Create: `readme.md`, `docs/porting_report.md`. Follow the uart_print report's sections: prompt, model/effort/tokens/time, decisions, verification, open items.
- Modify: `examples/makefile`, `examples/readme.md`. Add the entry. A conflict with PRs #188/#189/#190 is expected; note it in the report.

- [ ] **Step 1:** Write the readme:
  - the wiring table (§6);
  - SW6 default and LD17 off; no BoosterPack;
  - the build and run steps (make, CCS import);
  - the CCS memory-browser check (§12): `HALF_SEQ` ≈ 250/s, per-channel columns at a 32-byte stride, the expected periods (1 kHz = 16 frames, 3 kHz ≈ 5.33, 500 Hz = 32, 2 kHz = 8, 5 kHz = 3.2), `UNDERRUN_COUNT` = 0, level about ±2^22;
  - the memory-graph setup;
  - the Ethernet-reader interface (§9 layout and the `HALF_SEQ` rule);
  - the Verification section linking the pru-simulator harness, `OPEN_PRU_PATH`, the test names and the measured budgets;
  - the open items, including `FIXME` hardware-run placeholders as in the earlier ports.
- [ ] **Step 2:** Write the porting report, with the measured numbers from Tasks 2–4 and a timing summary.
- [ ] **Step 3:** Final verification:
  - pru-simulator: `env -u PYTHONPATH .venv/bin/python -m pytest tests -q`. No failures beyond those on `feat/pdm-audio-demo`: the 3 clock/AM263x tests.
  - node UI tests unchanged.
  - open-pru make and CCS builds: 0 errors.
  - clpru: all three firmwares, 0 errors.
- [ ] **Step 4:** Commit. Do not push; the controller asks the user.
