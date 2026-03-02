# QSPI Memory Interface Example

This example demonstrates PRU-based QSPI memory interface functionality.

## Supported Devices

- AM261x (am261x-lp)

## Project Structure

```
qspi_mem_interface/
  firmware/
    main.asm                    # Shared PRU source
    am261x-lp/
      icss_m0_pru0_fw/          # PRU0 firmware
      icss_m0_pru1_fw/          # PRU1 firmware
  mcuplus/
    qspi_mem_interface_example.c  # R5F application
    am261x-lp/
      r5fss0-0_freertos/        # R5F FreeRTOS project
```

## Building

### Using Makefile

```bash
cd examples/qspi_mem_interface
make -s
```

### Using CCS

Import the projects from `ti-pru-cgt/example.projectspec` and
`ti-arm-clang/example.projectspec` into CCS.

## Description

The R5F loads PRU0 and PRU1 firmware for QSPI memory interface operations.
Both PRU cores execute a minimal firmware that halts after initialization.

