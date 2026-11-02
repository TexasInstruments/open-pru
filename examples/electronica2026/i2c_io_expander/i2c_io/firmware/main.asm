; SPDX-License-Identifier: BSD-3-Clause
; Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

;***************************************************************************
;   File:     main.asm
;
;   Brief:    ICSSG0 PRU0 bit-bang I2C master (1 Mbit/s) driving a
;             TCA9538 8-bit IO expander as a running LED.
;
;   Ported from pru_simulator source/i2c_tca9538_running_led.asm to
;   AM243x LaunchPad (LP-AM243) silicon. See
;   ../docs/porting_report.md for the full change list.
;
;   Behaviour:
;             - Configure all 8 TCA9538 ports as outputs (Configuration
;               register 0x03 = 0x00).
;             - Forever: write one set bit 0x01 -> 0x02 -> ... -> 0x80 ->
;               0x01 to the Output Port register (0x01).
;
;   Hardware requirements:
;             - PRU core clock 200 MHz (example.syscfg, coreClk).
;             - R30/R31 in direct GPIO mode (ICSSG GPCFG0 GP_MUX_SEL = 0,
;               set by the R5F host before the core starts).
;             - SCL = PRG0_PRU0_GPO0 (R30 bit 0), LP BoosterPack pin 33
;             - SDA = PRG0_PRU0_GPO1 (R30 bit 1), LP BoosterPack pin 32
;             - TCA9538 at 7-bit address 0x23 (A1 = 1, A0 = 1).
;
;   Silicon note (push-pull SDA):
;             On AM243x a PRU pad is muxed either as GPO (mode 0) or as
;             GPI (mode 1), and GPO drives push-pull. The simulator's
;             open-drain model (write 1 = release, read SDA back on R31)
;             does not exist on silicon. Therefore this port:
;             - drives SDA LOW during the 9th (ACK) clock, so the master
;               never fights the slave's ACK pull-down, and
;             - does not sample ACK: r20 is forced to 0 (ACK). Both
;               replacements cost 1 cycle each, so the timing is
;               identical to the simulated source.
;             NACK detection (init_fail, run_nack) is therefore inactive
;             on silicon. The paths are kept for parity with the
;             simulator source.
;
;   I2C bit rate:
;             1 Mbit/s (Fast-mode Plus timing). The TCA9538 datasheet
;             rates the device for 400 kHz maximum (tLOW >= 1.3 us,
;             tHIGH >= 0.6 us). 1 Mbit/s is outside that specification
;             and was kept on request; raise DELAY_COUNT for in-spec
;             operation (DELAY_COUNT = 130 gives about 380 kHz).
;
;   Steps to build:
;             - Using CCS: import ti-pru-cgt/example.projectspec, build.
;             - Using makefile: make (project directory)
;
;***************************************************************************

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .sect       ".text"

; ---- Register map --------------------------------------------------------
; Reg     Use                               Live in           Owner
; r0      scratch, error-flag value         run_nack          main
; r1      subroutine return address         jal .. jmp r1     FN_I2C_*
; r2      byte to shift out (MSB first)     FN_I2C_WRITE_BYTE main/FN
; r3      delay-loop counter                every FN_I2C_*    FN_I2C_*
; r6      bit counter (8 downto 1)          FN_I2C_WRITE_BYTE FN
; r19     DRAM0 error-flag offset           run_nack          main
; r20     ACK result: 0 = ACK, 2 = NACK     after write_byte  FN/main
;         (silicon port: always 0, see header)
; r21     running-LED pattern 0x01 .. 0x80  run_loop          main
; r30     GPO: SCL = bit 0, SDA = bit 1     everywhere        hardware
; r31     GPI: not used on silicon          -                 hardware
;
; Call convention: one level of jal r1 / jmp r1. No FN_I2C_* calls
; another, so r1 is never nested.
;
; ---- Cycle budget (200 MHz, 5 ns/cycle) ----------------------------------
; Edge-to-edge spacing on the pins (DELAY_COUNT = 48).
; Path                       Budget      Estimate     Verified
; data bit, SCL low          100 cyc     100-101 cyc  pru-simulator edges
; data bit, SCL high         100 cyc     101 cyc      pru-simulator edges
; ACK bit,  SCL low          100 cyc     99 cyc       pru-simulator edges
; ACK bit,  SCL high         100 cyc     99 cyc       pru-simulator edges
; START / STOP setup, hold   100 cyc     98 cyc       pru-simulator edges
; --------------------------------------------------------------------------

