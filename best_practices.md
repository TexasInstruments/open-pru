# PRU Assembly and C Best Practices Guide
## These best practices will be used as by Qodo Merge as context for evaluation during the Improve stage

## Introduction

This document outlines best practices for developing PRU (Programmable Real-time Unit) firmware for Texas Instruments Sitara devices. The PRU is a deterministic, real-time processor subsystem that provides precise timing control and low-latency I/O operations.

**Supported Devices:**
- AM243x, AM261x, AM263x, AM263px, AM64x series

## General Principles

### 1. Real-time Determinism
- **Predictable Timing**: Every instruction should have predictable execution time except load/store from outside ICSS IP
- **Avoid Conditional Branches**: When possible, use unconditional code paths
- **Cycle Counting**: Always document cycle counts for critical code sections
- **Interrupt Handling**: Minimize interrupt latency and processing time

### 2. Resource Management
- **Register Usage**: Efficiently utilize the 32 available registers (R0-R31). R30 and R31 are special purpose registers connected to IO and may not be available for general purpose use
- **Memory Conservation**: PRU has limited memory (8KB to 16KB instruction which is device dependent, 8KB data and 12KB/32KB/64KB shared data mem which is device dependent)
- **Power Efficiency**: Implement power-saving techniques when appropriate via sleep instruction

### 3. Code Maintainability
- **Modular Design**: Use macros and includes for reusable code
- **Clear Documentation**: Document timing requirements and hardware dependencies
- **Version Control**: Follow consistent naming and versioning conventions

## Assembly Code Best Practices
### 1. **Always Validate Memory Boundaries**
Before any memory access, ensure the address is within allocated bounds.

**Example:**
```asm
; Safe memory write with bounds checking
safe_write:
    ; Check if offset is within bounds
    ldi32   TEMP_REG_2, WRITE_OFFSET
    ldi32   TEMP_REG_3, MAX_BUFFER_SIZE
    qbgt    handle_error, TEMP_REG_3, TEMP_REG_2  ; Jump to error if offset > max
    
    ; Safe to write
    add     TEMP_REG_2, TEMP_REG_2, DATA_BUFFER_OFFSET ; DATA_BUFFER_OFFSET < 255
    sbco    &r_dataReg, c28, TEMP_REG_2, 4
    qba     continue_execution
    
handle_error:
    ; Set error flag and handle gracefully
    ldi     ERROR_REG, ERR_BOUNDS_VIOLATION
    qba     error_handler
```

### 2. **Use Constants for Buffer Limits**
Define and use constants for all memory boundaries.

**Example:**
```asm
; Define memory constants
.define DATA_BUFFER_START    0x1000
.define DATA_BUFFER_SIZE     0x400   ; 1KB buffer
.define DATA_BUFFER_END      0x1400  ; START + SIZE

; Validate access is within [START, END)
validate_address:
    ldi    TEMP_REG_2.w0, DATA_BUFFER_START
    qblt    addr_too_low, TEMP_REG_2.w0, TEMP_REG_1
    ldi    TEMP_REG_2.w0, DATA_BUFFER_END
    qbge    addr_too_high, TEMP_REG_2.w0, TEMP_REG_1
    ; Address is valid, continue...
```

### 3. **Implement Safe Loop Constructs**
Always include termination conditions and bounds checking in loops.

**Example:**
```asm
; Safe loop with bounds checking
safe_loop_start:
    ldi     LOOP_COUNTER, 0
    ldi     MAX_ITERATIONS, 100
    ldi     MAX_BUFFER_SIZE, 1024
    
loop_body:
    ; Check loop bounds
    qbeq    loop_done, LOOP_COUNTER, MAX_ITERATIONS
    
    ; Calculate safe offset
    lsl     TEMP_OFFSET, LOOP_COUNTER, 2  ; Multiply by 4 for word access
    qbgt    loop_done, MAX_BUFFER_SIZE, TEMP_OFFSET
    
    ; Safe memory access
    add     TEMP_ADDR, DATA_BUFFER_OFFSET, TEMP_OFFSET
    lbco    &dataReg, c28, TEMP_ADDR, 4
    
    ; Process data...
    
    ; Increment and continue
    add     LOOP_COUNTER, LOOP_COUNTER, 1
    qba     loop_body
    
loop_done:
    ; Clean exit
```

### 4. **Use Defensive Programming Techniques in use cases when performance considerations allow this or requirements mandate it**
Add sanity checks and fail-safe mechanisms.

