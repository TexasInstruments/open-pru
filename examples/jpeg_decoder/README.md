# JPEG decoder on PRU

## Overview

**Tested on hardware.** Every frame result in the Measured results section was measured on TI LaunchPads with the supplied PRU firmware: LP-AM243 (ICSSG, PRU at 250 MHz for the bundled images and 333.3 MHz measured for the separate corpus) and LP-AM261 (ICSSM, PRU at 225 MHz), in both single-core and dual-core configurations. Every decoded image matched the expected sample-block CRC, which was generated from libjpeg-turbo output, so the PRU output is bit-exact with libjpeg-turbo for every measured image.

This example decodes baseline JPEG entropy scans to planar sample blocks on TI PRU cores. It supports grayscale, 4:2:0 and 4:4:4 sampling, including restart markers. Progressive JPEG, arithmetic coding, 4:2:2 sampling, non-interleaved scans, 16-bit quantization tables and 12-bit sample precision are unsupported. The R5F host parses headers, builds Huffman and quantization tables, loads the scan, starts the PRU cores, polls completion, and checks the block stream CRC. The R5F libjpeg-turbo comparison below is an earlier independent measurement; that decoder is not included here.

The host rejects frames with no blocks or more than `MAX_BLOCKS=4000`, more than `MAX_SEGMENTS=256` restart segments, output exceeding its MSRAM window, or a scan larger than the selected Shared RAM or MSRAM window. A scan starts in ICSS Shared RAM at `0x16500` if it fits; otherwise the entire scan goes to MSRAM. ICSSM Shared RAM ends at `0x18000`, so its local scan capacity is 6912 bytes. This avoids overflowing the smaller ICSSM Shared RAM; there is no scan refill protocol. These are implementation limits, not a promise that every JPEG geometry below the block limit is supported. Source: [host source](host/mcuplus/jpeg_host.c) and the firmware linker files.

## Architecture

The pipeline is entropy decode, dequantization, inverse DCT, then sample output. [Single core](jpeg_decoder_single_core/README.md) runs the whole pipeline on PRU0. [Dual core](jpeg_decoder_dual_core/README.md) assigns entropy decoding to PRU0 and the remaining stages to PRU1. PRU0 hands coefficient blocks to PRU1 with the scratchpad XFR slot protocol in [ipc_xfr.inc](common/ipc_xfr.inc); PRU1 acknowledges consumed slots. The host starts PRU1 and waits for its ready flag, written after scratchpad initialization, before enabling PRU0. Only the host writes the PRU CONTROL registers. It also clears scratchpad register remapping before starting the cores, since that configuration can survive a core reset. Source: firmware and [host source](host/mcuplus/jpeg_host.c).

## Memory map

Offsets in the ICSS column are relative to the ICSS instance base obtained from the PRUICSS driver. Firmware uses explicitly addressed local DRAM; the linker DRAM region is not an exclusive reservation. IRAM ends before the start stub at `0x2f00`.

| Region | Both devices |
|---|---|
| PRU0 control and scan descriptor | ICSS + `0x1000` |
| Component descriptors; zigzag table | ICSS + `0x1300`; ICSS + `0x0d40` |
| Dual PRU1 control and quantization tables | ICSS + `0x2000`; ICSS + `0x2100` |
| Single PRU0 output pointer and quantization tables | ICSS + `0x1870`; ICSS + `0x1900` |
| DC predictors; restart segment index; decode error byte | `0x1400`; `0x1030`; `0x1100` |
| Single-core work buffers and register spill | `0x1100` through `0x185f` |
| Dual-core register spill and coefficient staging | `0x1600`; `0x1700` |
| Start stub | IRAM `0x2f00` |
| Short scan | ICSS + `0x16500`, ending before device Shared RAM limit |

Source: [host source](host/mcuplus/jpeg_host.c) and the firmware `linker.cmd` files.

