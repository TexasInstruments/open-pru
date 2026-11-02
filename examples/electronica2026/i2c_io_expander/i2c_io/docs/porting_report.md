# Porting report: pru-simulator I2C TCA9538 running LED -> AM243x LP

Date: 2026-11-02
Branch: `dev_tley_electronica2026_i2c_io` (open-pru)
Source: `pru_simulator/source/i2c_tca9538_running_led.asm`
Target: `examples/electronica2026/i2c_io_expander/i2c_io` (AM243x LaunchPad,
ICSSG0 PRU0, R5F r5fss0-0 FreeRTOS host, CCS)

## 1. Prompt

The port was done by an AI agent (Claude Code) from this single prompt:

> Move the project from @source/i2c_tca9538_running_led.asm to a new dev
> branch on open-pru\examples\electronica2026\i2c_io_expander\. It should run
> on real target AM243 LP using CCS. Start with empty project on open-pru and
> copy it over to the i2c_io_expander folder. Only AM243 LP hardware variant
> is ok. Update sysconfig in this project to set correct PRU clock and PRU
> output pins. Only load PRU0 from ICSS_G0. We can drop all other PRUs from
> ARM loader. Rename the empty project in thie i2C.. folder to i2c_io.
> Document the porting effort including this prompt, model, effort, tokens
> and time used.

During the run the agent asked two questions (see section 4). The user
answered both; there was no other human input.

## 2. Model, effort, tokens and time

| Item                    | Value                                             |
| ----------------------- | ------------------------------------------------- |
| Agent                   | Claude Code (CLI), Linux PC                       |
| Model                   | Claude Opus 5.5 (`claude-opus-5-5`)               |
| Reasoning effort        | high (as recorded in the session transcript)      |
| Skills used             | `pru-assembly` (lint, simulate, timing workflow), open-pru `docs_ai/task_create_project.md` runbook |
| Start                   | 2026-11-02 11:06:37 CET (prompt submitted)        |
| End                     | 2026-11-02 11:23 CET (report written, before commit) |
| Wall-clock time         | 16 min 32 s                                       |
| of which user answering | 3 min 24 s (two questions)                        |
| Model API calls         | 81                                                |
| Output tokens           | 70,397                                            |
| Input tokens, uncached  | 168                                               |
| Input tokens, cache write | 193,718                                         |
| Input tokens, cache read  | 11,633,804                                      |

The token numbers are summed from the `usage` fields of the Claude Code
session transcript (`~/.claude/projects/.../<session>.jsonl`), counting only
API calls after the prompt was submitted. They were measured just before the
commit, so they leave out the final commit step (a few thousand tokens).
Cache-read tokens dominate. Every agent turn re-reads the cached conversation
(tool outputs, file contents), so they grow with the number of turns, not
with the amount of new work.

## 3. What was done, in order

1. Read open-pru `AGENTS.md`, `docs_ai/README.md`,
   `docs_ai/task_create_project.md` and `best_practices.md`.
2. Created branch `dev_tley_electronica2026_i2c_io` from `main` (58cbf1a8).
3. Copied `examples/empty` to
   `examples/electronica2026/i2c_io_expander/i2c_io`.
4. Removed every board except `am243x-lp`, and every core except
   `icss_g0_pru0_fw`.
5. Renamed `empty` to `i2c_io` everywhere: makefiles, both
   `example.projectspec`, `makefile_projectspec`, host source
   `empty_example.c` -> `i2c_io_example.c`, and `empty_example_main` ->
   `i2c_io_example_main`. Fixed the `OPEN_PRU_PATH` depth (6 -> 8 levels) and
   the project `imports.mak` include (2 -> 4 levels).
6. Rewrote the R5F loader (`mcuplus/i2c_io_example.c`) to load PRU0 only.
7. Ported the firmware into `firmware/main.asm` (section 5).
8. Updated `example.syscfg`: PRU core clock and PRU0 GPO0/GPO1 pin mux
   (section 6).
9. Verified: lint, makefile build, pru-simulator run of the built `.out`
   against the original source, repo-root build, and CCS headless import and
   build of both projects (section 7).
10. Added the project to `examples/makefile` and `examples/readme.md`, and
    wrote `readme.md` and this report.

## 4. Decisions made with the user

The simulator models R30 as open-drain: writing 1 releases the line, and SDA
can be read back on R31 bit 1. Silicon behaves differently. Two issues were
found and put to the user:

| Issue | Finding | User decision |
| ----- | ------- | ------------- |
| SDA direction | On AM243x a PRU pad is muxed either as `PRG0_PRU0_GPOn` (mode 0) or `PRG0_PRU0_GPIn` (mode 1), never both. GPO drives push-pull. Confirmed in SysConfig data: `GPI14` resolves to pad `PRG0_PRU0_GPO14` in `spi_loopback`. | **Drive SDA low during the ACK clock, no ACK check** |
| Bit rate | The TCA9538 datasheet specifies 400 kHz max (tLOW >= 1.3 us, tHIGH >= 0.6 us). The source runs at 1 Mbit/s (500 ns / 500 ns). | **Keep 1 Mbit/s** |

Alternatives that were offered:

- True open-drain by rewriting the SDA PADCONFIG mux mode from the PRU at run
  time. This needs CTRL_MMR pad unlock and has variable bus latency.
- A 1:1 port. This would cause SDA contention on every ACK, and the ACK read
  would be meaningless.

## 5. Firmware changes (simulator source -> `firmware/main.asm`)

| Change | Why | Timing impact |
| ------ | --- | ------------- |
| Added TI copyright header, `.retain`, `.retainrefs`, `.global main`, `.sect ".text"`, `main:` label | Required for clpru / CCS `.out` (`best_practices.md`) | none |
| `zero &r0, 120` before the first instruction | Register init (`best_practices.md`) | +1 cycle at start only |
| ACK clock: `set r30, r30, SDA_BIT` -> `clr r30, r30, SDA_BIT` | Push-pull SDA: drive low instead of releasing, so the master never fights the slave's ACK | 1 cycle -> 1 cycle |
| ACK sample: `and r20, r31, 2` -> `ldi r20, 0` | R31 bit 1 is not connected to the SDA pad while it is muxed as GPO | 1 cycle -> 1 cycle |
| `i2c_start/stop/write_byte` -> `FN_I2C_START/STOP/WRITE_BYTE` with NaturalDocs headers (pseudo code, PEAK cycles, registers modified) and per-line cycle comments | open-pru conventions | none |
| Register map and cycle budget block at the top | Skill and `best_practices.md` | none |

Unchanged: register allocation, `DELAY_COUNT = 48`, TCA9538 address 0x23,
init sequence, run loop and the DRAM0 0x0FFE error flag. The NACK paths are
kept but cannot trigger on silicon.

## 6. SysConfig changes (`mcuplus/am243x-lp/r5fss0-0_freertos/example.syscfg`)

```js
pruicss1.instance  = "ICSSG0";
pruicss1.coreClk   = 200000000;          // firmware timing assumes 200 MHz
pruicss1.AdditionalICSSSettings[0].PruGPIO.create(1);
...PruGPIO[0].PRU_ICSSG0_PRU.$assign      = "PRU_ICSSG0_PRU0";
...PruGPIO[0].PRU_ICSSG0_PRU.GPO0.$used   = true;   // SCL
...PruGPIO[0].PRU_ICSSG0_PRU.GPO0.$assign = "PRG0_PRU0_GPO0";
...PruGPIO[0].PRU_ICSSG0_PRU.GPO1.$used   = true;   // SDA
...PruGPIO[0].PRU_ICSSG0_PRU.GPO1.$assign = "PRG0_PRU0_GPO1";
```

200 MHz is also the SysConfig default. It is written explicitly to document
the dependency, but SysConfig GUI may drop it on save because it equals the
default. The generated code was checked:

- `ti_power_clock_config.c`: `TISCI_DEV_PRU_ICSSG0_CORE_CLK, 200000000`
- `ti_pinmux_config.c`: `PIN_PRG0_PRU0_GPO0` and `PIN_PRG0_PRU0_GPO1`, both
  `PIN_MODE(0) | PIN_PULL_DISABLE`

GP_MUX_SEL (direct GPIO mode) is not a SysConfig output. The R5F sets it
explicitly with `PRUICSS_setGpMuxSelect(..., PRUICSS_GP_MUX_SEL_MODE_GP)`
before `PRUICSS_loadFirmware()`.

**Deviation from the open-pru runbook.** `docs_ai/task_create_project.md`
says not to edit `example.syscfg` pin routing or PRUICSS settings by hand.
This edit was made because the prompt asked for it explicitly. Please open
the file once in the SysConfig GUI, check the clock and the two pins, and
save. The GUI also migrates the device to the new part-number scheme
(`AM2434`), which removes the build warning listed in section 7.

## 7. Verification