**Example:**
```asm
; Defensive buffer access with multiple checks
secure_buffer_access:
    ; Validate pointer is not null
    qbeq    null_ptr_error, BUFFER_PTR, 0
    
    ; Validate size is reasonable
    ldi32   TEMP_REG_1, MAX_ALLOWED_SIZE
    qbgt    size_error, TEMP_REG_1, ACCESS_SIZE
    
    ; Calculate end address and check for overflow
    add     END_ADDR, BUFFER_PTR, ACCESS_SIZE
    qblt    overflow_error, BUFFER_PTR, END_ADDR  ; Overflow if end < start
    
    ; Verify end address is within bounds
    ldi32   TEMP_REG_2, MEMORY_LIMIT
    qbgt    bounds_error, END_ADDR, TEMP_REG_2
    
    ; All checks passed - safe to access
    lbco    &dataReg, c28, BUFFER_PTR, ACCESS_SIZE
```

### 5. **Document Memory Layout and Constraints**
Clear documentation prevents mistakes.

**Example:**
```asm
;==============================================================================
; Memory Layout:
; 0x0000 - 0x0FFF : Reserved system memory (DO NOT ACCESS)
; 0x1000 - 0x13FF : Data buffer (1KB)
; 0x1400 - 0x17FF : Stack space (1KB)
; 0x1800 - 0x1FFF : Shared memory region (2KB)
;==============================================================================

; Safe accessor macro for data buffer
.macro m_SAFE_DATA_ACCESS offset, size, dest_reg
    ; Validate offset + size <= buffer size
    ldi32   TEMP_CHECK, \offset + \size
    ldi32   TEMP_LIMIT, DATA_BUFFER_SIZE
    qbgt    memory_fault, TEMP_LIMIT, TEMP_CHECK
    
    ; Perform safe access
    ldi32   TEMP_ADDR, DATA_BUFFER_START + \offset
    lbco    &\dest_reg, c28, TEMP_ADDR, \size
.endm
```

### 6. **Implement Error Recovery**
Handle errors gracefully rather than crashing.

**Example:**
```asm
; Centralized error handler
error_handler:
    ; Log error type
    sbco    &ERROR_REG, c28, ERROR_LOG_ADDR, 4
    
    ; Reset to safe state
    ldi     BUFFER_PTR, DATA_BUFFER_START
    ldi     ACCESS_SIZE, 0
    
    ; Signal error to host
    ldi     r31.b0, PRU_ERROR_SIGNAL
    
    ; Return to safe execution point
    qba     safe_restart_point
```

### 7. **Use Static Analysis Where Possible**
Leverage assembler features to catch errors at build time.

**Example:**
```asm
; Use assembler assertions
.if (BUFFER_OFFSET + MAX_ACCESS_SIZE) > TOTAL_MEMORY_SIZE
    .emsg "Buffer configuration exceeds memory limits"
.endif

; Create safe access patterns
.macro BOUNDS_CHECKED_WRITE reg, offset
    .if (\offset >= MAX_BUFFER_SIZE)
        .emsg "Static bounds check failed: offset too large"
    .endif
    sbco    &\reg, c28, DATA_BUFFER_OFFSET + \offset, 4
.endm
```

### File Structure and Organization

```assembly
; Copyright header (required for TI projects)
; File description and purpose
; Build instructions
; Hardware requirements

; CCS/makefile specific settings
    .retain     ; Required for building .out with assembly file
    .retainrefs ; Required for building .out with assembly file

    .global     main
    .sect       ".text"

; Include files
    .include "icss_constant_defines.inc"
    .include "custom_macros.inc"

; Register aliases for clarity
    .asg    R2,     TEMP_REG
    .asg    R3.w0,  OFFSET_ADDR
    .asg    100000, DELAY_COUNT

; Main program
main:
    ; Implementation
```

### Register Management

#### Register Allocation Strategy
```assembly
; Use consistent register aliases
    .asg    R0,     DATA_REG
    .asg    R1,     COUNTER_REG
    .asg    R2,     TEMP_REG
    .asg    R3,     ADDRESS_REG
    ; R30 - GPO (General Purpose Output)
    ; R31 - GPI (General Purpose Input)
```

#### Register Initialization
```assembly
init:
    ; Always clear register space at startup
    zero    &r0, 120    ; Clear R0-R29 (30 registers × 4 bytes)
    
    ; Initialize specific registers
    ldi     DATA_REG, 0
    ldi     COUNTER_REG, 0
```

### Timing and Performance

#### Cycle Counting Documentation
```assembly
;************************************************************************************
;   Macro: m_delay_cycles
;   
;   PEAK cycles: 3 + DELAY_COUNT cycles
;   
;   Parameters:
;       DELAY_COUNT - Number of cycles to delay
;************************************************************************************
m_delay_cycles .macro DELAY_COUNT
    ldi32   r5, DELAY_COUNT     ; 2 cycles
delay_loop?:
    sub     r5, r5, 1          ; 1 cycle
    qbne    delay_loop?, r5, 0 ; 1 cycle (when branching)
    .endm
```

