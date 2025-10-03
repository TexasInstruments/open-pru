# Linker command files for C projects

These linker command files can be used as a starting point for PRU firmware that
is written in C, or mixed C and assembly.

## More information

For training on how to customize the linker.cmd file for your project, refer to
the PRU Academy.

## Common modifications

Until these examples are ported to OpenPRU, we will refer to the older PRU
Software Support Package (PSSP).
<PSSP_PATH> = https://git.ti.com/cgit/pru-software-support-package/pru-software-support-package/tree

### Add the INTC map (Linux only)

The INTC map is used in order to allow the Linux PRU remoteproc driver to
configure the PRU's interrupt controller (INTC) during initialization.

1. Add the INTC map file to the project. Use this file as a template:
   <PSSP_PATH>/examples/<processor>/PRU_Direct_Connect0/intc_map_0.h

2. Include the INTC map file in the main.c file.

```
#include "intc_map_0.h"
```

3. Add the INTC map structure to the linker command file. Reference
   <PSSP_PATH>/examples/<processor>/PRU_Direct_Connect0/xxx_PRU0_intc.cmd

```
/* Specify the sections allocation into memory */
SECTIONS {

  ...

  .pru_irq_map (COPY) :
  {
          *(.pru_irq_map)
  }
}
```

### Add a resource table (Linux only)

The resource table is used to pass information between the PRU and the Linux
PRU remoteproc driver while the remoteproc driver is initializing the PRU core.
A resource table is only needed if the PRU core uses the RPMsg inter-processor
communication protocol (IPC) to communicate with Linux. If RPMsg is not used,
then a resource table is not needed.

1. Add a resource table file to the project. Use this file as a template:
   <PSSP_PATH>/examples/<processor>/xxx_RPMsg_Echo_InterruptX/resource_table.h

2. Include the resource table in the main.c file:

```
#include "resource_table.h"
```

3. Add the resource table to the linker command file. Reference
   <PSSP_PATH>/examples/<processor>/xxx_RPMsg_Echo_InterruptX/xxx_PRU0_intc_rscTbl.cmd

For example, on AM64x RTU1:
```
 /* Ensure resource_table section is aligned on 8-byte address for
   ARMv8 (64-bit) kernel */
.resource_table : ALIGN (8) >  RTU1_DMEM_1, PAGE 1
```

4. The resource table includes rsc_types.h. Add the include path for rsc_types.h
   so that the compiler can find the header file. Path is:
   open-pru/source/include/c_code/linux/
