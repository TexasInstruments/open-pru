# PRU Assembly Instruction Cheat Sheet
OP(255) is either 8 bit immideate value or register
Load/store and xfr instructions can use bn as length operand. bn is any of R0 bytes - b0, b1, b2, b3.
Move indirect instructions use any of R1 bytes as pointer. 
Certain IO modes and broadside accelerators use fixed registers. For example hardware mulitplier uses R25-R29 and BS-RAM uses R1-R9.

## Arithmetic Operations

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **ADD** | `ADD Reg1, Reg2, OP(255)` | Unsigned 32-bit add: Reg1 = Reg2 + OP(255) |
| **ADC** | `ADC Reg1, Reg2, OP(255)` | Add with carry: Reg1 = Reg2 + OP(255) + carry |
| **SUB** | `SUB Reg1, Reg2, OP(255)` | Subtract: Reg1 = Reg2 - OP(255) |
| **SUC** | `SUC Reg1, Reg2, OP(255)` | Subtract with carry: Reg1 = Reg2 - OP(255) - carry |
| **RSB** | `RSB Reg1, Reg2, OP(255)` | Reverse subtract: Reg1 = OP(255) - Reg2 |
| **RSC** | `RSC Reg1, Reg2, OP(255)` | Reverse subtract with carry |

## Logical Operations

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **AND** | `AND Reg1, Reg2, OP(255)` | Bitwise AND: Reg1 = Reg2 & OP(255) |
| **OR** | `OR Reg1, Reg2, OP(255)` | Bitwise OR: Reg1 = Reg2 \| OP(255) |
| **XOR** | `XOR Reg1, Reg2, OP(255)` | Bitwise XOR: Reg1 = Reg2 ^ OP(255) |
| **NOT** | `NOT Reg1, Reg2` | Bitwise NOT: Reg1 = ~Reg2 |
| **LSL** | `LSL Reg1, Reg2, OP(31)` | Logical shift left: Reg1 = Reg2 << OP(31) |
| **LSR** | `LSR Reg1, Reg2, OP(31)` | Logical shift right: Reg1 = Reg2 >> OP(31) |

## Bit Operations

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **CLR** | `CLR Reg1, Reg2, OP(31)` | Clear bit: Reg1 = Reg2 & ~(1<<OP(31)) |
| **SET** | `SET Reg1, Reg2, OP(31)` | Set bit: Reg1 = Reg2 \| (1<<OP(31)) |
| **LMBD** | `LMBD Reg1, Reg2, OP(255)` | Left-most bit detect - scans for bit value |

## Comparison Operations

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **MIN** | `MIN Reg1, Reg2, OP(255)` | Copy minimum value to Reg1 |
| **MAX** | `MAX Reg1, Reg2, OP(255)` | Copy maximum value to Reg1 |

## Data Movement

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **MOV** | `MOV Reg1, Reg2` | Copy value: Reg1 = Reg2 (zero extends) |
| **LDI** | `LDI Reg1, IM(65535)` | Load immediate value into Reg1 |
| **LDI32** | `LDI32 Reg1, IM(2^32-1)` | Pseudo code for 32 bit load |

## Move Register File Indirect (V2+ cores only)

The MVIx instruction family moves 8-bit (MVIB), 16-bit (MVIW), or 32-bit (MVID) values using register pointers.

**Pointer Registers:** Must be r1.b0, r1.b1, r1.b2, or r1.b3 (byte offsets into register file, 0-127)

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **MVIB** | `MVIB [*][--]Reg1[++], [*][--]Reg2[++]` | Move 8-bit value (byte) |
| **MVIW** | `MVIW [*][--]Reg1[++], [*][--]Reg2[++]` | Move 16-bit value (word) |
| **MVID** | `MVID [*][--]Reg1[++], [*][--]Reg2[++]` | Move 32-bit value (dword) |

**Addressing Modes:**
- `*Reg` - Indirect: use register value as offset into register file
- `Reg++` - Post-increment: use value then increment by data size (1/2/4 bytes)
- `--Reg` - Pre-decrement: decrement by data size then use value
- `Reg` - Direct: use register value directly (not as pointer)