#### Efficient Loop Structures
```assembly
; Preferred: LOOP instruction when feasible (more efficient)
    loop     loop_end, LOOP_COUNT
loop_start:
    ; Loop body
loop_end:

; Preferred second option: Down-counting loops 
    ldi     COUNTER_REG, LOOP_COUNT
loop_start:
    ; Loop body
    sub     COUNTER_REG, COUNTER_REG, 1
    qbne    loop_start, COUNTER_REG, 0

; Avoid: Up-counting loops (less efficient)
; ldi     COUNTER_REG, 0
; qbge    loop_end, LOOP_COUNT, COUNTER_REG
```

### Macro Design

#### Parameterized Macros
```assembly
;************************************************************************************
;   Macro: m_spi_transfer
;   
;   Description: Transfer data via SPI interface
;   
;   PEAK cycles: (7 + DELAY1 + DELAY2) × PACKET_SIZE
;   
;   Parameters:
;       dataReg     - Register containing data to send
;       PACKET_SIZE - Size of data packet in bits (1-32)
;       SCLK_PIN    - PRU pin number for SCLK
;       DELAY1      - High phase delay cycles
;       DELAY2      - Low phase delay cycles
;   
;   Assumptions:
;       - CS must be asserted before calling
;       - SCLK_PIN configured as output
;       - PACKET_SIZE validated (1-32 bits)
;************************************************************************************
m_spi_transfer .macro dataReg, PACKET_SIZE, SCLK_PIN, DELAY1, DELAY2
    ; Parameter validation
    .if (PACKET_SIZE > 32) | (PACKET_SIZE < 1)
    .emsg "Error: PACKET_SIZE must be 1-32 bits"
    .endif
    
    ; Implementation
    ldi     r10, PACKET_SIZE - 1
transfer_loop?:
    ; Transfer logic here
    .endm
```

#### Macro Naming Conventions
- Prefix with `m_` for macros
- Use descriptive names: `m_spi_read_packet`, `m_gpio_toggle`
- Include parameter validation
- Use unique local labels with `?` suffix

### GPIO and Pin Control

#### Pin Manipulation
```assembly
; Use macros for pin operations
m_pru_set_pin   .macro PIN_NUM
    set     r30, r30, PIN_NUM
    .endm

m_pru_clr_pin   .macro PIN_NUM
    clr     r30, r30, PIN_NUM
    .endm

; Usage
    m_pru_set_pin   4   ; Set GPIO pin 4
    m_pru_clr_pin   4   ; Clear GPIO pin 4
```

#### Pin State Reading
```assembly
; Read input pins from R31
    qbbc    pin_low, r31, PIN_NUM   ; Branch if bit clear
    ; Pin is high
    qba     continue
pin_low:
    ; Pin is low
continue:
```

### Memory Access Patterns

#### Efficient Memory Operations
```assembly
; Use appropriate addressing modes
    lbbo    &r2, r1, 0, 4      ; Load 4 bytes from address in r1
    sbbo    &r2, r1, 0, 4      ; Store 4 bytes to address in r1
    
; Burst operations for multiple bytes
    lbbo    &r2, r1, 0, 16     ; Load 16 bytes (4 registers)
```

## C Code Best Practices

### Host Application Structure

#### Standard Includes and Initialization
```c
#include <stdio.h>
#include <kernel/dpl/DebugP.h>
#include "ti_drivers_config.h"
#include "ti_drivers_open_close.h"
#include "ti_board_open_close.h"
#include <drivers/pruicss.h>

/* Include PRU firmware headers */
#include <pru0_load_bin.h>
#include <pru1_load_bin.h>
```

#### Error Handling
```c
void pru_application_main(void *args)
{
    int32_t status;
    
    /* Initialize drivers with error checking */
    Drivers_open();
    
    status = Board_driversOpen();
    DebugP_assert(SystemP_SUCCESS == status);
    
    /* PRU initialization */
    gPruIcssHandle = PRUICSS_open(CONFIG_PRU_ICSS0);
    DebugP_assert(gPruIcssHandle != NULL);
    
    /* Memory initialization */
    status = PRUICSS_initMemory(gPruIcssHandle, PRUICSS_DATARAM(PRUICSS_PRU0));
    DebugP_assert(status != 0);
    
    /* Firmware loading with size validation */
    status = PRUICSS_loadFirmware(gPruIcssHandle, PRUICSS_PRU0, 
                                  PRU0Firmware_0, sizeof(PRU0Firmware_0));
    DebugP_assert(SystemP_SUCCESS == status);
}
```