| Buffer | LP-AM243, ICSSG0 | LP-AM261, ICSSM0 |
|---|---|---|
| Shared RAM end | `0x20000` | `0x18000` |
| Output blocks | `0x700c0000`, before `0x70100000` | `0x70080000`, before `0x700cc000` |
| Large scan | `0x70120000` to `0x701d0000` | `0x700cc000` to `0x70150000` |
| Metadata base | `0x70100000` | `0x70171000` |
| Block component and restart tables | metadata + `0x8000`, `0xa000` | metadata + `0x1000`, `0x3000` |
| Long-code Huffman tables | metadata + `0x10000` | metadata + `0x0000` |
| Restart segments | metadata + `0xc000` | metadata + `0x5000` |

Source: [host source](host/mcuplus/jpeg_host.c). The AM261x host keeps the scan below its R5F shared and log memory windows and uses MSRAM when the scan exceeds local Shared RAM.

## Timing basis

Frame totals below are measured PRU CYCLE counts on silicon. Firmware headers
give conservative instruction-count bounds for local stage execution, rather
than measured per-stage times. MSRAM latency and contention add stalls, so
instruction counts are not a system WCET guarantee. For example, the measured max-Huffman MSRAM scan at 333.3 MHz required
5427.9 PRU0 cycles per block. External scan and long-code table accesses
add stalls beyond the local instruction counts.

## Measured results

The following frame counts use the PRU `CYCLE` counter with the supplied host application. Nominal clocks are 250 MHz on LP-AM243 and 225 MHz on LP-AM261; clocks measured under that host were 250 and 225.1 MHz respectively. R5F figures are from an earlier libjpeg-turbo 2.1.5 C-path run with `-O3`, no SIMD, `JDCT_ISLOW`, `raw_data_out=TRUE`, and caches enabled on the same boards. That R5F decoder is not shipped in this example.

### LP-AM243, ICSSG0

| Image | R5F ms / fps | 1 PRU cycles / ms / fps | 2 PRU cycles (PRU0, PRU1) / ms / fps | Bit-exact |
|---|---|---|---|---|
| flat_gray_400x400 | 6.66 / 150.1 | 5239067 / 20.956 / 47.7 | 4888289, 4894206 / 19.577 / 51.1 | yes, both |
| gradient_gray_400x400 | 8.63 / 115.9 | 6973092 / 27.892 / 35.9 | 6475190, 6482988 / 25.932 / 38.6 | yes, both |
| gradient_420_400x400 | 12.51 / 79.9 | 10433740 / 41.735 / 24.0 | 9607649, 9614986 / 38.460 / 26.0 | yes, both |

Source: measured on LP-AM243 with this host application; CRCs matched the expected sample blocks.

### LP-AM261, ICSSM0

| Image | R5F ms / fps | 1 PRU cycles / ms / fps | 2 PRU cycles (PRU0, PRU1) / ms / fps | Bit-exact |
|---|---|---|---|---|
| flat_gray_400x400 | 10.64 / 94.0 | 5237997 / 23.270 / 43.0 | 4888289, 4894162 / 21.742 / 46.0 | yes, both |
| gradient_gray_400x400 | 14.42 / 69.3 | 6972551 / 30.975 / 32.3 | 6475187, 6482954 / 28.800 / 34.7 | yes, both |
| gradient_420_400x400 | 20.97 / 47.7 | 10432803 / 46.347 / 21.6 | 9607649, 9614943 / 42.714 / 23.4 | yes, both |

Source: measured on LP-AM261 with this host application; CRCs matched the expected sample blocks.

### LP-AM243, ICSSG0, 16-image test corpus at 333.3 MHz measured

The test host requests 333.33 MHz in SysConfig. The PRU CYCLE counter against wall clock measured 333.3 MHz (three samples: 333.3, 333.3 and 333.4 MHz).