SCL_BIT     .set 0
SDA_BIT     .set 1
DELAY_COUNT .set 48          ; 2 cycles per sub+qbne iteration: 96 cycles
                              ; + 2..5 cycles phase overhead = 98-101 cycles
                              ; per half bit (measured in pru-simulator)
ADDR_W      .set 0x46        ; TCA9538 addr 0x23, R/W = 0: (0x23 << 1) | 0
CFG_REG     .set 0x03        ; TCA9538 Configuration register
OUT_REG     .set 0x01        ; TCA9538 Output Port register
ERR_ADDR    .set 0x0FFE      ; DRAM0 offset of the soft NACK error flag

;********
;* MAIN *
;********

main:
    zero &r0, 120                   ; clear r0-r29
    ldi  r30, 0x0003                ; idle bus: SCL = 1, SDA = 1

    jal  r1, FN_I2C_START
    ldi  r2, ADDR_W
    jal  r1, FN_I2C_WRITE_BYTE
    qbne init_fail, r20, 0

    ldi  r2, CFG_REG
    jal  r1, FN_I2C_WRITE_BYTE
    qbne init_fail, r20, 0

    ldi  r2, 0x00                   ; all 8 pins = outputs
    jal  r1, FN_I2C_WRITE_BYTE
    qbne init_fail, r20, 0

    jal  r1, FN_I2C_STOP

    ldi  r21, 0x01                  ; running-LED pattern, starts at P0

run_loop:
    jal  r1, FN_I2C_START
    ldi  r2, ADDR_W
    jal  r1, FN_I2C_WRITE_BYTE
    qbne run_nack, r20, 0

    ldi  r2, OUT_REG
    jal  r1, FN_I2C_WRITE_BYTE
    qbne run_nack, r20, 0

    mov  r2, r21
    jal  r1, FN_I2C_WRITE_BYTE
    qbne run_nack, r20, 0

    jal  r1, FN_I2C_STOP
    jmp  advance_pattern

run_nack:
    ldi  r0, 1
    ldi  r19, ERR_ADDR
    sbco &r0, c24, r19, 1           ; DRAM0[0x0FFE] = 1 (soft error flag)
    jal  r1, FN_I2C_STOP            ; release the bus, retry next iteration

advance_pattern:
    lsl  r21, r21, 1
    qbbc run_loop, r21, 8           ; loop unless bit 8 set (wrapped past P7)
    ldi  r21, 0x01
    jmp  run_loop

init_fail:
    halt                            ; no use continuing without CONFIG set

;***************************************************************************
;   Function: FN_I2C_START
;
;   Generate an I2C START condition from idle (SCL = 1, SDA = 1).
;
;   Pseudo code:
;       SCL = 1; SDA = 1; wait DELAY_COUNT
;       SDA = 0                      (START: SDA falls while SCL = 1)
;       wait DELAY_COUNT
;       SCL = 0; return
;
;   PEAK cycles: 7 + 4 * DELAY_COUNT (199 cycles at DELAY_COUNT = 48)
;
;   Parameters:
;       r1 - return address (set by jal)
;
;   Registers modified: r3, r30
;***************************************************************************
FN_I2C_START:
    set  r30, r30, SCL_BIT          ; 1
    set  r30, r30, SDA_BIT          ; 1
    ldi  r3, DELAY_COUNT            ; 1
is_d1:
    sub  r3, r3, 1                  ; 1
    qbne is_d1, r3, 0               ; 1
    clr  r30, r30, SDA_BIT          ; 1  SDA falls while SCL = 1 -> START
    ldi  r3, DELAY_COUNT            ; 1
is_d2:
    sub  r3, r3, 1                  ; 1
    qbne is_d2, r3, 0               ; 1
    clr  r30, r30, SCL_BIT          ; 1  SCL low, ready for first bit
    jmp  r1                         ; 1