#### Resource Management
```c
/* Always clean up resources */
void cleanup_resources(void)
{
    if (gPruIcssHandle != NULL) {
        PRUICSS_close(gPruIcssHandle);
    }
    Board_driversClose();
    Drivers_close();
}
```

### PRU Communication

#### Shared Memory Access
```c
/* Define shared memory structures */
typedef struct {
    uint32_t command;
    uint32_t data[16];
    uint32_t status;
} pru_shared_mem_t;

/* Access shared memory safely */
void write_pru_command(uint32_t command, uint32_t *data, uint32_t length)
{
    volatile pru_shared_mem_t *shared_mem = 
        (volatile pru_shared_mem_t *)PRUICSS_getSharedMemAddr(gPruIcssHandle);
    
    /* Write data first */
    for (uint32_t i = 0; i < length && i < 16; i++) {
        shared_mem->data[i] = data[i];
    }
    
    /* Write command last to trigger PRU processing */
    shared_mem->command = command;
}
```

## Project Structure

### Directory Organization
```
project_name/
├── .project/                   # Build configuration
│   ├── project.js
│   └── project_device.js
├── firmware/                   # PRU firmware
│   ├── main.asm               # Main assembly file
│   ├── macros.inc             # Custom macros
│   └── device_specific/       # Device-specific code
├── host_app/                  # R5F/A53 application
│   └── main.c
├── include/                   # Shared headers
├── docs/                      # Documentation
└── README.md
```

### File Naming Conventions
- Assembly files: `main.asm`, `spi_driver.asm`
- Include files: `custom_macros.inc`, `register_defines.inc`
- C files: `pru_application.c`, `spi_interface.c`
- Use lowercase with underscores

## Performance Optimization

### Critical Path Optimization

#### Minimize Branch Instructions
```assembly
; Preferred: Branchless code
    qbbc    skip_set, r31, INPUT_PIN
    set     r30, r30, OUTPUT_PIN
skip_set:
    clr     r30, r30, OUTPUT_PIN

; Instead of multiple conditional branches
```

#### Loop Unrolling for Critical Sections
```assembly
; For small, fixed iteration counts
    ; Unrolled loop (4 iterations)
    operation_macro PARAM1
    operation_macro PARAM2
    operation_macro PARAM3
    operation_macro PARAM4

; Instead of:
;   ldi r1, 4
; loop:
;   operation_macro
;   sub r1, r1, 1
;   qbne loop, r1, 0
```

### Memory Access Optimization

#### Burst Transfers
```assembly
; Load multiple registers in one operation (except in scenarios which requires predicatble task manager switching latency as ongoing burst instruction from lowee priority back ground task can stall task switch)
    lbbo    &r2, r1, 0, 16      ; Load r2, r3, r4, r5 (16 bytes)
    
; Instead of individual loads
;   lbbo    &r2, r1, 0, 4
;   lbbo    &r3, r1, 4, 4
;   lbbo    &r4, r1, 8, 4
;   lbbo    &r5, r1, 12, 4
```

## Debugging and Testing

### Debug Strategies

#### Instrumentation Points
```assembly
; Add debug outputs for timing analysis
debug_point_1:
    set     r30, r30, DEBUG_PIN_1   ; Mark start of critical section
    ; Critical code here
    clr     r30, r30, DEBUG_PIN_1   ; Mark end of critical section

; Add debug outputs for timing analysis - alternatively use edio when available (slower but predictable latency < 4 PRU cycles)

; Add debug outputs for timing analysis - if use case allows and 100+ ns jitter is ok, can also use SoC GPIO
```

#### Register State Logging
```c
// In host application
void log_pru_registers(void)
{
    uint32_t *pru_regs = (uint32_t *)PRUICSS_getDataMemAddr(gPruIcssHandle, PRUICSS_PRU0);
    
    DebugP_log("PRU Register Dump:\n");
    for (int i = 0; i < 30; i++) {
        DebugP_log("R%d: 0x%08x\n", i, pru_regs[i]);
    }
}
```

### Testing Framework

#### Unit Testing Macros
```assembly
; Test macro for validation
m_test_assert .macro condition, error_code
    .if condition
    ; Test passed
    .else
    ldi     r30.b0, error_code
    halt
    .endif
    .endm
```

## Documentation Standards

## General Remarks
1. The basic indentation must be 4 spaces.
2. Single spaces and newlines are the only allowed white space characters.
3. Unix end-of-line character ("\n" or ASCII LF (0x0A)) will be used on all files.
4. All source files must contain a header denoting the file name.
5. Any reference to a file name shall match the capitalization of the actual file name.
6. All source and header files must contain a copyright statement attributing the files to Texas Instruments.
7. No random blank line pattern must exist in the code.
8. Use "for example" instead of "e.g.".
9. Use "that is" instead of "i.e.".
10. Use "and so on" instead of "etc,".
11. No text beyond column 79 in source files.
    - Column 79 is the "soft" limit.
    - Long lines of code are more difficult to follow and interrupt code flow.
    - There shall be no text beyond column 129.
    - This is the "hard" limit.
    - Lines longer than column 79 should be the exceptional case in any source file.

