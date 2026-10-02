# PRU I2C Master

## Introduction

This project implements an I2C master (controller) in PRU firmware. A PRU core
bit-bangs SCL and SDA with an IEP-timed state machine, and a host core (here an
R5F core running NORTOS) controls the bus through commands and data buffers in
the PRU data RAM. The R5F example in `mcuplus/` loads the firmware, writes a
few bytes to an I2C target, reads them back and prints PASS or FAIL together
with the response code of the PRU firmware for every transfer.

The project contains:

* `firmware/`: PRU assembly firmware (`pru_i2c_main.asm`), the memory interface
  header (`pru_i2c_interface.h`) and a CCS/makefile project for PRU0 and PRU1
  of ICSSM0.
* `mcuplus/`: R5F host code. `pru_i2c_example.c` is the common host logic,
  `mcuplus/am261x-lp/r5fss0-0_nortos/` holds `main.c`, `example.syscfg` and
  the CCS/makefile project.

# Supported Combinations

 Parameter      | Value
 ---------------|-----------
 ICSSM          | ICSSM0 - PRU0, PRU1 (firmware). The R5F example runs the firmware on ICSSM0 PRU1
 Toolchain      | pru-cgt (PRU firmware), ti-arm-clang (R5F)
 Board          | am261x-lp
 Example folder | examples/pru_i2c/

For all other devices and boards see the section "Supported processors
per-project" in the [examples readme](../readme.md). Only the AM261x-LP has
build infrastructure for this project; building for any other device prints
"does not have a build option" and builds nothing.

# Validated HW & SW

This example has **not been validated on hardware**. It has only been built.

 Item                       | Version
 ---------------------------|-----------
 Build: MCU+ SDK            | AM261x MCU+ SDK 10.02.00.15 (the version used by the OpenPRU CI)
 Build: PRU compiler        | ti-cgt-pru 2.3.3
 Build: R5F compiler        | ti-cgt-armllvm 4.0.3.LTS
 Build: SysConfig           | 1.26.2
 Hardware run               | not done (no board revision, I2C target or scope capture to report)

# How it works

## PRU <-> host memory interface

The host core talks to the firmware through the data RAM of the PRU core that
runs the firmware (PRU0 -> DMEM0, PRU1 -> DMEM1). The firmware reads and writes
this memory at local address 0. All offsets and bit definitions are in
[firmware/pru_i2c_interface.h](firmware/pru_i2c_interface.h), the host code
includes that header instead of repeating the values.

 Offset (DMEM)     | Size | Name                                   | Description
 ------------------|------|----------------------------------------|-------------
 0x0008            | u16  | `IRQ_COMMON_REGISTER_OFFSET`           | Bit `<instance id>` is **set by the PRU** when a response is ready and **cleared by the host** to acknowledge it. The PRU does not accept the next command until the bit is cleared.
 0x000C            | u16  | `I2C_BUS_FREQUENCY_OFFSET`             | Bus frequency: `ICSS_I2C_100KHZ_FREQ` (3), `ICSS_I2C_400KHZ_FREQ` (2), `ICSS_I2C_1MHZ_FREQ` (1). Any other value sends the firmware into an error loop.
 0x000E            | u16  | `I2C_BUS_FREQUENCY_OFFSET` + 2         | IEP compare increment for the selected frequency (`IEP_CMP_INCREMENT_VAL_400KHZ`, ...)
 0x0100            |      | `ICSS_I2C_INSTANCE0_ADDR`              | Start of the instance 0 block (offsets below are relative to it)
 + 0x08            | u16  | `ICSS_I2C_COMMAND_OFFSET`              | Response of the PRU (`COMMAND_SUCCESS` 0x0500, `ADDRESS_ACKNOWLDEGE_FAILED`, `DATA_ACKNOWLDEGE_FAILED`, `INVALID_COMMAND`, ...)
 + 0x0A            | u16  | `ICSS_I2C_COMMAND_OFFSET` + 2          | Command from the host (`ICSS_I2C_SETUP_CMD`, `ICSS_I2C_TX_CMD`, `ICSS_I2C_RX_CMD`, ...). The PRU clears it when it raises the response.
 + 0x94 / + 0x95   | u8   | `ICSS_I2C_BUF_OFFSET`                  | TX / RX FIFO size
 + 0x98            | u16  | `ICSS_I2C_CNT_OFFSET`                  | Number of bytes to transfer, 1..255
 + 0xA4            | u32  | `ICSS_I2C_CON_OFFSET`                  | Configuration: bit 15 module enable, bit 10 master (must be 1), bit 8 10-bit addressing, bit 5 SMBus burst, bit 4 NACK the last read byte, bit 1 send STOP, bit 0 send START
 + 0xAC            | u16  | `ICSS_I2C_SA_OFFSET`                   | 7-bit target address
 + 0xD8 / + 0xD9   | u8   | `ICSS_I2C_PRU_PIN_OFFSET`              | SCL / SDA pin: the bit number in R30 (output) and R31 (input)
 + 0xE4            | u8   | `ICSS_I2C_PRU_INST_ID_OFFSET`          | Instance id, the bit used in the IRQ register (use 0)
 + 0x100 (0x0200)  | 256 B| `ICSS_I2C_INSTANCE0_TX_MEM`            | TX buffer
 + 0x200 (0x0300)  | 256 B| `ICSS_I2C_INSTANCE0_RX_MEM`            | RX buffer

