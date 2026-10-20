# Single core JPEG decoder

See the [JPEG decoder overview](../README.md) for format support, memory layout, measured results, and build instructions.

This firmware runs entropy decoding, dequantization, inverse DCT, and sample output sequentially on PRU0. The host selects it with `JPEG_MODE=single` and loads the generated PRU0 image. PRU0 advances the output pointer at local DRAM offset `0x1870` by `64` bytes per block and sets the completion word when the scan is done. The quantization tables begin at local DRAM offset `0x1900`.

## Supported Combinations

- AM243x LP, ICSSG0 PRU0 (250 MHz)
- AM261x LP, ICSSM0 PRU0 (225 MHz)

## Validated HW & SW

Measured on LP-AM243 (rev E1) and LP-AM261 (rev E2) with the host application in [`host/`](../host/), all bundled images bit-exact; results are in the [overview](../README.md). Built with TI PRU CGT 2.3.3, TI Arm Clang 4.0.5.LTS, MCU+ SDK 12.01.00.33 with SysConfig 1.28.0 (AM243x) and MCU+ SDK 11.00.00.29 with SysConfig 1.26.0 (AM261x).
