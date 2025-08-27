# Custom Frequency Generator

The project generates a square wave of the desired average clock frequency from the available system clock. It makes use of a fractional adder which generates a pulse having an average frequency that matches a configurable desired frquency, that vary over a wide range.

<img src="pictures/freq_gen_setup.png" alt="CFG_setup" width="600">
<figcaption>Fig.1 Frequency Generator setup</figcaption>
</figure>

## Overview

PRU has very few available clock frequency options. While clock dividers can help generate certain divisions of available clocks, to generate other specific clock frequencies, we have to rely on other mechanisms like a fractional adder. This project generates a square wave that on average has a frequency very close to a specific desired frequency. It does this by generating two frequencies higher and lower than the desired frequency in a calculated manner such that the average comes as close as possible to the desired frequency. 

 For the generation of a particular clock frequency, we generate two signals with time periods that correspond to the closest longer and shorter time perods for a particular clock frequency. The shorter time period is generated in the beginning and the fractional difference between the shorter period and the desired period (whic is calculated from the desired frequency) is accumulated as part of a fractional adder. Whenever this accumulated fraction exceeds a maximum limit, which is the clock time-period or the difference between the two closest longer and shorter time-periods, we generate the longer time period.  This process continues indefenitely, thereby generating the two time periods alternatively to give out the average desired time-period required.

 The fractional adder algorithm is integrated within the clock frequency generation code for both time-periods in this project. The duration of high and low signals are kept the same for the signal generation, as the generated signal is assumed to be used as a clock signal. The alternate generation of longer and shorter time periods for a specific example of 16.384 Mhz is shown below as a timing diagram:

 <img src="pictures/CFG_timing.png" alt="CFG_setup" width="1200">
<figcaption>Fig.2 Timing diagram of Custom frequency generator</figcaption>
</figure>

## Configuration

All the configurations mentioned are performed in the 'macros' section of the file "fn_fg_output.asm".

Maximum Genertable Frequency : (Clock Frequency/8)

=> minimum of 8 clock cycles time period

For a clock setting of 333Mhz: f_max = ~41.6 Mhz 


1. Determine a higher and lower time period close to the desired frequency. Eg., if the desired frequency is 16.384 Mhz, $$ T = 61.0351562 $$
PRU running on 333Mhz, has a time period of around 3ns and the closest higher and lower time periods are 60ns and 63ns.

2. Set the Q30 format of the fraction to be accumulated and added using fractional adder as parameter 'FRAC_ACC'. This is the difference between the desired time period and the chosen lower time period.
For the previous example,

    $$  T-T\_lower\_period = 1.03515625 $$
    $$  FRAC\_ACC = 1.03515625 * 2^{30} $$
    ```
    FRAC_ACC .set 0x42400000
    ```

3. Set the Q30 format of the maximum accumulation limit as parameter 'FRAC_ACC_MAX'. This is equal to the clock time period. For a clock of 333Mhz, 
    $$ FRAC\_ACC\_MAX= T\_clock = 3ns$$
    ```
     FRAC_ACC_MAX .set 0xC0000000
    ```
4. Set the time period of shorter period(60ns) in clock cycles
    $$60ns => 20 cycles @333Mhz$$
    ```
    CFG_T_SH_CYC .set 20
    ```
    Note : minimum value of CFG_T_SH_CYC is 8 according to implementation constraints. 
## How to Run

1. Import the device specific project from 'examples/custom_frequency_generator' in the open-pru repository.
2. Keep the default configuration (for 16.384Mhz) or change the configuration according to section [Configuration](#configuration) for a desired frequency.
3.  Build and load the "*.out" file into the core 'ICSSG0 PRU0' of AM243x device. 
4. Run the core and the desired average frequency is available on PIN PRU0_GPO0(BP.33). To view the signal, connect a channel of a logic scope to the PIN. 

## Results

A portion of the output signal traced using the PRU Logic Scope(see examples/logic_scope) and viewed on pulse view (see how to view trace in logic scope on documentation in examples/logic_scope/README.md) shows a signal with average frequency as 16.394Mhz, which is as close as one decimal place to the desired frequency : 

 <img src="pictures/cfg_results.png" alt="cfg_results" width="1000">
<figcaption>Fig.3 Resulting wave of Frequency generator</figcaption>
</figure>