The PRU also raises INTC system event SRSR1 bit 20 (PRU0) or bit 21 (PRU1)
(`ICSS_I2C_INTC_PRU0_BIT_VAL`, `ICSS_I2C_INTC_PRU1_BIT_VAL`) when it sets the
IRQ bit. The R5F example does not use this interrupt, it polls the IRQ bit
in the data RAM with a timeout. An interrupt can be added with the
`PRUICSS_intcInit()` / `PRUICSS_registerIrqHandler()` APIs.

## Sequence

1. **Configure, then start the PRU.** The firmware reads the frequency words
   (0x0C) once while it boots, and `PRUICSS_loadFirmware()` starts the core.
   So the host stops the core, clears the data RAM, writes the frequency words
   and the instance 0 block (pins, instance id, FIFO sizes, `CON` with module
   enable, master, START, STOP and NACK), and only then loads the firmware.
2. **Setup.** Write `ICSS_I2C_SETUP_CMD`, wait for the IRQ bit, check that the
   response is `COMMAND_SUCCESS`, clear the IRQ bit.
3. **Transfers.** For each transfer write the target address and the byte
   count (and the data into the TX buffer for a write), then write
   `ICSS_I2C_TX_CMD` or `ICSS_I2C_RX_CMD`. Wait for the IRQ bit, read the
   response (and the RX buffer for a read), clear the IRQ bit.

The demo uses a generic EEPROM-style target (default address 0x50) that
takes a one byte pointer followed by data:

1. write `[pointer, d0, d1, d2, d3]`
2. wait 10 ms for the internal write cycle of the target
3. write `[pointer]` (STOP is sent, the target keeps its pointer)
4. read 4 bytes and compare them with what was written

## SDA direction

The firmware turns SDA into an input (to read the ACK bit or read data) by
setting the bit of the SDA pin in the `ICSSM0_PRUx_GPIO_OUT_CTRL` register of
MSS_CTRL (1 = output disabled; PRU0 at 0x50D00810, PRU1 at 0x50D00814) and
back to an output by clearing it. SysConfig sets that bit for every PRU GPIO
configured with "rx", which includes SDA. The R5F example therefore clears the
SDA bit of this register before it starts the PRU, so that the PRU can drive
the START condition. The example assumes the firmware uses the register of the
PRU core it runs on.

# Pin assignment (AM261x-LP)

 Signal | PRU GPIO (R30/R31 bit) | SoC pad            | BoosterPack | Direction
 -------|------------------------|--------------------|-------------|----------
 SCL    | PR0_PRU1_GPIO18 (18)   | GPIO120 (ball C19) | J1.4        | output (push-pull)
 SDA    | PR0_PRU1_GPIO19 (19)   | GPIO119 (ball C18) | J1.3        | input and output, internal pull-up enabled

The bit numbers are set with `PRU_I2C_SCL_PIN` and `PRU_I2C_SDA_PIN` in
`mcuplus/pru_i2c_example.c`. The pads are configured in
`mcuplus/am261x-lp/r5fss0-0_nortos/example.syscfg`. The firmware uses one
number per signal for both the R30 output bit and the R31 input bit, so the
GPO and GPI have to be the same `PR0_PRU1_GPIO<n>` pin. If you change a pin,
change it in both files.

Why PRU1: on the AM261x-LP none of the `PR0_PRU0_GPIOx` (ICSSM0 PRU0) signals
is routed to the BoosterPack headers, they are used by the on-board Ethernet
PHY 0 (RGMII2). `PR0_PRU1_GPIO17`, `GPIO18` and `GPIO19` are routed to
J2.17, J1.4 and J1.3. The firmware itself is built for both PRU0 and PRU1.
To run it on PRU0 set `PRU_I2C_PRU_CORE` to 0 and change the pinmux to pins
that are available on your board.

How the routing was checked: in the *AM261x LaunchPad User's Guide*
(SPRUJF1D, section 2.12 "Pinmux Mapping", table "Pinmux Options for J1"),
`PR0_PRU1_GPIO19` is the mode 0 function of J1.3 and `PR0_PRU1_GPIO18` the mode
0 function of J1.4. SysConfig 1.26.2 resolves them to GPIO119 and GPIO120. The
SDK Ethernet pinmux for the LP (`icss_dual_emac/am261x-lp/pruicss_pinmux.h`)
does not use these two PRU GPIOs. This was a document and tool check only. It
was **not** checked against the LP schematic or on a board. In the standard
BoosterPack mux mode (BP_MUX_SW_S1:S0 = 00) J1.3 and J1.4 are the BoosterPack
UART3 RX and TX lines, so do not plug in a BoosterPack that drives them.

