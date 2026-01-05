# OpenPRU Examples

[Projects](#projects)  
[Supported processors per-project](#supported-processors-per-project)

This directory contains example PRU projects.

## Projects

custom_frequency_generator
* Generate a square wave with a configurable average clock frequency

empty
* The empty project can be used for PRU firmware development in assembly

empty_c
* The empty_c project can be used for PRU firmware development in C, and mixed
  C and assembly

fft/split_radix_fft_4k_single_core
* Use a real-valued DIT Split-Radix algorithm to perform a 4096-point
  Fast-Fourier Transform (FFT) on 4096 discrete input samples

fft/split_radix_fft_post_processing
* Extract FFT magnitude information from the output of
  split_radix_fft_4k_single_core

LCD_interface
* Use PRU direct GPOs to output data from frame buffer memory to a parallel
  RGB24 interface. The LCD display is 800x480 pixel with 24 bit color and 60 Hz
  frame rate

logic_scope
* PRU cores implement a simple logic scope by continuously sampling input
  signals. The captured trace data can be viewed with a tool like PulseView

pru_emif
* Emulate an Extended Memory Interface (EMIF) client 16-bit/32-bit asynchronous
  parallel interface to interface with C2K F28379D

pru_i2s_diagnostic
* Emulate an I2S/TDM master, allowing for transmission and reception of audio
  data over the PRU-I2S interface

spi_loopback
* Implement a loopback SPI connection between 2 PRU cores that act as SPI master
  and SPI slave

fir
* Implements a 64-tap FIR filter on PRU with 32 bit fixed-point precision inputs(Q31).

multicore_scheduler
* Makes use of IEP timer along with Task Managers in 6 cores to schedule tasks in 6 cores back to back with no delay in between.

## Supported processors per-project

Each project is tested on at least one processor. Many projects can be ported to
other processors, even if the project does not currently have a build
configuration for the other processors.

For more information about porting PRU
projects to different processors, refer to app note
[PRU Subsystem Migration Guide](https://www.ti.com/lit/spracj8).
For more information about the PRU features on each processor, refer to app note
[PRU Subsystem Features Comparison](https://www.ti.com/lit/sprac90).

* Y = project has build infrastructure for this processor
* Yport = project can be ported to this processor
* Npru = project is not compatible with the PRU subsystem on this processor
  * The project cannot be ported to this processor
* N-hw = project relies on SoC hardware that does not exist on this processor
  * The project cannot be ported to this processor, unless the project can be
    modified to work with the new processor's hardware
* N-sw = project's non-PRU software is not compatible with this processor
  * The PRU firmware may be able to be ported to this processor,
    but you will need to write new code for the non-PRU cores
  * There may be limitations related to the OS (for example, let's say the
    project requires a real-time OS that always responds within 10 usec. This
    project would not work with OSes like Linux, or even RT Linux)

| Project                         | am243x | am261x | am263px | am263x | am62x | am64x |
| ------------------------------- | ------ | ------ | ------- | ------ | ----- | ----- |
| custom_frequency_generator      | Y      | Yport  | Yport   | Yport  | Yport | Yport |
| empty                           | Y      | Y      | Y       | Y      | Y     | Y     |
| empty_c                         | Y      | Y      | Y       | Y      | Y     | Y     |
| fft/                                                                                 |
| split_radix_fft_4k_single_core  | Y      | Y      | Yport   | Yport  | Yport | Yport |
| split_radix_fft_post_processing | Y      | Yport  | Yport   | Yport  | Yport | Yport |
| LCD_interface                   | Y      | Yport  | Yport   | Yport  | N-sw  | Yport |
| logic_scope                     | Y      | Yport  | Yport   | Yport  | Yport | Yport |
| pru_emif                        | Y      | Yport  | Yport   | Yport  | N-sw  | Yport |
| pru_i2s_diagnostic              | Yport  | Y      | Yport   | Y      | N-sw  | Yport |
| spi_loopback                    | Y      | Yport  | Yport   | Yport  | N-sw  | Yport |
| fir                             | Y      | Yport  | Yport   | Yport  | Yport | Yport |
| multicore_sheduler              | Y      | Npru   | Npru    | Npru   | Npru  | Yport |

