# PRU direct GPIO vs SysConfig pad direction (AM26x PRU-ICSSM)

This note answers
[TexasInstruments/open-pru#157](https://github.com/TexasInstruments/open-pru/issues/157)
for the AM26x PRU-ICSSM devices that
[`gpio/gpio_toggle`](gpio_toggle/readme.md) already builds: AM263Px, AM261x,
and AM263x. It is documentation only. It does not add firmware.

The recurring question (E2E thread 1666087, *AM263P4 PRU GPIO direction
switching during runtime*) mixes three different mechanisms. Keep them
separate. Each claim below points at a file in this repository, or says
that this repository does not document the behavior.

## PRU direct GPIO: R30 is output, R31 is input

On the fast path used by most OpenPRU examples, the PRU core uses two fixed
registers:

| Register | Role in this repository |
| -------- | ----------------------- |
| R30 | General-purpose output. Firmware writes a bit to drive the matching GPO. |
| R31 | General-purpose input, and also the INTC system-event interface. Firmware reads a bit to sample the matching GPI. |

That split is stated in the
[PRU Assembly Instruction Cheat Sheet](../../docs/PRU%20Assembly%20Instruction%20Cheat%20Sheet.md)
(R30 = GPIO output / direct output pins, R31 = GPIO input / system event
flags), in [best practices](../../best_practices.md) (R30 GPO, R31 GPI), and
in the migration guide: R30 is the interface to the GPO pins, and R31 is the
interface to the GPI pins and INTC status
([§2.5](../../docs_ai/reference/pru_subsystem_migration_guide/pru_subsystem_migration_guide.md)).

[`gpio/gpio_toggle/firmware/main.asm`](gpio_toggle/firmware/main.asm) is the
AM26x illustration. The file comments that R30 and R31 are the output and
input registers, then the loop drives bit 4:

```
ldi     r30.b0, 0x10    ; GPIO pin 4 high
ldi     r30.b0, 0x00    ; GPIO pin 4 low
```

The project readme maps that bit onto the signal selected in SysConfig:
`PR0_PRU0_GPIO4` on AM263x and AM263Px, `PR1_PRU1_GPIO4` on AM261x. The bit
index and the `PRU_GPIOn` signal index are the same number. The lab does not
read R31.

An input sample, when one is required, is a read of R31. Best practices
branch on `r31` with `qbbc`. That read does not appear in the toggle
firmware.

R31 has a second use on the same register. The INTC lab posts a system event
by writing R31 bits 5:0
([`academy/intc/intc_mcu/firmware/main.asm`](../intc/intc_mcu/firmware/main.asm)).
That write is the interrupt strobe. It does not drive a pad, and it does not
change pad direction.

## SysConfig pad direction is chosen at configuration time

Which ball carries a PRU GPIO signal, and the pad settings for that ball, are
recorded in the MCU+ `example.syscfg` and applied when pinmux runs. A read or
write of R30 or R31 shows that firmware is using the PRU I/O interface. It
does not show which core programmed the pinmux. On MCU+ projects the pinmux
lives in `example.syscfg`
([porting runbook, signal I/O](../../docs_ai/task_port_project.md)).

What the toggle lab actually stores in syscfg:

* AM263Px LaunchPad,
  [`example.syscfg`](gpio_toggle/am263px-lp/r5fss0-0_freertos/example.syscfg),
  enables `PR0_PRU0_GPIO4`. The firmware drives that signal from R30 bit 4.
  The same file also sets `.rx = true` on `PR0_PRU0_GPIO0`,
  `PR0_PRU0_GPIO1`, and `PR0_PRU0_GPIO8`.
* AM263x LaunchPad,
  [`example.syscfg`](gpio_toggle/am263x-lp/r5fss0-0_freertos/example.syscfg),
  enables `PR0_PRU0_GPIO4` for the PRU. A separate SoC GPIO instance in that
  file sets `pinDir = "OUTPUT"`. `pinDir` belongs to that SoC GPIO instance.
  The PRU loop never writes it.
* AM261x LaunchPad,
  [`example.syscfg`](gpio_toggle/am261x-lp/r5fss0-0_freertos/example.syscfg),
  assigns `PR1_PRU1_GPIO4` and marks it used. The shared firmware still
  writes R30 bit 4.

`.rx` and `pinDir` are SysConfig properties baked into the generated pinmux.
`main.asm` does not mention them. Changing them means editing syscfg,
regenerating pinmux, and applying that configuration again.

This tree does not define what `.rx = true` encodes on an AM26x PRU GPIO
signal. Elsewhere in the tree the same property is set on both GPI and GPO
signals (see `examples/pru_emif`). Do not read the AM263Px `.rx = true` lines
as a documented input-versus-output switch. The fact this note uses is
narrower: the property is config-time text, and the R30 toggle loop does not
update it. The one explicit direction string in the lab is the SoC GPIO
`pinDir = "OUTPUT"` above.

`GPCFG0` / `GPCFG1` are another register, in the PRU-ICSS CFG space. The
AM263Px, AM261x, and AM263x headers name `PRU0_GPI_MODE` and `PRU0_GPO_MODE`
(and the PRU1 pair) inside `GPCFG0_REG` / `GPCFG1_REG`. See
[`source/include/am263px/cslr_icss_m.h`](../../source/include/am263px/cslr_icss_m.h)
(the same field names exist in the am261x and am263x headers). The headers
give mask, shift, and reset value. They do not give a mode table. The
migration guide's mode table (direct input, direct output, parallel capture,
shift, and the other rows in §2.5) describes GPI/GPO modes across devices. It
does not list AM263Px or AM261x, and it does not say those `GPCFG` fields are
a per-pin pad-direction register. The GPIO toggle firmware does not write
`GPCFG`.