## Files
1. The extension for clpru (CGT PRU) source files shall be ".asm".
2. The extension for pasm source files shall be ".p".
3. The extension for clpru header files shall be ".h".
4. The extension for pasm header files shall be ".hp" or ".h".
   - It is recommended to create separate sources for functions/macros which can be grouped in terms of feature, function etc to enable reuse
   - It is recommended to create separate headers for macros which can be grouped together wherever possible

## Programming Guidelines

### Register allocation
Registers are most critical resource in PRU assembly programs. Since they are scarce - often error prone as multiple portions of program may reuse same register or part of register and this unintended corruption leads to really hard to debug issues in practice. Documenting function and macros helps to avoid this issues but since multiple developers may be using the same code base and reuse of functions and macros across different firmware programs - this requires stricter discipline for compliance.

Naming registers using macros like define, .set, .asg is common practice and considered to be helping with readability. This guideline strongly discourages this practice. The recommended approach is below (more in line of object oriented programming principles).

This guideline recommends to avoid direct references to PRU registers (even via define, .set, .asg directives) and always access them as structure datatype. clpru requires to use `.struct` and `.sassign` directives for this purpose. This forces alignment checks, same structure can be re-used in different context and can be mapped to different set of registers. Another important advantage is that this makes it easy to check for register overlaps via monitoring .sassign declarations and could be automated or built into assembler warnings of some sort.

```assembly
Reg_params_s .struct
x_u .union
x .uint
.struct
w0 .ushort
w2 .ushort
.endstruct
.endunion
y .ubyte
z .ubyte
.endstruct

Reg .sassign R20, Reg_params_s ; Assign R20 to Reg_params_s

M_MOV32 .macro ARG1, ARG2
LDI ARG1.w0 , :ARG2: & 0xFFFF 
LDI ARG1.w2 , :ARG2: >> 16 
.endm

main:
M_MOV32 Reg.x_u, 0 
```

Note that due to a clpru issue - one can't access register modifier suffix (.b0, .w0 etc) from structure elements. In a way this lack of support can be considered like protection against unintended/unwanted access from a different context. Programmer can always expose desired byte, halfword access via union.

## Documentation
NaturalDocs (Perl based cross platform and easy to use HTML documentation generator) is recommended for PRU assembly documentation. For detailed documentation tips - refer to https://www.naturaldocs.org/reference/

Following information is mandatory to be documented for functions and macros:
- Pseudo code: To enable quality code reviews and improved readability of assembly source
- Peak Cycle Budget: Worst case cycle budget - profiled using timer/cycle counter or hand counted. Most common hard to debug issues in PRU firmware are due to cycle budget overflow during execution. This is the only reliable way to monitor such issues systematically and in an organized manner.
- Registers modified: Including macros invoked within shall be documented clearly as register corruption is another common cause behind hard to debug issues

Quick Tips:
- Use <function_name> or <macro_name> to create HTML reference from description
- Use (start code), (end code) to highlight the code block. For small blocks - start the line with >, I or :
- _text_to_display_ to underline and *text_to_display* for bold

Each developer updating an existing function or macro needs to update above fields before committing the changes to repository for code review.

## Extended Guidelines
This guideline does not intend to make an exhaustive requirement for naming of variables in this document. The predominant methods are styles like "CamelCase" as well as styles that use underscore between lowercase words. Either is acceptable. The requirements here are intentionally simplified as naming conventions tend to be fairly set for existing code, and changing them will not generally improve the quality. Some key guidelines for all new code are documented below:

- All functions are recommended to start with prefix FN_<function_name>
  - Example: FN_RCV_FB
  - Must be all uppercase and in underscore_case format.
  - This recommended to use `.asmfunc` and `.endasmfunc` directives to mark start and end of function.

- All tasks are recommended to be in the given format with fixed prefix and suffix - FN_<task_name>_TASK
  - Example: FN_RX_EOF_TASK
  - Must be all uppercase and in underscore_case format.
  - This recommended to use `.asmfunc` and `.endasmfunc` directives to mark start and end of task.

- All macros are recommended to start with prefix m_<macro_name>.
  - Above guideline is for standard macros (or macros for ethernet_switch).
  - It is recommended to have a secondary prefix for protocol specific macros. Example - m_pn_<macro_name> for PROFINET.
  - Macro name must be all lowercase and in underscore_case format.