| Check | Result |
| ----- | ------ |
| Lint (`pru-assembly` `lint_pru.py --openpru`) | 0 errors, 0 warnings |
| Project makefile build, PRU + R5F (`make clean; make`, DEVICE=am243x) | exit 0 |
| PRU image size (`.map`) | 0x150 bytes (84 instructions) of 0x3000 IRAM; flag 0x0FFE inside 4 KB PRU0 DMEM |
| Repo-root build (`make -s`, DEVICE=am243x) | exit 0, `i2c_io` appimage generated |
| CCS 21.0.1 headless import + build, PRU project | 0 errors, 0 warnings |
| CCS 21.0.1 headless import + build, R5F project | 0 errors, 1 warning (SysConfig part-number migration notice, inherited from the `empty` syscfg) |
| pru-simulator: built `.out` + TCA9538 model at 0x23, 200 MHz, 70,000 cycles | Config reg = 0x00 at cycle 5342. Output reg sequence 0x01, 0x02, ... 0x80, 0x01, 0x02, 0x04, identical to the original source. No halt. DRAM0[0x0FFE] = 0 |
| pru-simulator: SCL edges of the port vs the original source | 671 / 671 SCL edges identical in cycle and level (after the 1-cycle `zero` offset) |
| Phase widths (port, simulated) | SCL high 101 cyc (data) / 99 cyc (ACK); SCL low 100-101 cyc (data) / 99 cyc (ACK); 105-107 cyc between bytes (call overhead) |
| Transaction period (START to START) | 5841-5845 cycles = 29.2 us |
| Hardware run on LP-AM243 | **not done** (no board attached to this PC) |

Tool versions used: CCS 21.0.1 (`ccs2101`), ti-cgt-pru 2.3.3, ti-cgt-armllvm
5.1.1.LTS, SysConfig 1.28.1, MCU+ SDK AM243x 12.01.00.33. The open-pru
release notes list MCU+ SDK 11.1 - 12.0 as validated for AM243x, so 12.01 is
one minor version newer than validated.

Environment notes for building on this Linux PC:

- `imports.mak` default `MCU_PLUS_SDK_PATH` (`~/ti/mcu_plus_sdk`) does not
  exist. The SDK path was passed on the make command line.
- CCS 21.0.1 has no `sysconfig_1.28.1/nodejs/node`. `SYSCFG_NODE` was pointed
  at `ccs/tools/node/node` on the command line.
- `imports.mak` was not modified.

## 8. Things the simulator could not show

- Open-drain versus push-pull pads (section 4).
- The R31 read-back of a GPO pad: on silicon it reads the unmuxed GPI path.
- TCA9538 timing limits: the simulator model accepts 1 Mbit/s.
- The visible LED effect. One Output Port write takes 29.2 us, so the full
  8-step cycle takes 234 us. On real LEDs all eight look dimly on, and there
  is no visible running light. A delay of about 100 ms per step in `run_loop`
  would make it visible.
- The ACK clock low phase is 99 cycles (495 ns). This is 5 ns below the I2C
  Fast-mode Plus tLOW minimum (500 ns). The same timing is in the simulator
  source.

## 9. Open items

- [ ] FIXME: run on LP-AM243 hardware, record the board revision in
      `readme.md`, and capture SCL/SDA with a logic analyzer.
- [ ] FIXME: check the BoosterPack J-header positions (J4.8 / J4.9) against
      the board silkscreen. The BP pin numbers 33/32 come from the
      LCD_interface pin-mapping slide.
- [ ] FIXME: `examples/readme.md` portability entries for am261x, am263px,
      am263x, am62x and am64x. The firmware uses only direct R30 GPO and c24
      DRAM, so am64x looks like `Yport`. Confirm before filling in.
- [ ] Decide whether to slow the run loop down for a visible LED demo.
- [ ] Decide whether to lower the bit rate to 400 kHz (DELAY_COUNT >= 130)
      for TCA9538 datasheet compliance.
- [ ] Open `example.syscfg` in the SysConfig GUI and save (section 6).

## 10. Files

```
examples/electronica2026/i2c_io_expander/i2c_io/
  makefile                                  am243x only, PRU0 + R5F targets
  readme.md                                 wiring, run and validate steps
  docs/porting_report.md                    this file
  firmware/main.asm                         ported PRU0 firmware
  firmware/am243x-lp/icss_g0_pru0_fw/ti-pru-cgt/
    example.projectspec, makefile, makefile_projectspec, linker.cmd
  mcuplus/i2c_io_example.c                  R5F: load + start PRU0 only
  mcuplus/am243x-lp/r5fss0-0_freertos/
    example.syscfg                          200 MHz, PRU0 GPO0/GPO1 pin mux
    main.c
    ti-arm-clang/ example.projectspec, makefile, makefile_projectspec,
                  makefile_ccs_bootimage_gen, syscfg_c.rov.xs
examples/makefile                           project added to SUBDIRS
examples/readme.md                          project list + portability row
```
