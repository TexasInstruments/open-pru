# Porting report: pru-simulator PDM demo -> 8-channel PDM audio on AM243x LP

Date: 2026-11-03 / 2026-11-04
Branch: `dev_tley_electronica2026_pdm_audio` (open-pru), from `origin/main`
Source: `pru_simulator/source/pdm/` (`pdm_gen.asm`, `pdm_rx.asm`, `reference.py`), pru-simulator PR #106
Target: `examples/electronica2026/pdm_audio/pdm_audio` (AM243x LaunchPad, ICSSG0
RTU1 + PRU1 + PRU0 at 333.33 MHz, R5F r5fss0-0 FreeRTOS host, CCS)
Simulator side: pru-simulator branch `feat/pdm-open-pru-check`, stacked on PR #106

## 1. Prompt

The port was done by an AI agent (Claude Code) from this single request:

> port to open-pru like the other demos. use one pdm output per channel and check whether we can go up to 8 channels and max 333 MHz clock. you can also use RTU1 and IPC_SP. If shared ram is too small then use MSRAM.

The agent took "like the other demos" to mean the same flow and deliverables as
the i2c_io (PR #188), uart_rx (PR #189) and uart_print (PR #190) ports: a new
dev branch from `origin/main`, AM243x LP only, an R5F loader, a readme and this
report. Unlike those ports, this one needed a design first (`design.md`), then a
plan (`plan.md`), then an implementation in seven tasks. The user answered a set
of design questions (section 4); there was no other human input during the
implementation.

## 2. Model, effort, tokens and time

| Item                    | Value |
| ----------------------- | ----- |
| Agent                   | Claude Code (CLI), Linux PC, subagent-driven development: a controller session dispatched one implementer subagent per task and a reviewer after each task |
| Model                   | Claude Opus 5.5 (`claude-opus-5-5`) for the controller. The controller ran Opus, Sonnet and Haiku subagents per task: the implementers were Opus for tasks 2, 3, 4 and 6 and Sonnet for tasks 1, 5 and 7 (FIXME: the Haiku jobs and the reviewer models from the session transcript) |
| Reasoning effort        | FIXME: from the session transcript |
| Skills used             | `pru-assembly` (lint, simulate, timing workflow), superpowers brainstorming, writing-plans and subagent-driven-development; the uart_print, uart_rx and i2c_io ports as the step-by-step template |
| Start                   | 2026-11-03, about 22:5x CET (design started; first commit 22:56). The schematic and TRM research files in the scratch dir date from 22:29 |
| End                     | 2026-11-04, final verification and this report, about 02:0x CET (FIXME: exact time from the last commit) |
| Wall-clock time         | about 3 h (22:5x to about 02:0x), counted from the design commit |
| Model API calls         | FIXME: from the session transcript |
| Output tokens           | FIXME: from the session transcript |
| Input tokens, uncached  | FIXME: from the session transcript |
| Input tokens, cache write | FIXME: from the session transcript |
| Input tokens, cache read  | FIXME: from the session transcript |

Commit timestamps (CET) show how the time was spent:

| Commit | Time | Content |
| ------ | ---- | ------- |
| open-pru `422e9420`, `139a8f69` | 11-03 22:56, 22:59 | design |
| open-pru `292827c6` | 11-03 23:02 | implementation plan |
| pru-sim `7282aa1` | 11-03 23:36 | Task 1: `CLK_SEL` 0 follows the PdmLink clock |
| open-pru `9d4065ae`, pru-sim `344f273` | 11-03 23:56 / 23:59 | Task 2: RTU1 modulators, harness |
| open-pru `ccb72b09`, `0faf4ce8`, pru-sim `ad2de32` | 11-04 00:32, 00:40 / 00:33 | Task 3: PRU1 clock and pins, protocol doc fix |
| open-pru `36e373e3`, pru-sim `4f6b18b` | 11-04 01:04 / 01:04 | Task 4: PRU0 8-channel receiver |
| open-pru `53fe09b0`, pru-sim `4436f32` | 11-04 01:17 / 01:17 | Task 5: clips and clip buffer tool |
| open-pru `bfbc5da2`, `22719ea5`, pru-sim `fe67563` | 11-04 01:28, 01:43 / 01:33 | Task 6: R5F app, SysConfig, ICSSG config fix |
| open-pru (this commit) | 11-04 about 02:0x | Task 7: readme, report, examples index |

Time before the first commit (research of the TRM, the schematic and the
pru-simulator code) is not in the table. Token counts are not available to the
agent that wrote this report; they come from the controller's session
transcript (`~/.claude/projects/.../<session>.jsonl`), counting only API calls
after the prompt was submitted.

## 3. What was done, in order

1. **Design** (`docs/design.md`): the user's questions were turned into a design
   with pins, cores, budgets, protocol and a board check. Facts were taken from
   the TRM (SPRUIM2H), the LP-AM243 schematic (PROC109A) and the pru-simulator
   code (section 5 of the design lists them).
2. **Plan** (`docs/plan.md`): seven tasks, with a budget checkpoint after the
   RTU1 task and a fallback (move modulator work to PRU1, then reduce the channel
   count).
3. **Task 1, pru-simulator:** `CLK_SEL` = 0 (sd8_clk, GPI16) now follows the
   PdmLink clock (`attach_pdm_link(..., clock_net=8)`), so the simulator can
   model all SD channels on one shared clock. Two new tests in
   `tests/test_sd_pin_input.py`.
4. **Task 2, RTU1:** `rtu1_mod.asm`, 8 bit-exact second-order sigma-delta
   modulators, plus the project skeleton and the RTU1 harness. Budget
   checkpoint: worst 1307 cycles per batch against 2278, so the plan continued.
5. **Task 3, PRU1:** `pru1_pdm.asm`, the cycle-exact clock and pin driver, and
   the RTU1 + PRU1 lockstep harness. The start handshake was changed to a nonce
   and echo (ruling R3) because the planned rule deadlocked.
6. **Task 4, PRU0:** `pru0_rx.asm`, the 8-channel port of `pdm_rx.asm`, and the
   three-core lockstep harness.
7. **Task 5, clips:** 8 booth clips (3 new tones), `make_clip_buffer.py`,
   `mcuplus/pdm_clips.h`.
8. **Task 6, R5F:** `pdm_audio_example.c`, SysConfig (333.33 MHz clock, 18 pads,
   MSRAM region and MPU entry), `pdm8_shared.h`. A fix round made the R5F write
   the complete ICSSG configuration registers (ruling R5).
9. **Task 7:** readme, this report, the entry in `examples/makefile` and
   `examples/readme.md`, final verification.
10. **Final review and fix wave** (ruling R7): a whole-branch review found
    four important points and six minor ones, all fixed in one pass
    (section 4, "Final review fixes"): lost IPC scratchpad writes could hang
    RTU1 and PRU1 (I1), a booth hazard when the jumpers stay on for the next
    demo (I2), no version field in the public header (I3), licence headers of
    the two FIR files (I4), plus a comment, troubleshooting rows, halting the
    unused ICSSG0 cores, a sticky SD-overflow counter, the design status and
    the copyright year.

Each task had a reviewer; the reviews approved every task, and the findings
that needed a decision are in section 4.

## 4. Decisions

### Decided by the user (design.md section 13)

- If 8 channels don't fit: move modulator work to PRU1 first, reduce the channel count only after that.
- Target open-pru and the AM243x LP, like the other demos.
- One PDM output per channel (no L/R pairing).
- Up to 8 channels. Max clock 333 MHz.
- RTU1 may be used. RTU1 cannot drive pins; it loads from RAM and shares through the scratchpad (IPC_SP).
- Use MSRAM if shared RAM is too small (it is too small for the clips).
- Single shared SD clock for all channels.
- PCM in ping-pong halves of 64 samples per channel, all channels arranged so the Ethernet PRU reads a half in sequence, then switches to the other half.
- No PicoScope. No R5F console check: the board check is the CCS memory browser.
- The simulator stays at 3 SD channels.
- Clips come from MSRAM, written by the R5F at start-up. (Refined in Task 6: the clips are linked into the R5F image and loaded with the ELF, so the R5F writes no clip data at run time.)
- open-pru ships only the 8-channel firmware (design section 10). The 3-channel
  variant exists only as load-time overrides in the pru-simulator harness.

### Calls made by the designer (design.md section 14)

RTU1 runs all 8 modulators and PRU1 only drives pins; the 28-period batch
protocol with seq and ack bytes; sample-interleaved halves with a `HALF_SEQ`
counter; one frame-interleaved clip buffer (128 000 B in a 128 KB region) that loops, with 8 different
clips; logical channels 4..7 on SD5..SD8 (SD4 is skipped because its GPI9 goes
to the RGMII PHY); `CLK_SEL` = 0 driven by the PdmLink clock in the simulator;
PRU1's timed path uses no shared memory.

### Rulings during the implementation (controller)

- **Ruling R1:** RTU1 scratch local 0x1700 = global DRAM1 0x3700 in the simulator and on silicon (the Task 2 report's "DRAM0 in sim" claim was wrong). So PRU1 must leave local 0x1700..0x17FF unused (linker and asm), and PRU0 must not touch DRAM1 0x3700..0x37FF. Cost if wrong: a memory collision between PRU1 and RTU1.
- **Ruling R2:** the start-order deadlock (PRU1 first takes a stale seq 1): PRU1 must observe seq == 0 (RTU1's clear) before accepting seq 1; the R5F still starts RTU1 first (Task 6). Cost if wrong: a hang on rerun. (Superseded in part by R3.)
- **Ruling R3:** accept the implementer's nonce/echo start handshake replacing R2's seq0-first rule. The reviewer verified it deadlock-free and stale-safe for both start orders and for reruns. Cost if wrong: a hang on silicon start.
- **Ruling R4:** fold the design section 8/9 update (nonce handshake, counters written by RTU1, no PRU1 run flag; `BATCH_COUNT` is the liveness indicator) into Task 3's fix round, so the spec stays truthful. Cost: doc churn.
- **Ruling R5:** fold the reviewer's minors 1 (write the full GPCFG0 and GPCFG1 and clear SD SHARE_EN/MAN_SD_EN), 2 (set PCTR_RST_VAL = 0 explicitly) and 4 (drop or MPU-cover the inherited NON_CACHE_MEM) into a Task 6 fix round. The booth runs several ICSSG0 demos (uart_print, i2c_io, uart_rx) back-to-back from CCS, often without a System Reset, so leftover GPO/SD mode bits are a realistic failure. Cost if wrong: a few lines of extra init.
- **Ruling R6:** fold the Task 7 reviewer's minors (Validated-HW table FIXME, DRAM0 0x07F8 vs 0x0800, unsourced statements, 128 000 B in a 128 KB region) into the Task 7 fix round: these are public-repo docs for a first board bring-up. Cost: small edits.
- **Ruling R7:** one final fix wave covers the final review's I1 to I4 and M1 to M4, M6 and the design status (M5 in part: `plan.md` is kept as it was, as a record). For I4, no TI copyright is invented for `decimation_fir.h`, whose origin is not recorded: the generator emits an SPDX line for `fir_coefs.inc`, and the provenance of `decimation_fir.h` stays an open decision for the author (section 9). Cost if wrong: a licence follow-up before merge.
- **Ruling R8:** the merge to open-pru `main` waits for the user's LP-AM243 board run (FIXMEs filled), as for the earlier ports. Cost: none.

### Final review fixes

| Finding | Fix | Check |
| ------- | --- | ----- |
| I1: same-cycle IPC scratchpad XOUTs (no arbitration) could lose RTU1's start nonce (PRU1 echoed every 6 cycles, about 1 start in 6 coincided) or a late batch (PRU1's underrun XOUT at slot t11); either hangs the pair | RTU1 reads its seq (at the start also the nonce) back after every XOUT and writes again (unbounded retry: RTU1 alone writes bytes 0..29). PRU1's start loop XINs all 32 bytes and XOUTs R9.b2/b3 only when the SPAD does not hold (0, nonce): once per nonce, again if lost; every pass 9 cycles. Each batch slot XOUTs both R9.b2 and R9.b3, so the next slot repeats a lost underrun count or ack. PRU1's timed path keeps its cycle counts and stays memory-free | `test_lost_spad_write_at_start_recovers` (4 cases) and `test_lost_spad_write_on_underrun_recovers` (3 cases) drop one XOUT of RTU1 or PRU1; `test_pru1_start_writes_once_per_nonce` counts PRU1's start XOUTs (3 before the first edge; the old loop made 3333 in 20 000 cycles alone). On the old firmware 7 of these 8 fail (4 hang, 1 counts 9 false underruns, 1 loses an underrun for good, 1 counts 3333 writes); all pass now. RTU1 worst 1329 cycles per batch (was 1327) |
| I2: with the jumpers on, i2c_io's push-pull SDA on BP.32 fights PRU1_GPO1 (BP.67), which keeps running | readme: "Before running another demo: do a System Reset (or halt PRU1) and remove the jumpers; i2c_io drives BP.32", and a troubleshooting row. No other demo changed | docs |
| I3: no version field in the public header | `PDM8_HDR_VERSION` (+0x2C) = `PDM8_VERSION` = 1, written by the R5F; readers must check MAGIC and VERSION (`.inc`, `.h`, readme) | `test_c_header_matches_inc`; the harness writes it as the R5F does |
| I4: no SPDX line in `fir_coefs.inc` and `decimation_fir.h` | `gen_fir_coefs.py` emits `; SPDX-License-Identifier: BSD-3-Clause` as the first line (no copyright line: the data come from `decimation_fir.h`); regenerated. `decimation_fir.h` unchanged: provenance open (section 9, readme) | `test_fir_coefs_regenerate_identically` compares the tool output with the shipped file byte for byte, and the shipped file with the pru-simulator copy after removing exactly that SPDX line |
| M1: PRU0 comment "m odd: half 1" | "m even: half 1" (comment only) | - |
| M2: troubleshooting "one channel's PCM_COUNT is 0" and "BATCH_COUNT frozen means RTU1 stalled" | a missing data jumper gives a flat or noisy column, not a stuck count (if one count stops, all stop); a frozen BATCH_COUNT is RTU1 stalled, PRU1 halted, or a failed start handshake (readme, design section 9) | docs |
| M3: a leftover RTU0 / TX_PRU0 / TX_PRU1 from an earlier demo would corrupt PRU0's DRAM0 0x1000..0x19FF | the R5F also halts `PRUICSS_RTU_PRU0`, `PRUICSS_TX_PRU0`, `PRUICSS_TX_PRU1` (SDK `pruicss.h` IDs 2, 4, 5) | make, CCS build |
| M4: no board-readable SD overflow indicator | PRU0 tests R31[29] at each spin exit before the clear and adds 1 to `SD_OVF_COUNT` (+0x30), sticky; readme step "SD_OVF_COUNT stays 0"; design section 7 fallback points at it. w_comb 48 -> 49 | `test_sd_overflow_is_counted` (PRU0 stalled 2 SD windows: count = 3, then stays); `test_rx_writes_only_its_memory` checks the count stays 0 in the normal run |
| M5: design status | "implemented (board run pending)"; `plan.md` kept | - |
| M6: copyright years 2024-2026 in the new R5F file | 2026 | - |

## 5. Firmware changes (simulator source -> open-pru)

| Change | Why | Impact |
| ------ | --- | ------ |
| `pdm_rx.asm` -> `pru0_rx.asm`: `N_CH` = 8, SD map `SD_CH_0..7` = {0,1,2,3,5,6,7,8} as plain-literal build options with `.if/.emsg` guards; every channel `CLK_SEL` = 0, `CLK_INV` = 0, `SD_SAMPLE_SIZE` = 95 | 8 channels on one shared clock (GPI16); SD4 is skipped | none on the DSP: the block from `PDM_CH:` to `PDM_SAT_OK:` (120 lines) is byte-identical to the source, apart from one `QBBS` at the spin exit that counts SD overflows (`SD_OVF_COUNT`, final review fix M4) |
| PRU0 output: interleaved ping-pong halves in shared RAM, `PCM_COUNT[c]`, `HALF_SEQ` published by the N_CH-th channel to finish a half (per-half done counter in DRAM0 0x7F8); the BLOCK_EVENT and ring code is removed | the Ethernet-reader interface | with the FIR stagger the last channel to finish is the last odd channel, so the rule works for 3 and 8 channels |
| `pdm_gen.asm` -> two cores: RTU1 `rtu1_mod.asm` (modulators) and PRU1 `pru1_pdm.asm` (clock, pins) | at 333 MHz one period is 108.5 cycles; one core cannot do 8 modulators and the clock | RTU1 1329 of 2278 cycles per batch, PRU1 about 14 % busy |
| Clock 108 + 73/144 cycles per period (integer fractional adder, long period in the high half) | 3.072 MHz from 333.33 MHz | edges equal `pdm_clock_edges` exactly |
| Pin mapping in registers (fixed 9 cycles, `m_map`), one R30 write per half period; data changes with the falling edge | PRU1's timed path must not touch memory | setup 54 cycles (needs 26), hold 54/55 |
| IPC scratchpad XFR ID 15, 28-period batches, seq/ack, nonce/echo start, silence batch and underrun count on a late RTU1 | no torn reads, underruns visible, PRU1 timing independent of RTU1 | counters are stored by RTU1 for PRU1, and lag by up to one batch |
| Lost-write handling: RTU1 reads back and repeats, PRU1 writes only when the SPAD differs at the start and both of its bytes at every slot | the SPAD has no arbitration (final review fix I1) | RTU1 +2 cycles per batch; PRU1 slot timing unchanged |
| `VERSION` (+0x2C) and `SD_OVF_COUNT` (+0x30) in the header | readers check the layout; the board shows a PRU0 overrun | PRU0 w_comb +1 cycle |
| `decimation_fir.h` copied unchanged; `fir_coefs.inc` regenerated with an SPDX first line | open-pru convention; the reference reads the same include | `fir_coefs.inc` equals the pru-simulator copy apart from the SPDX line; `decimation_fir.h` byte-identical, no licence header (provenance open, section 9) |
| `gen_fir_coefs.py`: default paths changed, SPDX first line | regenerates `fir_coefs.inc` byte for byte | a test checks it |
| SPDX/TI header, entry labels, `.retain`, `.if/.emsg` guards in all three cores | open-pru conventions, `clpru` | none |

The simulator assembler has no `.emsg` and does not expand a `.set` that refers
to another `.set`, so the shared map `pdm8_shared.inc` uses plain literals and
checks derived values with guards.

## 6. SysConfig and R5F changes

Compared with the uart_print project (the copied structure), in
`mcuplus/am243x-lp/r5fss0-0_freertos/example.syscfg`:

- ICSSG0 core clock: `pruicss1.coreClk = 333333333` (as `pru_emif` and
  `spi_loopback`). `iepClk` stays at its default of 200 MHz; the demo uses no
  IEP.
- 18 pads as two `PruGPIO` instances: PRU1 GPO0, 1, 2, 6, 11, 12, 13, 15, 16
  (mode 0, no pull) and PRU0 GPI1, 3, 5, 7, 11, 13, 15, 16, 17 (mode 1,
  receiver on, no pull). Each pad was checked against the SysConfig device data,
  the generated `ti_pinmux_config.c` and the schematic ball table (all 18 match).
- Memory region `PDM_CLIPS` at 0x700C0000 (128 KB region, the buffer is 128 000 B), section `.pdm_clips`, and
  an MPU entry `CONFIG_MPU_REGION5` (Normal non-cacheable, no execute). The
  inherited `NON_CACHE_MEM` region (no MPU entry of its own, so it was cached
  in fact) was removed (ruling R5).

The R5F (`mcuplus/pdm_audio_example.c`): halts RTU1, PRU1 and PRU0, and RTU0,
TX_PRU0 and TX_PRU1 (final review fix M3: their DRAM0 share holds PRU0's state); writes
GPCFG0 (PRU0 mux = SD, everything else at its reset value), GPCFG1 (all reset)
and PRU0_SD_CFG as whole registers; sets CTBIR0 C24 = 0; zeroes shared RAM and
both DRAMs; writes the header (with VERSION); loads the three IRAMs; sets PCTR_RST_VAL = 0;
resets and enables RTU1, PRU1, PRU0 in that order; idles. The registers the SDK
has no API for are written through the `hwAttrs` bases, as the motor-control
SDK SDFM driver does.

**Deviation from the open-pru runbook.** `docs_ai/task_create_project.md` says
not to edit `example.syscfg` pin routing or PRUICSS settings by hand. This edit
was made by hand, as for the earlier ports, and validated with the SysConfig
command line, both builds and the generated files. Please open the file once in
the SysConfig GUI, check the clock and the pins, and save (open item).

## 7. Verification

### Budgets (pru-simulator, 333.33 MHz)

| Check | Result |
| ----- | ------ |
| RTU1 cycles per 28-period batch | worst 1329, mean 1256, min 1243, limit 2278 (58 %). Task 2 (before the counters moved to RTU1): 1307; before the final fix wave (seq read-back): 1327 |
| RTU1 path costs | group of 4 periods 168 cycles, batch end 38..43 (+2 seq read-back), frame fetch 73 (83 with a wrap) |
| PRU1 per batch | about 418 of 3038 cycles busy (13.8 %); the rest is LOOP/NOP padding. Per period 14 busy of 108 (15 of 109), the slot period 26/27 |
| PRU1 setup / hold | setup 54 cycles (162 ns, needs at least 26), hold 54/55 |
| PRU0, 3 channels, measured | w_comb 49 (48 before the SD_OVF_COUNT test), w_fir 920, spin 1, wrap 3, worst window 1983 |
| PRU0, 8 channels, extrapolated | 8 x 49 + 4 x 920 + 7 + 3 = 4082 of 10 417 cycles (39 %) |
| PRU0, pessimistic bound | 4447 cycles (43 %): every FIR input negative (+64 per FIR) and one PDM period of shadow skew |
| RTU1 MSRAM latency tolerated | about 973 cycles per frame fetch: spare 2278 - 1329 = 949, plus the 24 cycles of the simulated fetch already inside the 1329 (the Task 2 figure of about 990 was before the counters moved to RTU1) |

The design's fallback (move modulator work to PRU1, then reduce the channel
count) was not needed: 8 channels at 333.33 MHz fit all three cores.

### Simulator harness

`tests/test_pdm_open_pru_port.py` in pru-simulator, branch
`feat/pdm-open-pru-check`, finds the project through `OPEN_PRU_PATH` (default
`~/GitHub/open-pru`) and skips if the project is missing. It has 53 tests (38
test functions, some parametrised), grouped by core in `readme.md`, section
"Verification". The checks that matter most:

- RTU1 batches, the PRU1 pins and the PRU0 PCM are **bit-exact** against
  `reference.sigma_delta2` and `reference.pcm_chain_for_channel` (at 3 channels
  for the PCM).
- The clock edges equal the 108 + 73/144 schedule over 3 x 144 periods.
- Start order: RTU1 first, PRU1 first, and 5 stale scratchpad states do not
  deadlock. A rerun on stale scratchpad and shared RAM behaves exactly like the
  first run (`test_rerun_clears_handshake`).
- An injected RTU1 stall gives `UNDERRUN_COUNT` > 0 and silence bits, while the
  PRU1 clock edge times are unchanged.
- PRU1's timed path has no memory access (static check and a run-time recorder).
- One lost scratchpad XOUT of either core, at the start or around an underrun,
  does not hang the pair; the stream stays bit-exact and every silence batch is
  counted.
- PRU0 counts a lost SD sample in `SD_OVF_COUNT` (2-window stall: 3, one per
  channel) and the normal run leaves it at 0.
- `HALF_SEQ` is published only when all channels have written the half (checked
  at each store), 4.0000 ms apart.
- PRU0 reads and writes only its own DRAM0 range and the shared RAM.
- The clip header equals `pdm_layout.load_clip`, `pdm8_shared.h` equals
  `pdm8_shared.inc`, `fir_coefs.inc` regenerates byte for byte.
- Mutation checks in each task (wrong pin, wrong tail bit, early publish,
  swapped parity, no nonce echo, and others) were all caught by at least one
  test.

Results on the final tree (2026-11-04):

| Check | Result |
| ----- | ------ |
| pru-simulator `env -u PYTHONPATH .venv/bin/python -m pytest tests -q` (includes `test_pdm_e2e.py`) | 3 failed, 3533 passed, 3 skipped (PDM_SLOW opt-in), 2 xfailed in 434 s. The 3 failures are the known baseline: 2 in `test_clock_speed_endpoint.py`, 1 `test_server.py::test_am263x_state_advertises_only_supported_cores` |
| pru-simulator `node --test tests/ui_*.test.js` | 113 tests, 104 passed, 9 failed. The 9 failing test names are identical to the baseline run |
| open-pru `make DEVICE=am243x ...` from the project dir | exit 0, 0 errors; the only warning is the inherited SysConfig part-number notice; `pdm_audio.release.appimage.hs_fs` generated |
| CCS 21.0.1 headless projectCreate + Release build, fresh workspace, 4 projects | RTU1, PRU1, PRU0: 0 errors, 0 warnings; R5F: 0 errors, 1 warning (the same SysConfig notice) |
| `clpru -v4` on `rtu1_mod.asm`, `pru1_pdm.asm`, `pru0_rx.asm` | exit 0, no diagnostics |
| clpru image sizes | RTU1 .text 0x69C, PRU1 0x9F4, PRU0 0x794 bytes |
| `examples/makefile` dry run (`make -n`) | enters `electronica2026/pdm_audio/pdm_audio` |
| pru-assembly linter `--openpru` (run in Tasks 2 to 4, not repeated) | 0 errors; PRU0 has one soft warning, "79-column", on lines copied verbatim from `pdm_rx.asm` |
| Hardware run on LP-AM243 Rev A, CCS memory browser (2026-11-04, user) | **Passed**, all 8 channels, after 1230 s: header as expected, UNDERRUN_COUNT 0, SD_OVF_COUNT 0, HALF_SEQ / BATCH_COUNT / RTU1 frames / PCM_COUNT all give the same run time, PCM_COUNT[0..7] within 2, tones 500 Hz to 5 kHz at the right frequency, about -90 dB fit residual. Details in `readme.md`, "Hardware results" |
| PDM clock on a PicoScope 3416E (D0) | 3.072 MHz (min 3.071, max 3.072 MHz) |

The three known failing tests in pru-simulator (2 in
`test_clock_speed_endpoint.py`, 1 `test_server.py::test_am263x_state_advertises_only_supported_cores`)
are the same as on `feat/pdm-audio-demo` before this work; the node UI tests
have the same 9 failures as on `main`.

Tool versions: CCS 21.0.1 (`ccs2101`), ti-cgt-pru 2.3.3, ti-cgt-armllvm
5.1.1.LTS, SysConfig 1.28.1, MCU+ SDK AM243x 12.01.00.33 (one minor version
newer than the validated 11.1 - 12.0). Environment notes for building on this
Linux PC are the same as in the other ports: the SDK path and
`SYSCFG_NODE` (`ccs/tools/node/node`) are passed on the make command line, and
`imports.mak` is not modified. Always build from the project directory, never at
the repo root (a root build dirties `pru_i2s`).

## 8. Things the simulator could not show

- **RTU1 MSRAM latency.** The simulator reads MSRAM in a flat ~24 cycles. The
  margin covers a fetch of about 975 cycles (951 spare plus the 24 simulated), but the real value is unmeasured. A slow
  fetch shows as `UNDERRUN_COUNT` > 0. On the board it stayed 0 for 20 minutes
  (2026-11-04), so the fetch fits; its length is still not measured.
- **XOUT to XIN visibility between RTU1 and PRU1.** Assumed one cycle each
  (as in the simulator and the skill's timing reference). The protocol does not
  depend on the exact delay: a late batch costs one underrun, and a read-back
  that runs ahead of its own write only repeats the write. There is also
  no hardware arbitration of the scratchpad, so the byte ownership rules
  (each core XOUTs only its own bytes) are essential, and whether two
  same-cycle XOUTs are merged is not known: the firmware repeats any write
  that did not land (final review fix I1). The simulator merges them, so the
  harness drops single writes to test this.
- **Channels 3 to 7.** The simulator has 3 SD channels, so the PCM of channels
  3 to 7 and the SD routing of SD5 to SD8 are verified only statically (pin
  table, the assembly with the 8-channel options, clpru). The board run
  (2026-11-04) showed all of them working. The first board run had to read
  `PCM_COUNT[3..7]` and compare them with the other channels, and
  `SD_OVF_COUNT`, which counts every SD sample PRU0 was too late for (R31[29],
  tested at each spin exit). The flag itself is not a check from the CCS
  register view: PRU0 clears it for every channel in each window.
- **SD shadow overflow semantics.** The TRM (SPRUIM2H Table 6-419) defines
  R31[29] as shadow_update_flag_ovf: a shadow latched while valid was still
  set. The pru-simulator models `SDChannel.ovf` as a 28-bit accumulator
  overflow instead, which is set in every normal run (acc3 passes 2^28; the
  comb works mod 2^28): unpatched, the shared 3-channel run counts 1122
  "overflows" without a lost sample. The harness therefore replaces the flag
  with the TRM behaviour for PRU0's channels (`trm_shadow_ovf`); the
  accumulators and shadows stay the simulator's. A simulator fix is a separate
  pru-simulator item (section 9).
- **Shadow skew on the board.** In the simulator PRU0 reinitialises before the
  first PDM edge, so all SD shadows latch together. On the board the clock is
  already running when PRU0 starts, so the 8 back-to-back reinits can straddle
  an edge, at a cost of at most one extra spin of 109 cycles per window. That
  is in the pessimistic bound (4439). The PCM itself is not affected.
- **ICSSG register state after other demos.** The simulator resets the
  registers. The R5F writes the full GPCFG and SD_CFG registers (ruling R5), but
  that was checked only against the TRM.
- **Pad delays and rise times.** At 3.072 MHz with 162 ns of setup they should
  not matter, but they are not measured. The scope check is optional.
- **Register contents after reset.** The firmware clears or initialises what it
  uses; the simulator starts at 0.

## 9. Open items

- [x] **Run on LP-AM243 hardware** and do the memory-browser check in
      `readme.md`. Done 2026-11-04 on Rev A: passed, results in section 7
      and in `readme.md`, "Hardware results". The level of the higher tones
      falls by the zero-order hold of RTU1 (x0.85 at 5 kHz), as predicted;
      the readme now says so. Tone columns repeat exactly within a half, so
      the readme's "frozen column" check now applies to channels 0 to 2 only.
- [ ] **decimation_fir.h: provenance and licence to be confirmed by the
      author before merge.** It is a byte-identical copy of the pru-simulator
      file (from the audio_pipe demo) and has no licence header; its origin is
      not recorded, so no TI copyright was added (ruling R7). Once confirmed,
      add the header here and in pru-simulator together (the harness checks
      the identity), and decide whether `gen_fir_coefs.py` should then emit a
      copyright line too. `fir_coefs.inc` now carries the SPDX line from the
      generator.
- [ ] **pru-simulator: `SDChannel.ovf` semantics.** The simulator sets R31[29]
      on a 28-bit accumulator overflow; the TRM defines it as the shadow
      update flag overflow (section 8). The harness patches it for this demo;
      the simulator itself should follow the TRM (its own SD tests assert the
      current behaviour, so that is a separate change).
- [ ] **Conflict with PRs #188 / #189 / #190.** `examples/makefile` and
      `examples/readme.md` each get one line for `electronica2026/...` in every
      one of these four branches (i2c_io, uart_rx, uart_print and this one).
      Whichever merges first, the others need a small conflict resolution in
      those two files: the `SUBDIRS` list in the makefile, the project
      description list, and the portability table (`electronica2026/` group
      header row plus the project row) in the readme. The entry here is
      `electronica2026/pdm_audio/pdm_audio`.
- [x] **`examples/readme.md` portability row:** `Y` for am243x, `Yport` for
      the other five (user, 2026-11-04: they can be ported with limitations,
      for example the lower PRU clock on AM261x). The agent had proposed
      `Npru` for am261x, am263px, am263x and am62x.
- [ ] **SysConfig GUI re-save.** `example.syscfg` was edited by hand (section
      6). Open it once in the GUI, check the clock and the pins, and save.
- [ ] **`iepClk`** stays at its default of 200 MHz. The Ethernet-reader project
      may want to set it.
- [ ] **DRAM0 0x1000..0x1A00** is the open-pru default share of RTU0 and
      TX_PRU0. This demo runs neither; a later demo that adds them would
      collide.
- [ ] **Odd-channel PCM is 1/32 kHz late** because of the FIR stagger. This
      matters for future beamforming across channels.
- [ ] **Modulator input range:** int32 integrators; 0.5 FS clips are safe;
      sustained near-FS input wraps.
- [ ] **Deferred minor review findings:**
  - `attach_pdm_link` does not validate `clock_net`; the test name
    `test_clk_sel_zero_gets_no_ticks` is slightly misleading now; the
    pru-simulator plan line 95 is stale about `CLK_SEL` 0.
  - No test for a single-core restart (the R5F always restarts both cores) and
    none for RTU1 holding batch 0 with stale bytes 30 and 31.
  - The `pdm8_shared.inc` sentence about "only PRU1 restarted" attributes the
    255 false underruns to PRU1; they come from RTU1's mod-256 delta at a
    publish PRU1 never acks.
  - The pessimistic PRU0 bound excludes the sat24 and DC-branch variants (a few
    cycles); `pdm_audio_load` has no IRAM size bound of its own (the linker
    bounds it).
  - `UNDERRUN_COUNT` aliases if more than 255 underruns happen between two RTU1
    publishes; GPCFG read-back asserts include the GPI_SB bit (safe in direct
    mode); an over-long comment line in `pdm_clips.h` (the R5F one was fixed
    in the final fix wave).
  - The other electronica2026 ports keep "2024-2026" in their
    `*_example.c` (copied from the SDK template), while their new firmware
    files say 2026. This port now uses 2026 in `pdm_audio_example.c`
    (final review M6); `main.c` keeps the template's 2024-2025.
- [ ] **Readme FIXMEs left after the board run:** the LP boot-switch
      positions and SOC initialization need, the XDS110 console line, whether
      the R5F was running while the memory browser refreshed, the I/O voltage
      of the pads (schematic), the CCS memory-graph field names, the CCS menu
      path of System Reset, start-up after another ICSSG0 demo without a
      System Reset, and the PDM data pins on a scope. The PCM_COUNT spread
      is now filled in (within 2).
- [ ] **PR.** The branch and the pru-simulator branch
      `feat/pdm-open-pru-check` (stacked on PR #106) were pushed on
      2026-11-04 at the user's request, before the board run; no PR is
      open yet.
- [ ] The i2c_io, uart_rx and uart_print directories in the working tree are
      untracked copies from other branches and are not part of this branch.

## 10. Files

```
examples/electronica2026/pdm_audio/pdm_audio/
  makefile                                  am243x only: three PRU cores + R5F
  readme.md                                 wiring, run steps, memory-browser check, reader interface
  docs/design.md, docs/plan.md              design and implementation plan
  docs/porting_report.md                    this file
  clips/                                    8 booth clips, SOURCES.md
  tools/make_clip_buffer.py                 clips -> mcuplus/pdm_clips.h
  tools/gen_fir_coefs.py                    regenerates firmware/fir_coefs.inc
  firmware/pdm8_shared.inc                  shared map: header, SPAD protocol, constants
  firmware/rtu1_mod.asm                     RTU1: 8 sigma-delta modulators
  firmware/pru1_pdm.asm                     PRU1: cycle-exact clock and pin driver
  firmware/pru0_rx.asm                      PRU0: 8-channel SD receiver and PCM chain
  firmware/fir_coefs.inc, decimation_fir.h  FIR coefficients (copied from pru-simulator)
  firmware/am243x-lp/icss_g0_rtu_pru1_fw/ti-pru-cgt/
  firmware/am243x-lp/icss_g0_pru1_fw/ti-pru-cgt/
  firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/
                                            each: example.projectspec, makefile,
                                            makefile_projectspec, linker.cmd
  mcuplus/pdm_audio_example.c               R5F: ICSSG config, load, start
  mcuplus/pdm8_shared.h                     C mirror of pdm8_shared.inc
  mcuplus/pdm_clips.h                       generated clip buffer (.pdm_clips)
  mcuplus/am243x-lp/r5fss0-0_freertos/
    example.syscfg                          333.33 MHz, 18 pads, PDM_CLIPS region, MPU entry
    main.c
    ti-arm-clang/ example.projectspec, makefile, makefile_projectspec,
                  makefile_ccs_bootimage_gen, syscfg_c.rov.xs
examples/makefile                           project added to SUBDIRS
examples/readme.md                          project list + portability row
pru_simulator (branch feat/pdm-open-pru-check, stacked on PR #106):
  pru_io/sd_filter.py, simulator.py         CLK_SEL 0 follows the PdmLink clock
  tests/test_sd_pin_input.py                two new tests
  tests/test_pdm_open_pru_port.py           the harness (53 tests)
```