- All labels must be uppercase and in underscore_case format.
  - It is recommended to use task/function/macro short name as prefix for label.
  - Example: FN_RX_EOF_TASK → Labels start with RX_EOF_<>.
  - Prefer line break before label for better readability.

- All instruction operation code must be in lowercase - example xin instead of XIN, or in consistency with the existing code.
  - White space (5 spaces for 3-letter operations and 4 spaces for 4-letter operations) shall provide better readability.
  - Example:
    ```
    xin     PRU_SPAD_B0_XID, &MII_RCV, $sizeof(MII_RCV)
    qbbs    RX_EOF_YIELD, MII_RCV.rx_status, rx_frame_error
    ```
    instead of
    ```
    xin PRU_SPAD_B0_XID, &MII_RCV, $sizeof(MII_RCV)
    qbbs RX_EOF_YIELD, MII_RCV.rx_status, rx_frame_error
    ```

- Structure names must of the form Struct_<structure_name>.
  - structure_name must be all uppercase and in underscore_case format.
  - Example: Struct_MII_RCV where structure_name = MII_RCV
  - structure_name can then be used for register assignment.
  - Example: MII_RCV .sassign R21, Struct_MII_RCV

- Only FIXME markers should be used for pending work/bugs. No TODO markers to be used.
  - This will help for easy identification in firmware using single marker.

## Use of forward declarations in assembly
Compiler does not support forward declarations of symbols via the .set directive in an assembly file. Hence programmer must ensure that all symbols used as sources for .set directives are defined before they are used.

## Header File include rules
- It is recommended to include only header files required in .asm sources
- Four underscores recommended in header guard, example below:
  ```
  .if !$isdefed("____icss_header_h")
  ____icss_header_h   .set     1
  
  .endif ; ____icss_header_h
  ```
### File Headers
```assembly
;************************************************************************************
;   File:     filename.asm
;   
;   Brief:    Brief description of file purpose
;   
;   Author:   Developer Name
;   Date:     YYYY-MM-DD
;   Version:  X.Y.Z
;   
;   Hardware: Supported devices (AM243x, AM64x, etc.)
;   
;   Dependencies:
;       - icss_constant_defines.inc
;       - custom_macros.inc
;   
;   Build Instructions:
;       CCS: Import project and build
;       Makefile: make -C firmware all
;   
;   Performance:
;       Critical path: X cycles
;       Memory usage: Y bytes
;       
;   Notes:
;       - Special considerations
;       - Known limitations
;************************************************************************************
```

### Function/Macro Documentation
```assembly

;************************************************************************************
;   Macro: m_function_name
;   
;   Description:
;       Detailed description of what the macro does
;
;   Pseudo code:
;
;   PEAK cycles: X cycles (breakdown if complex)
;   Memory usage: Y bytes
;   
;   Parameters:
;       param1 - Description and valid range
;       param2 - Description and constraints
;   
;   Returns:
;       Description of return values or side effects
;   
;   Assumptions:
;       - List all assumptions about system state
;       - Hardware configuration requirements
;       - Register usage assumptions
;   
;   Side Effects:
;       - Modified registers
;       - Changed pin states
;       - Memory modifications
;   
;   Example:
;       m_function_name r1, 8, PIN_4
;************************************************************************************

;************************************************************************************
;
;   Macro: m_store_bytes_n_advance
;
;  Store num_bytes to (*c28+write_ptr) offset from register file (reg_src)
;
;   PEAK cycles:
;      2+(num_bytes/4)
;
;   Invokes:
;       None
;
;   Pseudo code:
;      (start code)
;       for (i=0; i < num_bytes; i++) {
;           *(uint8_t*)(c28+write_ptr+i) = *(uint8_t*)(&reg_src); 
;       }
;       write_ptr += num_bytes;
;      (end code)
;
;   Parameters:
;      reg_src : First register in the register file
;      write_ptr : Write pointer register (Offset in shared memory)
;      num_bytes : Number of bytes to write (32 or b0)
;    
;   Returns:
;      None
;
;   See Also:
;
;************************************************************************************
m_store_bytes_n_advance .macro reg_src, write_ptr, num_bytes
    .var tmp
    .asg :num_bytes(1):, tmp ; extract first char to tmp
    .if $symcmp(tmp,"b") = 0 ;variable length : .b0, .b1, .b2, .b3
    sbco &reg_src, c28, write_ptr, num_bytes; r20.w0 is read pointer
    add write_ptr, write_ptr, r0.:num_bytes: ; Increment bytes read
    .else ; fixed length : 1..255
    sbco &reg_src, c28, write_ptr, num_bytes; r20.w0 is read pointer
    add write_ptr, write_ptr, num_bytes; Increment bytes read
    .endif
    .endm
 
```

## Common Pitfalls

### Assembly Code Pitfalls