;***************************************************************************
;   Function: FN_I2C_STOP
;
;   Generate an I2C STOP condition from SCL = 0, leaves the bus idle.
;
;   Pseudo code:
;       SDA = 0; wait DELAY_COUNT
;       SCL = 1; wait DELAY_COUNT
;       SDA = 1                      (STOP: SDA rises while SCL = 1)
;       return
;
;   PEAK cycles: 6 + 4 * DELAY_COUNT (198 cycles at DELAY_COUNT = 48)
;
;   Parameters:
;       r1 - return address (set by jal)
;
;   Registers modified: r3, r30
;***************************************************************************
FN_I2C_STOP:
    clr  r30, r30, SDA_BIT          ; 1  SDA low while SCL low (setup)
    ldi  r3, DELAY_COUNT            ; 1
ps_d1:
    sub  r3, r3, 1                  ; 1
    qbne ps_d1, r3, 0               ; 1
    set  r30, r30, SCL_BIT          ; 1
    ldi  r3, DELAY_COUNT            ; 1
ps_d2:
    sub  r3, r3, 1                  ; 1
    qbne ps_d2, r3, 0               ; 1
    set  r30, r30, SDA_BIT          ; 1  SDA rises while SCL = 1 -> STOP
    jmp  r1                         ; 1

;***************************************************************************
;   Function: FN_I2C_WRITE_BYTE
;
;   Shift r2.b0 out MSB first on SDA, then clock the 9th (ACK) bit.
;
;   Pseudo code:
;       for bit = 7 downto 0:
;           SCL = 0; SDA = r2[7]; wait DELAY_COUNT
;           SCL = 1;              wait DELAY_COUNT
;           r2 <<= 1
;       SCL = 0; SDA = 0 (silicon: push-pull, never release); wait
;       SCL = 1; wait; r20 = 0 (ACK assumed); SCL = 0
;       return
;
;   PEAK cycles: 9 + 8 * (10 + 4 * DELAY_COUNT) + 4 * DELAY_COUNT
;                (1817 cycles = 9.085 us at DELAY_COUNT = 48)
;
;   Parameters:
;       r1    - return address (set by jal)
;       r2.b0 - byte to send (0x00 - 0xFF)
;
;   Returns:
;       r20 - 0 = ACK (always 0 on silicon, see file header)
;
;   Registers modified: r2, r3, r6, r20, r30
;***************************************************************************
FN_I2C_WRITE_BYTE:
    ldi  r6, 8                      ; 1
wb_bit:
    clr  r30, r30, SCL_BIT          ; 1
    qbbc wb_clear, r2, 7            ; 1
    set  r30, r30, SDA_BIT          ; 1
    jmp  wb_delay1                  ; 1
wb_clear:
    clr  r30, r30, SDA_BIT          ; 1
wb_delay1:
    ldi  r3, DELAY_COUNT            ; 1
wb_d1:
    sub  r3, r3, 1                  ; 1
    qbne wb_d1, r3, 0               ; 1
    set  r30, r30, SCL_BIT          ; 1  slave samples SDA
    ldi  r3, DELAY_COUNT            ; 1
wb_d2:
    sub  r3, r3, 1                  ; 1
    qbne wb_d2, r3, 0               ; 1
    lsl  r2, r2, 1                  ; 1
    sub  r6, r6, 1                  ; 1
    qbne wb_bit, r6, 0              ; 1

    ; 9th clock (ACK). Silicon port: SDA is push-pull, so drive it LOW
    ; instead of releasing it. The slave pulls low for ACK as well, so
    ; master and slave never drive opposite levels.
    clr  r30, r30, SCL_BIT          ; 1
    clr  r30, r30, SDA_BIT          ; 1  was: set (release) in simulator
    ldi  r3, DELAY_COUNT            ; 1
wb_d3:
    sub  r3, r3, 1                  ; 1
    qbne wb_d3, r3, 0               ; 1
    set  r30, r30, SCL_BIT          ; 1
    ldi  r3, DELAY_COUNT            ; 1
wb_d4:
    sub  r3, r3, 1                  ; 1
    qbne wb_d4, r3, 0               ; 1
    ldi  r20, 0                     ; 1  was: and r20, r31, 2 (ACK sample)
    clr  r30, r30, SCL_BIT          ; 1
    jmp  r1                         ; 1