# Steps to run the example

Generic build, load and run steps are not repeated here, see the
[Getting Started](../../docs/getting_started.md) documents. The project
specific steps are:

1. **Wire the target.** Connect an I2C target to J1.4 (SCL), J1.3 (SDA) and
   GND, and power it from 3.3 V. SDA needs a pull-up resistor (for example
   2.2 k to 4.7 k to 3.3 V). The example also enables the internal pull-up of
   the SDA pad, but that one is weak, use an external resistor. SCL is driven
   push-pull and needs no pull-up. The default target is an EEPROM-style device (for example a 24C02)
   at address 0x50. Change `PRU_I2C_TARGET_ADDR` (and, if needed,
   `PRU_I2C_TARGET_REG`, `PRU_I2C_TEST_LEN`) in `mcuplus/pru_i2c_example.c`
   for another device.
2. **Build.** From the repository root (with `DEVICE`, `BUILD_MCUPLUS` and the
   tool paths set in `imports.mak`) run
   `make DEVICE=am261x BUILD_MCUPLUS=y`, or build only this project with
   `make -C examples/pru_i2c all DEVICE=am261x BUILD_MCUPLUS=y`. The PRU
   firmware is built first, the R5F build includes the generated
   `pru1_load_bin.h`. The R5F image is
   `mcuplus/am261x-lp/r5fss0-0_nortos/ti-arm-clang/pru_i2c.release.out`
   (`.mcelf` and `.appimage` are generated next to it). With CCS, import the
   PRU0/PRU1 firmware projects and the R5F project from `examples/pru_i2c`,
   build the PRU projects first.
3. **Run on the R5F core** (r5fss0-0) and open the UART0 console (the log
   output of the example goes to UART0, like in the other AM261x examples).

## Expected output

With a responding target:

```
PRU I2C master example started
PRU core 1, SCL = PR0_PRU1_GPIO18, SDA = PR0_PRU1_GPIO19, target 0x50
Write 4 bytes at pointer 0x00: response 0x0500 (COMMAND_SUCCESS)
Write pointer 0x00: response 0x0500 (COMMAND_SUCCESS)
Read 4 bytes: response 0x0500 (COMMAND_SUCCESS)
  byte 0: wrote 0xA5, read 0xA5
  byte 1: wrote 0xB4, read 0xB4
  byte 2: wrote 0x87, read 0x87
  byte 3: wrote 0x96, read 0x96
PASS: PRU I2C write/read back
All tests have passed!!
```

With no target on the bus the first transfer fails with
`response 0x0508 (ADDRESS_ACKNOWLDEGE_FAILED (no ACK from target))` and the
example prints `Some tests have failed!!`.
A `TIME_OUT_ERROR (no response from PRU)` means the PRU did not answer within
100 ms, for example because the firmware is not running or the setup command
was not accepted.

# Known limitations

* **SCL is push-pull.** The firmware drives SCL high and low; it does not
  release it. Clock stretching by a target is not supported, and SCL must not
  be shared with other bus masters or driven by another device.
* **Only plain I2C read and write are implemented.** The commands for
  `ICSS_I2C_TX_CMD` (write) and `ICSS_I2C_RX_CMD` (read) work on a data
  count of 1..255 bytes. The SMBus commands (`ICSS_SMBUS_*`: quick command,
  send/receive byte, read/write byte/word, block read/write) are stubs in the
  firmware and must not be used.
* Master mode only, one instance (instance 0) per PRU core, no multi-master
  arbitration, 7-bit addressing in the example (the `CON` bit for 10-bit
  addressing exists in the firmware interface but is not used here).
* The START and STOP bits of `CON` are latched by the setup command. The
  example sets both, so each transfer is a complete START ... STOP transaction.
  The pointer-then-read sequence of the demo therefore uses two separate
  transactions and works only with targets that keep their pointer across a
  STOP (EEPROM-like). Targets that need a repeated START are not covered.
* **Bus frequency is nominal.** The timing constants in
  `pru_i2c_interface.h` are calculated for a 200 MHz ICSS clock. The
  SysConfig configuration of this example sets the ICSSM0 core clock to 225 MHz
  and the IEP clock to 250 MHz (`CONFIG_PRU_ICSS0_CORE_CLK_FREQ_HZ`,
  `CONFIG_PRU_ICSS0_IEP_CLK_FREQ_HZ` in the generated `ti_drivers_config.h`), so
  the real SCL frequency can differ from the selected 100 kHz / 400 kHz /
  1 MHz. Measure SCL on the bus before relying on the timing.
* The example polls the IRQ bit instead of using the PRU interrupt.
* The example is built for NORTOS on the r5fss0-0 core of the AM261x-LP only.
* Build-tested only, see "Validated HW & SW".