#### 1. Register Corruption
```assembly
; Problem: Not preserving registers
function_call:
    ; Using r1 without saving
    ldi     r1, 0x1234
    ; ... function body
    jmp     r3.w2

; Solution: Save and restore registers
function_call:
    ; Save registers on stack or in unused registers
    mov     r29, r1         ; Save r1
    ldi     r1, 0x1234
    ; ... function body
    mov     r1, r29         ; Restore r1
    jmp     r3.w2
```

#### 2. Timing Assumptions
```assembly
; Problem: Assuming fixed timing without measurement
delay_loop:
    sub     r1, r1, 1
    qbne    delay_loop, r1, 0
    ; Assuming this takes exactly X microseconds

; Solution: Measure and document actual timing
; Measured timing: 3 cycles per iteration at 200MHz = 15ns per iteration
delay_loop:
    sub     r1, r1, 1      ; 1 cycle
    qbne    delay_loop, r1, 0  ; 2 cycles when branching
```

#### 3. Macro Parameter Validation
```assembly
; Problem: No parameter validation
m_bad_macro .macro PARAM
    ldi     r1, PARAM
    .endm

; Solution: Always validate parameters
m_good_macro .macro PARAM
    .if (PARAM > MAX_VALUE) | (PARAM < MIN_VALUE)
    .emsg "Error: PARAM out of range"
    .endif
    ldi     r1, PARAM
    .endm
```

### C Code Pitfalls

#### 1. Shared Memory Race Conditions
```c
// Problem: Race condition in shared memory access
shared_mem->data = new_value;
shared_mem->flag = 1;  // PRU might read old data

// Solution: Use proper ordering and barriers
shared_mem->data = new_value;
__sync_synchronize();  // Memory barrier
shared_mem->flag = 1;
```

#### 2. Resource Leaks
```c
// Problem: Not cleaning up on error paths
int init_pru(void) {
    handle = PRUICSS_open(CONFIG_PRU_ICSS0);
    if (some_error_condition) {
        return -1;  // Leaked handle!
    }
    return 0;
}

// Solution: Proper cleanup
int init_pru(void) {
    handle = PRUICSS_open(CONFIG_PRU_ICSS0);
    if (some_error_condition) {
        PRUICSS_close(handle);
        return -1;
    }
    return 0;
}
```
# PRU Instruction Set Cheat Sheet

## Overview

The PRU (Programmable Real-Time Unit) is a 32-bit RISC processor with a small, deterministic instruction set. This cheat sheet provides a quick reference for PRU assembly programming.

## Instruction Categories

PRU instructions are divided into four major categories:
- **Data Movement**: Instructions that move data in/out of processor registers
- **Arithmetic**: Instructions that perform arithmetic operations
- **Logical**: Instructions that perform logical operations
- **Program Flow**: Instructions that control program flow

## Register Information

- 32 general-purpose 32-bit registers (R0-R31)
- Special registers R25-R29 are connected to the MAC (Multiply and Accumulate) unit

## Instruction Syntax

```
[label:] INSTRUCTION REG1, REG2, OP(value) [; comment]
```

## Data Movement Instructions

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| LDI | `LDI REG, IMM` | Load immediate value into register |
| LBBO | `LBBO &REG, REG, IMM/REG, IMM` | Load bytes with constant table offset |
| LBCO | `LBCO &REG, REG, IMM/REG, IMM` | Load bytes from constant table |
| SBBO | `SBBO &REG, REG, IMM/REG, IMM` | Store bytes with constant table offset |
| SBCO | `SBCO &REG, REG, IMM/REG, IMM` | Store bytes to constant table |
| MOV | `MOV REG, REG/IMM` | Move data between registers |
| XIN | `XIN IMM(BSID), &REG, IMM` | Transfer data from external device to registers |
| XOUT | `XOUT IMM(BSID), &REG, IMM` | Transfer data from registers to external device |
| XCHG | `XCHG IMM(BSID), &REG, IMM` | Exchange data with external device |

## Arithmetic Instructions

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| ADD | `ADD REG1, REG2, REG/IMM` | Unsigned integer addition |
| ADC | `ADC REG1, REG2, REG/IMM` | Unsigned integer addition with carry |
| SUB | `SUB REG1, REG2, REG/IMM` | Unsigned integer subtraction |
| SUC | `SUC REG1, REG2, REG/IMM` | Unsigned integer subtraction with carry |
| RSB | `RSB REG1, REG2, REG/IMM` | Reverse subtraction |
| RSC | `RSC REG1, REG2, REG/IMM` | Reverse subtraction with carry |
| MIN | `MIN REG1, REG2, REG/IMM` | Minimum of two values |
| MAX | `MAX REG1, REG2, REG/IMM` | Maximum of two values |

