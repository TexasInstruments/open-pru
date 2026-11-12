# Porting report: pru-simulator UART print -> AM243x LP

Date: 2026-11-03
Branch: `dev_tley_electronica2026_uart_print` (open-pru)
Source: `pru_simulator/source/uart_print.asm`
Target: `examples/electronica2026/uart_print/uart_print` (AM243x LaunchPad,
ICSSG0 PRU0, R5F r5fss0-0 FreeRTOS host, CCS)

## 1. Prompt

The port was done by an AI agent (Claude Code) from this single prompt:

> convert the uart_print to open-pru like the uart_rx_11frame project

The agent took "like the uart_rx_11frame project" to mean the same flow and
deliverables as the uart_rx port (`../../uart_rx_frame/uart_rx`, branch
`dev_tley_electronica2026_uart_rx`, PR #189): a new dev branch from
`origin/main`, AM243x LP only, ICSSG0 PRU0 only, a 200 MHz PRU clock and the
pin set in SysConfig, an R5F loader for PRU0 only, a readme and this report.
The agent asked four questions before changing any files (section 4). There
was no other human input.

## 2. Model, effort, tokens and time

| Item                    | Value                                             |
| ----------------------- | ------------------------------------------------- |
| Agent                   | Claude Code 2.1.316 (CLI), Linux PC               |
| Model                   | Claude Opus 5.5 (`claude-opus-5-5`)               |
| Reasoning effort        | high (as recorded in the session transcript)      |
| Skills used             | `pru-assembly` (lint, simulate, timing workflow); the uart_rx port (`../../uart_rx_frame/uart_rx/docs/porting_report.md`) as the step-by-step template |
| Start                   | 2026-11-03 11:28:57 CET (prompt submitted)        |
| End                     | 2026-11-03 11:40:44 CET (report finished, before commit) |
| Wall-clock time         | 11 min 47 s                                       |
| of which user answering | 2 min 34 s (four questions, one dialog)           |
| Model API calls         | 49                                                |
| Output tokens           | 47,502                                            |
| Input tokens, uncached  | 100                                               |
| Input tokens, cache write | 130,271                                         |
| Input tokens, cache read  | 5,031,687                                       |

The token numbers are summed from the `usage` fields of the Claude Code
session transcript (`~/.claude/projects/.../<session>.jsonl`), counting only
API calls after the prompt was submitted. They were measured just before the
commit, so they leave out the commit step itself. Compared with the uart_rx
port (12 min 04 s, 52 calls, 57,723 output tokens), the build files were
copied from uart_rx with only names changed, so most of the time went into
the logic_scope conflict check (section 4) and the timing runs (section 7).

## 3. What was done, in order

1. Created branch `dev_tley_electronica2026_uart_print` from `origin/main`
   (58cbf1a8). It is independent of the i2c_io (PR #188) and uart_rx
   (PR #189) branches.
2. Copied the tracked files of the uart_rx project from its branch to
   `examples/electronica2026/uart_print/uart_print`, leaving out its `test/`
   folder. The uart_rx project is itself `examples/empty` reduced to the
   AM243x LP PRU0 and R5F projects, so this is the same starting point with
   the uart_rx build fixes already in (`OPEN_PRU_PATH` depth, include paths
   of the unused cores removed). The folder depth is the same, so no paths
   had to change except the project folder name.
3. Renamed `uart_rx` to `uart_print` everywhere: both `example.projectspec`,
   both `makefile_projectspec`, the PRU and R5F makefiles, the top makefile,
   host source `uart_rx_example.c` -> `uart_print_example.c` and
   `uart_rx_example_main` -> `uart_print_example_main`. Changed the project
   titles and descriptions.
4. Updated the R5F loader comments and console message. The code is
   unchanged: clear PRU0 DRAM, set GP_MUX_SEL to GP mode, load and start
   PRU0 only.
5. Ported the firmware into `firmware/main.asm` (section 5).
6. Updated `example.syscfg`: the PRU0 pin is now GPO0 as an output
   (section 6).
7. Verified: lint, makefile build, CCS headless import and build of both
   projects, a dry run of `examples/makefile`, and pru-simulator runs of the
   built `.out` against the original source (section 7).
8. Added the project to `examples/makefile` and `examples/readme.md`, and
   wrote `readme.md` and this report.

## 4. Decisions made with the user

The agent first read open-pru's `examples/logic_scope`, because the earlier
session notes said the port should be tested with "logicscope". That example
would have conflicted with the port in three ways: it uses ICSSG0 PRU0 and
RTU0, its PRU0 `main.c` sets all eight PRG0_PRU0_GPO0-7 pads to inputs, and
it sets the ICSSG0 core clock to 333 MHz. The questions were built around
that.

| Question | Options offered | User decision |
| -------- | --------------- | ------------- |
| Core and TX pin | ICSSG0 PRU1, TX on BP.11, jumper to logic_scope's trigger input BP.44 (recommended); or ICSSG0 PRU0 like uart_rx | **PRU0.** "logic_scope is not used but real picoscope." TX = PRG0_PRU0_GPO0 (BP.33) |
| PRU clock and baud rate | 333 MHz, to match logic_scope (recommended); or 200 MHz with the source's `BAUD_COUNT` | **200 MHz, 115200 baud** |
| Send once or repeat | Repeat with an idle gap (recommended); or once, then halt, as in the source | **Repeat with an idle gap** |
| Folder | `uart_print/uart_print` or `uart_tx_string/uart_print` | **`uart_print/uart_print`** |

With the PicoScope instead of logic_scope, the clock recommendation no
longer applied. 200 MHz keeps the source's `BAUD_COUNT` and matches the
other two electronica demos.

## 5. Firmware changes (simulator source -> `firmware/main.asm`)

| Change | Why | Timing impact |
| ------ | --- | ------------- |
| Added SPDX/TI copyright header, `.retain`, `.retainrefs`, `.global main`, `.sect ".text"`, `main:` label | Required for clpru / CCS `.out` (`best_practices.md`) | none |
| `zero &r0, 120` at start | Register init (`best_practices.md`). Also a hardware fix, see below | +1 cycle at start only |
| `qbeq done, r0, 0` -> `qbeq idle_gap, r0.b0, 0` | Hardware fix, see below | none, same instruction |
| `done: halt` -> `idle_gap:` loop of `IDLE_GAP` = 100,000 iterations (`ldi32` + `sub`/`qbne`), then `qba send_string`, which reloads `r4 = 0` | User decision: repeat (section 4). The string is not rewritten, so a string changed in DRAM0 from CCS is sent on the next pass | new path after the string only; idle gap 200,017 cycles = 1.000 ms |
| Label `send_string:` on the existing second `ldi r4, 0` | Jump target for the repeat | none |
| A `halt` after the repeat loop | Kept as the program's end marker; never reached | none |
| Rewrote the header: behaviour, DRAM0 layout, hardware requirements, register map, measured cycle budget; per-line cycle comments on the bit path | Skill and `best_practices.md` | none |

**Hardware fix: string terminator.** The source loads each byte with
`lbco &r0, c24, r4.b0, 1`, which writes only `r0.b0`, and then tests the
whole register with `qbeq done, r0, 0`. That only works if `r0.b1-b3` are
0. In the simulator they are, because registers start at 0. After a reset
on silicon, register contents are not guaranteed. Each byte shifts r0 right
by 8 bits, so any leftover bits in `r0.b1-b3` are gone after three
characters. With `Hello 2026` the source would therefore still have worked.
A string of fewer than three characters (for example written from CCS
before the first pass) could miss its terminator, and the firmware would
send DRAM0 contents until the leftover bits had been shifted out. The port
clears r0-r29 at start and compares only `r0.b0`. Either change alone fixes
it. Both are in, at a cost of one cycle at start.

Unchanged: `BAUD_COUNT = 868`, `TX_BIT = 0`, the string constants, the
register use, and every instruction of the start, data and stop bit paths.
The bit timing matches the source cycle for cycle (section 7).

## 6. SysConfig changes (`mcuplus/am243x-lp/r5fss0-0_freertos/example.syscfg`)

Compared with the uart_rx project, the four GPI2 input lines (input, pull-up)
were replaced by GPO0 as an output:

```js
pruicss1.instance  = "ICSSG0";
pruicss1.coreClk   = 200000000;          // firmware timing assumes 200 MHz
pruicss1.AdditionalICSSSettings[0].PruGPIO.create(1);
...PruGPIO[0].PRU_ICSSG0_PRU.$assign      = "PRU_ICSSG0_PRU0";
...PruGPIO[0].PRU_ICSSG0_PRU.GPO0.$used   = true;   // TX
...PruGPIO[0].PRU_ICSSG0_PRU.GPO0.$assign = "PRG0_PRU0_GPO0";
```

These are the same two GPO0 lines as in the i2c_io project's SysConfig, which
ran on the LP-AM243 on 2026-11-02 (i2c_io also has GPO1). No pull resistor is
set: the PRU drives the pin push-pull at all times, and the firmware sets it
high as its second instruction.

The generated code was checked:

- `ti_power_clock_config.c`: `TISCI_DEV_PRU_ICSSG0_CORE_CLK, 200000000`
- `ti_pinmux_config.c`: `PIN_PRG0_PRU0_GPO0`, `PIN_MODE(0) | PIN_PULL_DISABLE`

GP_MUX_SEL (direct GPIO mode) is not a SysConfig output. The R5F sets it
with `PRUICSS_setGpMuxSelect(..., PRUICSS_GP_MUX_SEL_MODE_GP)` before
`PRUICSS_loadFirmware()`, as in uart_rx.

**Deviation from the open-pru runbook.** `docs_ai/task_create_project.md`
says not to edit `example.syscfg` pin routing or PRUICSS settings by hand.
This edit was made the same way as for i2c_io and uart_rx, to set the pin
the port needs. Please open the file once in the SysConfig GUI, check the
clock and the pin, and save. The GUI also migrates the device to the new
part-number scheme (`AM2434`), which removes the build warning listed in
section 7.

## 7. Verification

| Check | Result |
| ----- | ------ |
| Lint (`pru-assembly` `lint_pru.py --openpru`) | 0 errors, 0 warnings (source: also 0 / 0) |
| Project makefile build, PRU + R5F (DEVICE=am243x, BUILD_MCUPLUS=y) | exit 0, `uart_print.release.appimage.hs_fs` generated |
| PRU image size (`.map`) | 0xA0 bytes (40 instructions) of 0x3000 IRAM |
| `examples/makefile` (dry run, `make -n`) | enters `electronica2026/uart_print/uart_print` |
| CCS 21.0.1 headless import + build, PRU project | 0 errors, 0 warnings |
| CCS 21.0.1 headless import + build, R5F project | 0 errors, 1 warning (SysConfig part-number migration notice, inherited from the `empty` syscfg) |
| pru-simulator, built `.out`, DRAM0 after 20 instructions | `48 65 6C 6C 6F 20 32 30 32 36 00 00` = `Hello 2026\0\0` |
| pru-simulator, TX edges of the first string: source vs built `.out` | 57 edge intervals, identical |
| pru-simulator, built `.out`, 200 TX edges (3.4 passes), decoded at 115200 baud with mid-bit sampling | `Hello 2026` three times, then the start of a fourth pass that was cut off by the edge limit; all stop bits high |
| pru-simulator, bit timing of the built `.out` | start 1739, data 1742 (1) / 1743 (0), bit 7 1741 / 1742, stop 1745 cycles; byte 17,425 cycles = 114,780 baud (-0.37 %) |
| pru-simulator, repeat | idle gap 200,017 cycles (1.000 ms) after the stop bit; repeat period 374,245 cycles = 1.871 ms, identical for every pass |
| Hardware run on LP-AM243 Rev A, 2026-11-03: CCS load of both projects, PicoScope on BP.33 | pass (reported by the user) |

The simulator counts cycles, so the PRU clock in the local `memory.cfg` does
not affect these numbers. The edge runs used the pru-assembly skill's
`simulate.py edges` on the built `.out`. The DRAM0 check called
`Simulator.load_elf()` and `memory_read()` directly. The decoding script is
not part of this project.

**Why 0.37 % slow.** Each bit is 868 x 2 = 1736 delay cycles plus 3 to 9
cycles of bit handling, which the source does not subtract. The ideal bit
time at 200 MHz is 1736.1 cycles. A UART receiver typically tolerates about
2 %, so this was left as in the source. `BAUD_COUNT = 865` would bring the
error to about 0.03 %.

Tool versions used: CCS 21.0.1 (`ccs2101`), ti-cgt-pru 2.3.3, ti-cgt-armllvm
5.1.1.LTS, SysConfig 1.28.1, MCU+ SDK AM243x 12.01.00.33. The open-pru
release notes list MCU+ SDK 11.1 - 12.0 as validated for AM243x, so 12.01 is
one minor version newer than validated.

Environment notes for building on this Linux PC (same as uart_rx):

- `imports.mak` default `MCU_PLUS_SDK_PATH` does not exist. The SDK path was
  passed on the make command line, with `BUILD_MCUPLUS=y`.
- CCS 21.0.1 has no `sysconfig_1.28.1/nodejs/node`. `SYSCFG_NODE` was pointed
  at `ccs/tools/node/node` on the command line.
- `imports.mak` was not modified.

## 8. Things the simulator could not show

- **Register contents after reset.** The simulator starts with all
  registers at 0, which hid the short-string terminator bug (section 5).
- **Pin edges.** The simulator changes the pin in the same cycle. On silicon
  the GPO path and pad add a fixed delay and finite rise and fall times. At
  8.7 us per bit this does not matter.
- **Other ICSSG0 users.** The baud rate is only right while ICSSG0 runs at
  200 MHz. Firmware that changes the ICSSG0 clock (for example
  `examples/logic_scope`, 333 MHz) changes it for this core too.

## 9. Open items

- [x] Run on LP-AM243 hardware with the PicoScope (2026-11-03, pass,
      section 7). Board: LP-AM243 Rev A, recorded in `readme.md`.
- [ ] Save the PicoScope settings as a `.psdata` file. The settings in the
      readme are suggestions; the ones used in the hardware run were not
      recorded.
- [ ] `examples/readme.md` portability entries: set to `Yport` for am261x,
      am263px, am263x, am62x and am64x, like uart_rx (the firmware uses only
      direct R30 GPO and c24 DRAM). Please confirm.
- [ ] Open `example.syscfg` in the SysConfig GUI and save (section 6).
- [ ] Optional: `BAUD_COUNT = 865` for 0.03 % baud error (section 7).
- [ ] `examples/makefile` and `examples/readme.md` also get an
      `electronica2026/...` entry on the i2c_io (PR #188) and uart_rx
      (PR #189) branches. Each branch that merges after the first needs a
      small conflict resolution there.

## 10. Files

```
examples/electronica2026/uart_print/uart_print/
  makefile                                  am243x only, PRU0 + R5F targets
  readme.md                                 wiring, DRAM0 layout, run steps
  docs/porting_report.md                    this file
  firmware/main.asm                         ported PRU0 firmware
  firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/
    example.projectspec, makefile, makefile_projectspec, linker.cmd
  mcuplus/uart_print_example.c              R5F: load + start PRU0 only
  mcuplus/am243x-lp/r5fss0-0_freertos/
    example.syscfg                          200 MHz, PRU0 GPO0 output
    main.c
    ti-arm-clang/ example.projectspec, makefile, makefile_projectspec,
                  makefile_ccs_bootimage_gen, syscfg_c.rov.xs
examples/makefile                           project added to SUBDIRS
examples/readme.md                          project list + portability row
```