**Examples:**
```assembly
MVIW r2.w0, *r1.b3          ; Load word from register file offset in r1.b3
MVID *r1.b0++, r14          ; Store r14 to offset, then r1.b0 += 4
MVID *--r1.b0, r14          ; Decrement r1.b0 by 4, then store r14
MVIB r2, *r1.b0++           ; Load byte, then r1.b0 += 1
MVIW *r1.b1, r2.w0          ; Store word to offset in r1.b1
```

**Important Notes:**
- Either source OR destination must be a pointer (at least one `*`)
- Cannot have both source and destination as indirect (`*src, *dst` is invalid)
- Pointer values > 127 wrap around to 0
- Auto-increment/decrement amount matches operation size

## Memory Operations

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **LBBO** | `LBBO &Reg1, Rn2, OP(255), IM(124)` | Load byte burst from memory address Rn2+offset |
| **SBBO** | `SBBO &Reg1, Rn2, OP(255), IM(124)` | Store byte burst to memory address Rn2+offset |
| **LBCO** | `LBCO &Reg1, Cn2, OP(255), IM(124)` | Load byte burst with constant table offset Cn2+offset |
| **SBCO** | `SBCO &Reg1, Cn2, OP(255), IM(124)` | Store byte burst with constant table offset Cn2+offset |
| **ZERO** | `ZERO &Reg1, IM(124)` | Clear register space to zero (pseudo-op) |

**Memory Operation Notes:**
- Maximum transfer length: 124 bytes
- Can use register for length: `LBBO &r3, r1, r2.w0, b0` (length in r0.b0)
- ⚠️ **Do NOT use byte count of 0 in r0.bn** - can hang PRU!

**Examples:**
```assembly
lbbo &r2, r1, 5, 8          ; Copy 8 bytes into r2/r3 from memory at r1+5
sbbo &r2, r1, 256, 129      ; ERROR: Offset must be <256, Length must be <129
lbco &r2, c1, 5, 8          ; Copy 8 bytes from constant table address c1+5
```

## Transfer Bus Operations (XFR)

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **XIN** | `XIN IM(253), Reg, IM(124)` | Transfer in from peripheral device |
| **XOUT** | `XOUT IM(253), Reg, IM(124)` | Transfer out to peripheral device |
| **XCHG** | `XCHG IM(253), Reg, IM(124)` | Exchange with peripheral (may not be supported) |

**Examples:**
```assembly
XIN  XID_SCRATCH, r2, 8     ; Read 8 bytes from scratch pad to r2:r3
XOUT XID_SCRATCH, r2, b2    ; Write 'b2' bytes to scratch starting at r2
XCHG XID_SCRATCH, r2, 8     ; Exchange r2:r3 with 8 bytes from scratch
```

## Task Manager Operations (V4+ cores with ICSS_G only)

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **TSEN** | `TSEN 0` | Disable task manager (ICSS_G devices only) |
| **TSEN** | `TSEN 1` | Enable task manager (ICSS_G devices only) |

**Task Manager Notes:**
- Only available on V4+ PRU cores with ICSS_G subsystem
- Used to enable hardware task scheduling capabilities
- Enables context switching between multiple PRU tasks
- Check device documentation for ICSS_G availability

**Example:**
```assembly
tsen 0                      ; Disable task manager mode 0
tsen 1                      ; Enable task manager mode 1
```

## Branch/Jump Operations

### Unconditional Jumps

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **JMP** | `JMP OP(65535)` | Unconditional jump to address |
| **JAL** | `JAL Reg1, OP(65535)` | Jump and link (saves return address in Reg1) |
| **QBA** | `QBA Label` | Quick branch always (position independent) |

### Conditional Branches

**⚠️ CRITICAL: Operand order is reversed! Third operand is compared to second operand**

**Format:** `QBxx Label, Reg1, OP(255)` means: branch if `OP(255) <comparison> Reg1`