The test host embeds 16 synthetic images (not bundled with this example), raises the block limit to 7680, relocates output to `0x70158000` through `0x701cffff`, and unstuffs large scans in place in MSRAM. All 32 decodes (16 images x 2 modes) were bit-exact: CRC-32 matched libjpeg-turbo output. The R5F column retains the earlier libjpeg-turbo measurement at 800 MHz.


| Image | Content | R5F ms / fps | 1 PRU cycles / ms / fps | 2 PRU cycles / ms / fps | 2 PRU vs 1 PRU | Bit-exact |
|---|---|---|---|---|---|---|
| flat_gray_400x400 | smooth synthetic | 6.66 / 150.1 | 5297028 / 15.893 / 62.9 | 4894294 / 14.684 / 68.1 | 1.082x | yes, both |
| gradient_gray_400x400 | smooth synthetic | 8.63 / 115.9 | 7031022 / 21.095 / 47.4 | 6483074 / 19.451 / 51.4 | 1.085x | yes, both |
| impulse_gray_400x400 | smooth synthetic | 6.67 / 149.9 | 5329321 / 15.990 / 62.5 | 4923536 / 14.772 / 67.7 | 1.082x | yes, both |
| dc_only_gray_400x400 | smooth synthetic | 6.77 / 147.8 | 5383580 / 16.152 / 61.9 | 4918036 / 14.756 / 67.8 | 1.095x | yes, both |
| single_ac_gray_400x400 | smooth synthetic | 9.87 / 101.3 | 9590904 / 28.776 / 34.8 | 6520348 / 19.563 / 51.1 | 1.471x | yes, both |
| gradient_420_400x400 | smooth synthetic | 12.51 / 79.9 | 10521039 / 31.566 / 31.7 | 9615071 / 28.848 / 34.7 | 1.094x | yes, both |
| gradient_444_400x400 | smooth synthetic | 23.72 / 42.2 | 19863444 / 59.596 / 16.8 | 18197285 / 54.597 / 18.3 | 1.092x | yes, both |
| restart_gray_400x400 | smooth synthetic | 8.88 / 112.6 | 7062636 / 21.190 / 47.2 | 6483074 / 19.451 / 51.4 | 1.089x | yes, both |
| restart_420_400x400 | smooth synthetic | 12.64 / 79.1 | 10537731 / 31.616 / 31.6 | 9615071 / 28.848 / 34.7 | 1.096x | yes, both |
| nonmult16_gray_401x399 | smooth synthetic | 8.76 / 114.2 | 7138258 / 21.417 / 46.7 | 6581347 / 19.746 / 50.6 | 1.085x | yes, both |
| nonmult16_420_401x399 | smooth synthetic | 12.84 / 77.9 | 10851217 / 32.557 / 30.7 | 9916707 / 29.753 / 33.6 | 1.094x | yes, both |
| restart_nonmult16_420_401x399 | smooth synthetic | 12.95 / 77.2 | 10862455 / 32.591 / 30.7 | 9916707 / 29.753 / 33.6 | 1.095x | yes, both |
| dense_ac_gray_400x400 | dense | 24.97 / 40.0 | 22866087 / 68.605 / 14.6 | 13543585 / 40.635 / 24.6 | 1.688x | yes, both |
| dense_ac_420_400x400 | dense | 34.60 / 28.9 | 30063455 / 90.199 / 11.1 | 16087770 / 48.268 / 20.7 | 1.869x | yes, both |
| dense_ac_444_400x400 | dense | 69.67 / 14.4 | 60301351 / 180.922 / 5.5 | 32337046 / 97.021 / 10.3 | 1.865x | yes, both |
| maxhuff_gray_400x400 | worst-case Huffman | 24.79 / 40.3 | 22895892 / 68.695 / 14.6 | 13573757 / 40.725 / 24.6 | 1.687x | yes, both |

Source: measured on LP-AM243 (rev E1); CRCs matched the expected sample blocks. "2 PRU cycles" is the later of the two cores.

