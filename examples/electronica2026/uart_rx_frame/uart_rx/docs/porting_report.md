# Porting report: pru-simulator UART RX 11-byte frame -> AM243x LP

Date: 2026-11-02
Branch: `dev_tley_electronica2026_uart_rx` (open-pru)
Source: `pru_simulator/source/uart_rx_11frame.asm`
Target: `examples/electronica2026/uart_rx_frame/uart_rx` (AM243x LaunchPad,
ICSSG0 PRU0, R5F r5fss0-0 FreeRTOS host, CCS)

## 1. Prompt

The port was done by an AI agent (Claude Code) from this single prompt:

> Move the project from @source/uart_rx_11frame.asm to a new dev branch on
> open-pru\examples\electronica2026\uart_rx_frame\. It should run on real
> target AM243 LP using CCS. Start with empty project on open-pru and copy it
> over to the uart_rx_frame folder. Only AM243 LP hardware variant is ok.
> Update sysconfig in this project to set correct PRU clock and PRU input
> pin. Change the PRU input pin to GPIO2 of PRU0. Only load PRU0 from
> ICSS_G0. We can drop all other PRUs from ARM loader. Rename the empty
> project in the uart.. folder to uart_rx. Document the porting effort
> including this prompt, model, effort, tokens and time used.

Earlier in the same session, the source had been run in pru-simulator with
the payload `HelloWorld!`, using the `pru_uart_inject` method of the
simulator's MCP server class (called directly in Python, because the running
MCP server rejected the list payload). It only received the frame with the
PRU clock at 200 MHz, which confirmed the clock dependency (section 6). During the
port the agent asked one question (section 4). There was no other human
input.

## 2. Model, effort, tokens and time

| Item                    | Value                                             |
| ----------------------- | ------------------------------------------------- |
| Agent                   | Claude Code 2.1.316 (CLI), Linux PC               |
| Model                   | Claude Opus 5.5 (`claude-opus-5-5`)               |
| Reasoning effort        | high (as recorded in the session transcript)      |
| Skills used             | `pru-assembly` (lint, simulate, timing workflow); the earlier i2c_io port (`../../i2c_io_expander/i2c_io/docs/porting_report.md`, branch `dev_tley_electronica2026_i2c_io`) as the step-by-step template |
| Start                   | 2026-11-02 16:29:42 CET (prompt submitted)        |
| End                     | 2026-11-02 16:41 CET (report finished, before commit)                                   |
| Wall-clock time         | 12 min 04 s                                       |
| of which user answering | 1 min 49 s (one question)                         |
| Model API calls         | 52                                      |
| Output tokens           | 57,723                                        |
| Input tokens, uncached  | 104                                         |
| Input tokens, cache write | 147,870                                       |
| Input tokens, cache read  | 7,770,103                                       |

The token numbers are summed from the `usage` fields of the Claude Code
session transcript (`~/.claude/projects/.../<session>.jsonl`), counting only
API calls after the prompt was submitted. They were measured just before the
commit, so they leave out the commit step itself. Cache-read tokens dominate.
Every agent turn re-reads the cached conversation (tool outputs, file
contents), so they grow with the number of turns, not with the amount of new
work.

Compared with the i2c_io port (16 min 32 s, 81 calls, 70,397 output tokens),
this port reused that port's file transforms and checks, so most of the
effort went into the firmware decision (section 4) and the timing
measurement (section 7).

## 3. What was done, in order