| Instruction | Syntax | Comparison | Branches When |
|-------------|--------|------------|---------------|
| **QBEQ** | `QBEQ Label, Reg1, OP(255)` | `OP(255) == Reg1` | Third operand equals second |
| **QBNE** | `QBNE Label, Reg1, OP(255)` | `OP(255) != Reg1` | Third operand not equal to second |
| **QBGT** | `QBGT Label, Reg1, OP(255)` | `OP(255) > Reg1` | Third operand greater than second |
| **QBGE** | `QBGE Label, Reg1, OP(255)` | `OP(255) >= Reg1` | Third operand greater/equal to second |
| **QBLT** | `QBLT Label, Reg1, OP(255)` | `OP(255) < Reg1` | Third operand less than second |
| **QBLE** | `QBLE Label, Reg1, OP(255)` | `OP(255) <= Reg1` | Third operand less/equal to second |
| **QBBS** | `QBBS Label, Reg1, OP(31)` | Bit set | Bit OP(31) is set in Reg1 |
| **QBBC** | `QBBC Label, Reg1, OP(31)` | Bit clear | Bit OP(31) is clear in Reg1 |

### Branch Quick Reference: "To check if operand is ... than r1"

| To Check If... | Use This Instruction | Why |
|----------------|---------------------|-----|
| `r1 > 10` | `QBLT myLabel, r1, 10` | Branch if 10 < r1 |
| `r1 >= 10` | `QBLE myLabel, r1, 10` | Branch if 10 <= r1 |
| `r1 < 10` | `QBGT myLabel, r1, 10` | Branch if 10 > r1 |
| `r1 <= 10` | `QBGE myLabel, r1, 10` | Branch if 10 >= r1 |
| `r1 == 10` | `QBEQ myLabel, r1, 10` | Branch if 10 == r1 |
| `r1 != 10` | `QBNE myLabel, r1, 10` | Branch if 10 != r1 |

**Examples:**
```assembly
qbgt myLabel, r2.w0, 5      ; Branch if 5 > r2.w0
qbge myLabel, r3, r4        ; Branch if r4 >= r3
qblt myLabel, r2.w0, 5      ; Branch if 5 < r2.w0
qbeq myLabel, r3, r4        ; Branch if r4 == r3
qbbs myLabel, r31, 5        ; Branch if bit 5 set in r31
qbbc myLabel, r31.b1, 5     ; Branch if bit 5 clear in r31.b1
```

## Control Operations

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| **HALT** | `HALT` | Halt PRU operation (for breakpoints) |
| **SLP** | `SLP IM(1)` | Sleep PRU: 0=permanent, 1=wake on event |
| **WBS** | `WBS Reg1, OP(31)` | Wait until bit set (spin loop) |
| **WBC** | `WBC Reg1, OP(31)` | Wait until bit clear (spin loop) |
| **LOOP** | `LOOP Label, OP(256)` | Hardware-assisted loop (V3+ cores, non-interruptible) |

**Examples:**
```assembly
halt                        ; Stop PRU for debugging
slp 0                       ; Sleep permanently
slp 1                       ; Sleep until wake event
wbs r31, 30                 ; Spin until bit 30 set in r31
wbc r31, 5                  ; Spin until bit 5 clear in r31

; Hardware loop example. Loop setup takes one cycle
ldi r0.b0, 10
loop end_loop, r0.b0        ; Loop 10 times
    ; loop body here
end_loop:
```

## Register Notation

| Notation | Description | Example |
|----------|-------------|---------|
| **Rn** | 32-bit register (r0-r31) | `r5` = all 32 bits of register 5 |
| **Rn.b0** | Byte 0 (bits 0-7) | `r5.b0` = lowest byte |
| **Rn.b1** | Byte 1 (bits 8-15) | `r5.b1` = second byte |
| **Rn.b2** | Byte 2 (bits 16-23) | `r5.b2` = third byte |
| **Rn.b3** | Byte 3 (bits 24-31) | `r5.b3` = highest byte |
| **Rn.w0** | Word 0 (bits 0-15) | `r5.w0` = lower 16 bits |
| **Rn.w1** | Word 1 (bits 8-23) | `r5.w1` = middle 16 bits |
| **Rn.w2** | Word 2 (bits 16-31) | `r5.w2` = upper 16 bits |
| **Rn.tx** | Single bit x (t0-t31) | `r5.t7` = bit 7 of r5 |
| **Cn** | Constant table entry (c0-c31) | `c1` = constant table pointer 1 |

