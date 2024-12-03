<div align="center">

# FFT on PRU

[Introduction](#introduction) | [Overview](#Overview) | [Instructions](#Instructions) | [Algorithm](#Algorithm) | [Usage](#usage)

</div>

## Introduction
The provided code performs a 4096-point Real-valued Fast-Fourier Transform on 4096 discrete input samples in the ICSS Shared Memory. The specific implementation makes use of Split-Radix algorithm and runs on ICSSG PRU0. 

## Overview
The algorithm assumes input samples in Q24 format to be present in the configured memory space. The current implementation assumes unsigned inputs which are common for adc measurement values. 

The function is a task that gets called every specified time interval, which could also be configured to be triggered by a particular event (for example, when inputs samples are ready). There is a window function applied to the input samples in the beginning which can be configured if needed (see [Utilities](#utilities)). 

After execution, the output samples in frequency domain will be present in shared memory in the order : {Re(0), Re(1),.., Re(N/2), Im((N/2)-1),.., Im(1)}. 

## Instructions 
### Loading LUTs
### Execution
### Post Processing

## Utilities

## Algorithm 

### Window Function and Bit-order Reversed scrambling 

#### Windowing 
The FFT is performed on a fixed number of discrete input signal samples. In this specific implementation, we will be performing a 4K FFT which processes 4096 samples (2 to the power of 12) at a time. The algorithm assumes that the input data set that we ae working on is a finite one that represents one period of the continuous signal. This means the end points of the input sample set are assumed to be continuous. The signals could be sampled coherently, which means the samples collectively contain an integer number of periods in it and which by default should mean the end points are continuous. However, the more common case is that the signal is sampled non-coherently and there is an abrupt transition between end-points. This artificial discontinuity shows up in the final FFT spectrum and widens the spectral lines of different frequencies more than what it is, leading to what is known as Spectral Leakage. To minimize spectral leakage for non-coherent samples, we apply a window function over the input samples which smoothens out the end-point signal amplitudes and minimizes the effect of spectral leakage.

#### Bit-order reversed index calculation
When FFT is performed on a discrete-time signal, the output samples in frequency domain appear scrambled. More specifically, the output sample corresponding to index 'i' will be in position 'j' where 'j' can be obtained by reversing the order of bits in 'i'. Instead of un-scrambling the outputs to a linear form, what we do is to scramble the input data by calculating the bit-order reversed index for each index of input data and storing the samples in that position. When FFT is performed on such a scrambled form, the output frequency domain data comes out in a linear form.

To calculate the bit-order-reversed index for every index, we use a Look up Table Based approach which is described in the diagram :

### Split-radix FFT