1. Created branch `dev_tley_electronica2026_uart_rx` from `origin/main`
   (58cbf1a8). It is independent of the i2c_io branch (open PR #188).
2. Copied the tracked AM243x LP files of `examples/empty` (PRU0 firmware
   project, R5F project, `main.asm`, top makefile, readme, host source) to
   `examples/electronica2026/uart_rx_frame/uart_rx`. No other board or core
   was copied.
3. Renamed `empty` to `uart_rx` everywhere: both `example.projectspec`, both
   `makefile_projectspec`, the PRU and R5F makefiles, host source
   `empty_example.c` -> `uart_rx_example.c` and `empty_example_main` ->
   `uart_rx_example_main`. Fixed the `OPEN_PRU_PATH` depth (6 -> 8 levels).
   Removed the PRU1, RTU_PRU0/1 and TX_PRU0/1 include paths from the R5F
   projectspec. The top makefile is the i2c_io one with the project name
   changed (am243x only, LP PRU0 and R5F targets, `imports.mak` 4 levels up).
   All build files were diffed against the i2c_io port with names swapped:
   the only differences are titles and descriptions.
4. Wrote the R5F loader (`mcuplus/uart_rx_example.c`, from the i2c_io one):
   clear PRU0 DRAM, set GP_MUX_SEL to GP mode, load and start PRU0 only.
5. Ported the firmware into `firmware/main.asm` (section 5).
6. Updated `example.syscfg`: PRU core clock and the PRU0 GPI2 input pin
   (section 6).
7. Verified: lint, makefile build, pru-simulator runs of the built `.out`
   against the original source, a per-cycle timing trace, a dry run of
   `examples/makefile`, and a CCS headless import and build of both projects
   (section 7).
8. Added the project to `examples/makefile` and `examples/readme.md`, and
   wrote `readme.md` and this report.

## 4. Decision made with the user

| Issue | Finding | User decision |
| ----- | ------- | ------------- |
| Unbounded frame offset | The source does `r1 += 11` after every frame and never wraps it. That is fine for a short simulation. On silicon, a continuous stream writes past PRU0 DRAM0 after about 745 frames, then into PRU1 DRAM and shared RAM, and after about 11,900 frames (about 0.3 s of back-to-back frames at 4 Mbaud) into the ICSSG INTC and CFG registers. | **Ring buffer**: wrap `r1` to 0 when the next frame would reach 0x0FFC (372 frames x 11 bytes) |

Alternatives that were offered: a single slot (always offset 0, newest frame
only), or keeping the source unchanged.

A ring buffer only helps a reader if the reader can find the newest frame.
The agent therefore also writes the 16-bit frame count to the free DRAM0
bytes at 0x0FFC (section 5). This is the one addition that was not asked for
explicitly. Remove the `sbco &r20, c24, r18, 2` line if it is not wanted.

## 5. Firmware changes (simulator source -> `firmware/main.asm`)

| Change | Why | Timing impact |
| ------ | --- | ------------- |
| Added TI copyright header, `.retain`, `.retainrefs`, `.global main`, `.sect ".text"`, `main:` label | Required for clpru / CCS `.out` (`best_practices.md`) | none |
| `zero &r0, 120` before the original `ldi` initialisation | Register init (`best_practices.md`) | +1 cycle at start only |
| RX pin GPI0 -> GPI2: `GPI0_MASK = 1` -> `RX_MASK = 4` in the four `and r15, r31, ...` reads | Prompt: input on GPIO2 of PRU0 | none, same instruction |
| `store_frame`: after `add r1, r1, 11`, `ldi r18, 0x0FFC` + `qbgt ring_ok, r1, r18` + `ldi r1, 0` | Ring buffer (section 4) | +2 or +3 cycles, after the last stop bit sample |
| `store_frame`: `sbco &r20, c24, r18, 2` | Publish the frame count at 0x0FFC | +1 to +2 cycles, same place |
| `qbeq store_frame, r16, 11` -> `FRAME_LEN` symbol; `store_bN:` labels on their own line | Readability, open-pru style | none |
| Rewrote the header: behaviour, DRAM0 layout, hardware requirements, register map and a measured cycle budget. The source's working notes ("Wait - ...", "Revised equalized design") were removed | Skill and `best_practices.md` | none |

Unchanged: register allocation, `BIT_TIME = 22`, `START_DELAY = NEXT_DELAY =
34`, the per-byte start-bit resync, the `qbeq store_bN` chain, the framing
error path and the DRAM0 0x0FFE error flag. Every instruction on the receive
path is the same as in the source, so the simulator timing still holds.

## 6. SysConfig changes (`mcuplus/am243x-lp/r5fss0-0_freertos/example.syscfg`)

```js
pruicss1.instance  = "ICSSG0";
pruicss1.coreClk   = 200000000;          // firmware timing assumes 200 MHz
pruicss1.AdditionalICSSSettings[0].PruGPIO.create(1);
...PruGPIO[0].PRU_ICSSG0_PRU.$assign      = "PRU_ICSSG0_PRU0";
...PruGPIO[0].PRU_ICSSG0_PRU.GPI2.$used   = true;   // RX
...PruGPIO[0].PRU_ICSSG0_PRU.GPI2.$assign = "PRG0_PRU0_GPO2";
...PruGPIO[0].PRU_ICSSG0_PRU.GPI2.rx      = true;
...PruGPIO[0].PRU_ICSSG0_PRU.GPI2.pu_pd   = "pu";   // open input idles high
```

**Why 200 MHz matters here.** The receiver is cycle-counted: 4 Mbaud at
200 MHz is 50 cycles per bit. In the same session, the unchanged source was
run in pru-simulator with a local 300 MHz configuration and received nothing
(framing error flag set). At 200 MHz it received `HelloWorld!` correctly.
200 MHz is also the SysConfig default. It is written explicitly to document
the dependency, but SysConfig GUI may drop it on save because it equals the
default.

**Why the pull-up.** The simulator drives the input idle high. On silicon an
unconnected or not yet powered sender leaves the pad floating, and a low
level looks like a start bit. In the timing trace (section 7), a GPI that
was low from reset made the firmware start its first byte 14 cycles before
the real start bit.
With the pull-up the line idles high.

The generated code was checked:

- `ti_power_clock_config.c`: `TISCI_DEV_PRU_ICSSG0_CORE_CLK, 200000000`
- `ti_pinmux_config.c`: `PIN_PRG0_PRU0_GPO2` (ball G1),
  `PIN_MODE(1) | PIN_INPUT_ENABLE | PIN_PULL_DIRECTION`. Bit 16
  (`PIN_PULL_DISABLE`) is clear, so the resistor is on; bit 17 selects
  pull-up.

GP_MUX_SEL (direct GPIO mode) is not a SysConfig output. The R5F sets it
explicitly with `PRUICSS_setGpMuxSelect(..., PRUICSS_GP_MUX_SEL_MODE_GP)`
before `PRUICSS_loadFirmware()`.

**Deviation from the open-pru runbook.** `docs_ai/task_create_project.md`
says not to edit `example.syscfg` pin routing or PRUICSS settings by hand.
This edit was made because the prompt asked for it explicitly. Please open
the file once in the SysConfig GUI, check the clock and the pin, and save.
The GUI also migrates the device to the new part-number scheme (`AM2434`),
which removes the build warning listed in section 7.

## 7. Verification

| Check | Result |
| ----- | ------ |
| Lint (`pru-assembly` `lint_pru.py --openpru`) | 0 errors, 0 warnings |
| Project makefile build, PRU + R5F (DEVICE=am243x, BUILD_MCUPLUS=y) | exit 0, `uart_rx.release.appimage.hs_fs` generated |
| PRU image size (`.map`) | 0x164 bytes (89 instructions) of 0x3000 IRAM |
| `examples/makefile` (dry run, `make -n`) | enters `electronica2026/uart_rx_frame/uart_rx` and its PRU0 firmware directory |
| CCS 21.0.1 headless import + build, PRU project | 0 errors, 0 warnings |
| CCS 21.0.1 headless import + build, R5F project | 0 errors, 1 warning (SysConfig part-number migration notice, inherited from the `empty` syscfg) |
| pru-simulator, `HelloWorld!`, 1 frame: source (GPI0) vs built `.out` (GPI2) | both: r20 = 1, DRAM0 = `HelloWorld!`, error flag 0. Port: count at 0x0FFC = 1 |
| pru-simulator, 3 frames with 0, 1 and 2 idle bits between frames | source and port: 3 frames, data correct, error flag 0 |
| pru-simulator, baud sweep 3.50-4.50 Mbaud in 50 kbaud steps, 2 frames of 0x55 0xAA 0x00 0xFF 0x01 0x80 0x7F 0xFE 0x0F 0xF0 0x5A | source and port identical: correct from 3.85 to 4.00 Mbaud |
| pru-simulator, ring wrap: r1 preset to slot 371 (offset 4081), r20 = 371, 3 frames | frames at 4081, 0 and 11, r1 = 22, count at 0x0FFC = 374, 0x0FFE untouched |
| pru-simulator, per-cycle trace of the `.out` (positions in the 50-cycle bit cell, middle = 25) | first data sample 75-77 cycles after the start edge; data samples at 25-33 (25-41 when frames are back-to-back, because `store_frame` delays the next start detection by 12 cycles); stop samples at 31-43 |
| Hardware run on LP-AM243, 2026-11-03: PicoScope 3416E AWG, `test/helloworld_4mbaud_awg.csv` at 20 kHz (4 Mbaud), 0 V / 2 V, no level shifter | pass: CCS memory browser shows `HelloWorld!` back-to-back from 0x30000000, frame-aligned (first slot starts with `H`); count at 0x30000FFC climbing; 0x30000FFE = 0 |

The simulator runs used a copy of `memory.cfg` with both PRU clocks set to
200 MHz. The scripts are not part of this project. They load the built
`.out` with `Simulator.load_elf()` and attach the simulator's
`UARTFrameGenerator` to pin 2.

Tool versions used: CCS 21.0.1 (`ccs2101`), ti-cgt-pru 2.3.3, ti-cgt-armllvm
5.1.1.LTS, SysConfig 1.28.1, MCU+ SDK AM243x 12.01.00.33. The open-pru
release notes list MCU+ SDK 11.1 - 12.0 as validated for AM243x, so 12.01 is
one minor version newer than validated.

Environment notes for building on this Linux PC:

- `imports.mak` default `MCU_PLUS_SDK_PATH` does not exist. The SDK path was
  passed on the make command line, with `BUILD_MCUPLUS=y`.
- CCS 21.0.1 has no `sysconfig_1.28.1/nodejs/node`. `SYSCFG_NODE` was pointed
  at `ccs/tools/node/node` on the command line.
- `imports.mak` was not modified.

## 8. Things the simulator could not show

- **Input synchroniser delay.** The simulator applies a GPI change in the
  same cycle. Silicon has a short synchroniser delay on the GPI path. It
  shifts every sample by the same few cycles, which the margins above
  absorb, but it is not measured.
- **Floating input.** The simulator drives the pin; silicon needs the
  pull-up (section 6).
- **Sender accuracy.** The generator in the simulator is cycle-exact. Real
  senders have a baud error, and this receiver only tolerates slow senders
  (section 9).
- **Edges and noise.** Slow edges, ringing or glitches on a 4 Mbaud line can
  look like a start bit or flip a sample. The firmware samples once per bit
  with no majority vote.

## 9. Open items

- [x] Run on LP-AM243 hardware with a 4 Mbaud sender and check DRAM0 in
      the CCS memory browser (2026-11-03, pass, section 7). The 2 V AWG high
      level was enough; no level shifter was needed.
- [x] Record the LP-AM243 board revision in `readme.md`: Rev A (2026-11-03).
- [x] RX pin location: BP.31 on header J4 (2026-11-02). The first version
      said "J4.10", taken from the J-header numbering in
      `examples/pru_eqep/readme.md`. The user pointed out that the LaunchPad
      has no J4 pin 10; its headers are labelled with BoosterPack pin
      numbers. BP.31 is confirmed by the LaunchPad pin mapping slide in
      `examples/LCD_interface/documents/PRU IO - LCD Interface.pptx`
      (PRU0 GPO0/1/2 = BP.33/32/31).
- [ ] Fast senders fail: 4.05 Mbaud (+1.25 %) already gives framing errors,
      because the stop bit of byte 11 is sampled at cycle 43 of 50 and the
      `qbeq store_bN` chain adds up to 11 cycles before the stop sample.
      A jump table or a shorter `BIT_TIME` path would centre the samples.
      The same limit is in the simulator source.
- [ ] No frame sync. Consider an idle-gap or header-byte check if the sender
      can start before the PRU.
- [ ] Optional: let the R5F print new frames on the UART console (poll the
      count at 0x0FFC).
- [ ] FIXME: `examples/readme.md` portability entries for am261x, am263px,
      am263x, am62x and am64x. The firmware uses only direct R31 GPI and c24
      DRAM, so am64x looks like `Yport`. Confirm before filling in.
- [ ] Open `example.syscfg` in the SysConfig GUI and save (section 6).
- [ ] `examples/makefile` and `examples/readme.md` also get an
      `electronica2026/...` entry on the i2c_io branch (PR #188). Whichever
      branch merges second needs a small conflict resolution there.

## 10. Files

```
examples/electronica2026/uart_rx_frame/uart_rx/
  makefile                                  am243x only, PRU0 + R5F targets
  readme.md                                 wiring, DRAM0 layout, run steps
  docs/porting_report.md                    this file
  firmware/main.asm                         ported PRU0 firmware
  firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/
    example.projectspec, makefile, makefile_projectspec, linker.cmd
  mcuplus/uart_rx_example.c                 R5F: load + start PRU0 only
  mcuplus/am243x-lp/r5fss0-0_freertos/
    example.syscfg                          200 MHz, PRU0 GPI2 + pull-up
    main.c
    ti-arm-clang/ example.projectspec, makefile, makefile_projectspec,
                  makefile_ccs_bootimage_gen, syscfg_c.rov.xs
examples/makefile                           project added to SUBDIRS
examples/readme.md                          project list + portability row
```