**Register Layout Example (r0):**
```
BIT:
31  30  29  28  27  26  25  24  23  22  21  20  19  18  17  16  15  14  13  12  11  10  9   8   7   6   5   4   3   2   1   0
|------------ r0.b3 -----------|------------ r0.b2 ------------|------------ r0.b1 -----------|------------ r0.b0 -----------|
|------------------------------ r0.w2 -------------------------|------------------------------- r0.w0 -----------------------|
                               |------------------------------ r0.w1 -------------------------|
|------------------------------------------------------------ r0 ------------------------------------------------------------|
```

## Special Registers

| Register | Purpose |
|----------|---------|
| **R30** | GPIO output / Direct output pins |
| **R31** | GPIO input / System event flags |

## Operand Limits

| Notation | Range | Used For |
|----------|-------|----------|
| **OP(31)** | 0-31 | Shift amounts, bit positions |
| **OP(255)** | 0-255 | General operands (immediate or register) |
| **OP(256)** | 0-256 | Loop iteration counts |
| **IM(124)** | 0-124 | Memory transfer lengths |
| **IM(253)** | 0-253 | XFR device IDs |
| **IM(65535)** | 0-65535 | Immediate values, addresses |

## Important Notes

- **All operations are 32-bit** with zero extension [2]
- **Carry bit position** depends on destination register width [2]
- **One instruction = 4 bytes** [2]
- **Branch comparisons** use reversed operand order: 3rd compared to 2nd [2]
- **Memory operations** with register count: do NOT use 0 - will hang PRU [2]
- **MVI instructions** only available on V2+ cores [2]
- **LOOP instruction** only available on V3+ cores [2]
- **TSEN instruction** only available on V4+ cores with ICSS_G
 [2]

## Core Revision Differences

| Feature | V1 | V2 | V3 | V4 (ICSS_G) |
|---------|----|----|-----|-------------|
| Basic instruction set | yes | yes | yes | yes |
| MVIB/W/D indirect | Limited | yes | yes | yes |
| LOOP hardware assist | - | - | yes | yes |
| FILL instruction | - | - | yes | yes |
| Improved SLP | - | - | yes | yes |
| TSEN (Task Manager) | - | - | - | yes |
| SCAN (deprecated) | yes | - | - | - |

## Common Code Patterns

```assembly
; Set bit 5 in r1
set r1, r1, 5

; Clear bit 3 in r2.b0
clr r2.b0, r2.b0, 3

; Toggle bit 7 in r3
xor r3, r3, (1<<7)

; Wait for event flag bit 30
wbs r31, 30

; Copy 8 bytes from memory to registers
lbbo &r2, r1, 5, 8          ; mem[r1+5] → r2:r3

; Store 12 bytes from registers to memory
sbbo &r5, r10, 0, 12        ; r5:r6:r7 → mem[r10]

; Indirect register file access
ldi r1.b0, &r5              ; r1.b0 = offset of r5 (20)
mvid r2, *r1.b0++           ; r2 = r5, r1.b0 = 24

; Hardware loop (V3+)
ldi r0.b0, 10
loop end_loop, r0.b0
    ; loop body - executes 10 times
end_loop:

; Enable task manager (V4+ ICSS_G only)
tsen 0                      ; Enable task manager

; Compare and branch
ldi r1, 100
qblt continue, r1, 50       ; if 50 < 100, branch to continue
    ; r1 <= 50
    jmp done
continue:
    ; r1 > 50
done:
```

---

**Reference:** TI PRU Assembly Instruction User Guide (SPRUIJ2) [2]

**Note:** V4 core features (TSEN) are specific to devices with ICSS_G subsystem. Consult your device-specific documentation for availability and usage details.
