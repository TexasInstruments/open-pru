# RPMSG Echo Example Using the GNU Toolchain

This is an RPMSG firmware built using the GNU toolchain for PRU. This firmware
is the classical RPMSG echo example from Texas Instruments, but with a twist
to show-off some GCC features. The returned echo string is reversed using
inline assembly routine.

WARNING: Neither the GNU PRU toolchain nor this example are supported by Texas Instruments!

The example was last tested on BeaglePlay board running kernel 6.12.57-ti-arm64-r59.

## Getting the GNU toolchain for PRU

Convenient prebuilt toolchain binaries are offered [here](https://github.com/dinuxbg/gnupru/releases).
They are prepared by the same hobbyist, who also maintains the PRU port for GCC and Binutils.
This toolchain includes:
  * [GCC](https://gcc.gnu.org/) compiler.
  * [Binutils](https://www.gnu.org/software/binutils/) assembler, linker and tools.
  * [newlib](https://sourceware.org/newlib/) C library.

Being free software, the GNU toolchain can also be built from sources.
PRU support has been mainlined, and is part of recent official source releases
for GCC, Binutils and newlib. Apart from official GNU sources, you need only a
[small additional package](https://github.com/dinuxbg/gnuprumcu) for
SoC-specific support in order to
[build the toolchain](https://github.com/dinuxbg/gnupru/?tab=readme-ov-file#building-from-sources) by yourself.

## Compiling the example

```
cd examples/gcc_rpmsg_echo_linux
make MCU_DEVICE=am62x.pru0
```

Or, if compiling for the other PRU core:

```
make MCU_DEVICE=am62x.pru1
```

Steps for running the firmware are the same as for the TI CGT example.

## Reasons to consider using the GNU toolchain for PRU

 * The GNU toolchain is free software.
 * GCC supports the [latest language](https://gcc.gnu.org/onlinedocs/gcc/Standards.html#C-Language)
   standards (e.g. C23 and C2Y), and numerous [extensions](https://gcc.gnu.org/onlinedocs/gcc/C-Extensions.html).
 * GCC has a really nice [static analyzer](https://developers.redhat.com/articles/2025/04/10/6-usability-improvements-gcc-15#4__an_easier_transition_to_c23).
 * GCC has a powerful [extension](https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html)
   for writing snippets of inline assembly inside C functions.

## Support

For community support you can:
  * File an issue in https://github.com/dinuxbg/gnupru
  * Ask a question in https://forum.beagleboard.org
