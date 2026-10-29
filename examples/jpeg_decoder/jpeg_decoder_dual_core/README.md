# Dual core JPEG decoder

See the [JPEG decoder overview](../README.md) for format support, memory layout, measured results, and build instructions.

PRU0 decodes the entropy scan and sends coefficient blocks to PRU1 through the scratchpad XFR slot protocol in [`ipc_xfr.inc`](../common/ipc_xfr.inc). PRU1 dequantizes, runs the inverse DCT, and writes sample blocks. The host selects this firmware with `JPEG_MODE=dual`, loads both generated images, starts PRU1 before PRU0, and waits for both completion states.

## Supported Combinations

- AM243x LP, ICSSG0 PRU0 and PRU1 (250 MHz)
- AM261x LP, ICSSM0 PRU0 and PRU1 (225 MHz)

## Validated HW & SW

Measured on LP-AM243 (rev E1) and LP-AM261 (rev E2) with the host application in [`host/`](../host/), all bundled images bit-exact; results are in the [overview](../README.md). Built with TI PRU CGT 2.3.3, TI Arm Clang 4.0.5.LTS, MCU+ SDK 12.01.00.33 with SysConfig 1.28.0 (AM243x) and MCU+ SDK 11.00.00.29 with SysConfig 1.26.0 (AM261x).