| Content | Images | R5F fps (geometric mean) | 1 PRU fps | 2 PRU fps | 2 PRU vs 1 PRU |
|---|---|---|---|---|---|
| smooth synthetic | 12 | 98.2 | 39.4 | 44.0 | 1.12x (1.08x-1.47x) |
| dense | 3 | 25.5 | 9.6 | 17.4 | 1.81x (1.69x-1.87x) |
| worst-case Huffman | 1 | 40.3 | 14.6 | 24.6 | 1.69x (1.69x-1.69x) |

Cycle counts depend on content and memory location. The smooth synthetic,
gradient, impulse and sparse coefficient images give smaller dual-core gains
than the dense coefficient images. These tests do not establish photographic
JPEG throughput; use representative application images when choosing a mode.


## Build and run

Configure `imports.mak` for the TI PRU compiler and MCU+ SDK, then build firmware before the R5F host so the generated firmware headers are available. The firmware project `host` target is empty; build the shared host separately as shown below. The project makefiles expose `all`, `pru`, `host`, and `clean`; the separate host makefile exposes `all`, `host`, `pru`, and `clean`.

```sh
make -C examples/jpeg_decoder/jpeg_decoder_single_core DEVICE=am243x pru
make -C examples/jpeg_decoder/host DEVICE=am243x BUILD_MCUPLUS=y JPEG_MODE=single all
make -C examples/jpeg_decoder/jpeg_decoder_dual_core DEVICE=am261x pru
make -C examples/jpeg_decoder/host DEVICE=am261x BUILD_MCUPLUS=y JPEG_MODE=dual all
```

Use either `DEVICE=am243x` or `DEVICE=am261x` consistently for firmware and host; use `JPEG_MODE=single` or `JPEG_MODE=dual` for the host. The CCS host projects select dual-core firmware. For single-core CCS use, add `JPEG_SINGLE_CORE` to the host compiler predefined symbols and replace its dual-core firmware include path with the single-core firmware board directory. Build the corresponding firmware first. CCS users can import each selected firmware `example.projectspec` and the matching host `example.projectspec` under `firmware/<board>/<core>/ti-pru-cgt/` and `host/mcuplus/<board>/r5fss0-0_freertos/ti-arm-clang/`. Source: project and host makefiles and projectspec files.

Load the host application on the R5F and observe its `DebugP_log` UART output. A passing run prints one CRC line per bundled image, for example `flat_gray_400x400: CRC 1bdadcaf expected 1bdadcaf, PRU0 5239067 cycles, 47 fps` on the LP-AM243 single PRU run; dual mode also prints PRU1 cycles. The expected CRCs for the other bundled images are `d5161b37` and `aa881b03`. Source: measured LP-AM243 console results and [host source](host/mcuplus/jpeg_host.c).

The synthetic corpus contains smooth, gradient, impulse, sparse coefficient, and dense coefficient test images; it does not measure photographs. The short scan starts at `0x16500` to leave preceding Shared RAM available for other data.

## Notes

On ICSSM, enabling the PRU cycle counter only in the start stub can race the host driver's read-modify-write of the PRU CONTROL register. The supplied host clears CYCLE while reset and writes CONTROL with COUNTER_ENABLE set before calling `PRUICSS_enableCore()`. Preserve that sequence when adapting the host, or the counter can stop while decoding continues. Source: [host source](host/mcuplus/jpeg_host.c).

Measured on LP-AM243 (rev E1) and LP-AM261 (rev E2), built with TI PRU CGT 2.3.3, TI Arm Clang 4.0.5.LTS, MCU+ SDK 12.01.00.33 with SysConfig 1.28.0 (AM243x) and MCU+ SDK 11.00.00.29 with SysConfig 1.26.0 (AM261x). The 250 MHz (LP-AM243) and 225 MHz (LP-AM261) tables cover the three bundled images on each board with the supplied host. The separate corpus table adds 4:4:4, dense and worst-case Huffman content and scans placed in MSRAM, on LP-AM243 only, using the test host build described above. Its PRU clock was measured at 333.3 MHz for the results above.
