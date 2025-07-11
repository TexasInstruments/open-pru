# CRC16/32

## Introduction

This example acts as a basic demonstration of the usage of CRC16/32 module in ICSSM PRU.

## Overview 

This example demonstrates the functionality of the built-in CRC (Cyclic Redundancy Check) module available in the PRU-ICSS. The example performs both CRC16 and CRC32 calculations on a test input value, showcasing how to properly configure and use the CRC module for data integrity verification.

The PRU CRC module implements industry-standard polynomial expressions:

CRC32 using the polynomial: x³² + x²⁶ + x²³ + x²² + x¹⁶ + x¹² + x¹¹ + x¹⁰ + x⁸ + x⁷ + x⁵ + x⁴ + x² + x + 1
CRC16 using the polynomial: x¹⁶ + x¹⁵ + x² + 1
The example code configures the CRC module, performs the calculations, and stores the results in:

- PRU Register R10 for CRC16 result
- PRU Register R11 for CRC32 result

This implementation is particularly useful in applications requiring data integrity validation, error detection in communication protocols, and verification of data transmission accuracy. The example runs on various AM26x family processors and demonstrates the essential features of the PRU CRC module in a straightforward manner.

# Supported Combinations

 Parameter      | Value
 ---------------|-----------
 ICSSM          | ICSSM0 - PRU0, PRU1, ICSSM1 (only in am261x) - PRU0, PRU1 
 Toolchain      | pru-cgt
 Board          | am261x-lp, am261x-som, am263px-cc, am263px-lp, am263x-cc, am263x-lp
 Example folder | examples/pru_io/crc/