For the public AM263Px description of direct input (GPI data into R31) and
direct output (R30 onto the GPO pins), see
[PRU-ICSSM Input/Output Modes](https://software-dl.ti.com/processor-industrial-sw/esd/motor_control_sdk/am263px/latest/docs/api_guide_am263px/DEVELOPER_GUIDE_PRUICSSM_IO_MODES.html)
in the AM263Px Motor Control SDK. That page also says device-level pinmux
takes priority over the PRU-ICSS internal pinmux. This note does not restate
mode encodings past that page and the in-tree bitfield names.

## What "changing GPIO direction at runtime" means here

**SoC GPIO direction registers can be written at runtime.** The shared macros
in
[`source/firmware/common/icss_gpio_cntrl_macros.inc`](../../source/firmware/common/icss_gpio_cntrl_macros.inc)
write a GPIO instance's `GPIO_DIRxx` registers. The macro comments define
`0` as output and `1` as input. That is the memory-mapped SoC GPIO module.
`academy/gpio/gpio_toggle` does not call those macros. Writing `GPIO_DIR`
does not retarget R30 or R31.

**On the PRU direct-GPIO path used by the toggle lab, a runtime direction
change is not what the firmware does:**

* The lab drives a pin by writing R30. The register stays the output
  register for the life of the program. Reading R31 samples an input or
  returns INTC status. It does not reconfigure the pad.
* Writing R31 to post an INTC event does not change pad direction.
* SysConfig `pinDir` and `.rx` are applied with pinmux. The PRU toggle loop
  does not write them.
* PRU_ICSSG direct output can add bidirectional support through CTRLMMR
  registers. The migration guide states that as a PRU_ICSSG upgrade relative
  to PRU-ICSS. The published walkthrough is the AM64x/AM243x FAQ
  [How to enable switching between GPO and GPI](https://e2e.ti.com/support/processors-group/processors/f/processors-forum/1386144/faq-how-to-enable-switching-capability-between-gpo-and-gpi-operation-for-pru-gpio).
  AM263Px, AM261x, and AM263x in this tree are PRU-ICSSM. This repository
  does not document that CTRLMMR sequence for them, and the GPIO lab does
  not implement it. Leave that FAQ on PRU_ICSSG.

## If one wire must both drive and be sampled

This repository's pattern is two different operations, each with the pad
configuration chosen in SysConfig before the PRU runs:

* Drive: enable the PRU GPIO signal in SysConfig and write the matching R30
  bit, as `gpio_toggle` does for GPIO4.
* Sample: read the matching R31 bit, as in the best-practices `qbbc`
  sequence. The ball still has to be pinmuxed onto that PRU input. The
  toggle lab does not contain that read. Its syscfg shows which file owns
  the pin choice.

The AM263Px LaunchPad syscfg enables GPIO4 and three other PRU GPIO signals
in the same configuration, while `main.asm` only writes R30 bit 4. That shows
several signals can be enabled together. It does not, by itself, show a
runtime direction change on one ball.

This repository does not document an open-drain pad mode, or a validated
sequence that turns one AM26x PRU-ICSSM ball from drive to sample at runtime.
No such sequence is given here. A design that needs one ball to alternate
has to come from the device TRM and the pinmux for that ball, then be proven
on hardware. It is outside what R30 and R31 select.