## Logical Instructions

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| AND | `AND REG1, REG2, REG/IMM` | Bitwise AND |
| OR | `OR REG1, REG2, REG/IMM` | Bitwise OR |
| XOR | `XOR REG1, REG2, REG/IMM` | Bitwise XOR |
| NOT | `NOT REG1, REG2` | Bitwise NOT |
| LSL | `LSL REG1, REG2, REG/IMM` | Logical shift left |
| LSR | `LSR REG1, REG2, REG/IMM` | Logical shift right |
| CLR | `CLR REG1, REG2, REG/IMM` | Clear bits |
| SET | `SET REG1, REG2, REG/IMM` | Set bits |
| LMBD | `LMBD REG1, REG2, REG/IMM` | Leftmost bit detection |

## Program Flow Control Instructions

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| JMP | `JMP LABEL/REG` | Unconditional jump |
| JAL | `JAL REG, LABEL/REG` | Jump and link (store return address) |
| QBGT | `QBGT LABEL, REG, REG/IMM` | Quick branch if REG/IMM greater than REG |
| QBGE | `QBGE LABEL, REG, REG/IMM` | Quick branch if REG/IMM greater than or equal REG |
| QBLT | `QBLT LABEL, REG, REG/IMM` | Quick branch if REG/IMM less than REG |
| QBLE | `QBLE LABEL, REG, REG/IMM` | Quick branch if REG/IMM less than or equal REG  |
| QBEQ | `QBEQ LABEL, REG, REG/IMM` | Quick branch if equal |
| QBNE | `QBNE LABEL, REG, REG/IMM` | Quick branch if not equal |
| QBA | `QBA LABEL` | Quick branch always |
| QBBS | `QBBS LABEL, REG, REG/IMM` | Quick branch if bit is set |
| QBBC | `QBBC LABEL, REG, REG/IMM` | Quick branch if bit is clear |
| LOOP | `LOOP LABEL, REG/IMM` | Loop specified number of times |
| HALT | `HALT` | Halt execution |
| SLP | `SLP IMM` | Sleep (low power mode) |

## Special Instructions

| Instruction | Syntax | Description |
|-------------|--------|-------------|
| MVI | `MVI REG, REG/IMM` | Move immediate value to register |
| ZERO | `ZERO REG, IMM` | Zero out a register |

## Register Field Selection

When accessing registers, you can select specific fields:

| Selector | Description |
|----------|-------------|
| `.b0` | Bits 7:0 (byte 0) |
| `.b1` | Bits 15:8 (byte 1) |
| `.b2` | Bits 23:16 (byte 2) |
| `.b3` | Bits 31:24 (byte 3) |
| `.w0` | Bits 15:0 (word 0) |
| `.w1` | Bits 31:16 (word 1) |
| `.w2` | Bits 23:8 (middle word) |

## Example Code

```assembly
; Simple example to add two numbers
    LDI     R0, 5       ; Load immediate value 5 into R0
    LDI     R1, 10      ; Load immediate value 10 into R1
    ADD     R2, R0, R1  ; Add R0 and R1, store result in R2

; Example of a loop
    LDI     R3, 0       ; Initialize counter
    LDI     R4, 10      ; Set loop limit
LOOP_START:
    ADD     R3, R3, 1   ; Increment counter
    QBLT    LOOP_START, R3, R4  ; Loop if R4 < R3

; Example of bit manipulation
    LDI     R5, 0x0F    ; Load 0x0F into R5
    SET     R5, R5, 8   ; Set bit 8 in R5
    CLR     R5, R5, 0   ; Clear bit 0 in R5
```

## Assembler Directives

| Directive | Description |
|-----------|-------------|
| `.set` | Define a symbol |
| `.text` | Start of code section |
| `.data` | Start of data section |
| `.struct` | Define a structure |
| `.global` | Make a symbol global |
| `.byte` | Define byte values |
| `.short` | Define 16-bit values |
| `.long` | Define 32-bit values |
| `.string` | Define a string |
| `.list` | Start source listing |
| `.nolist` | Stop source listing |

## Notes

- PRU instructions execute in a single cycle (except for multi-cycle instructions)
- The PRU has a deterministic execution model
- Register operations are 32-bit wide (with 33-bit results for arithmetic operations)
- Source values are zero-extended prior to operations

### General Pitfalls

#### 1. Inadequate Testing
- **Problem**: Testing only happy path scenarios
- **Solution**: Test edge cases, error conditions, and boundary values

#### 2. Poor Version Control
- **Problem**: No clear versioning or change tracking
- **Solution**: Use semantic versioning and detailed commit messages

#### 3. Insufficient Documentation
- **Problem**: Code without timing specifications or hardware requirements
- **Solution**: Document all timing-critical sections and hardware dependencies

---



