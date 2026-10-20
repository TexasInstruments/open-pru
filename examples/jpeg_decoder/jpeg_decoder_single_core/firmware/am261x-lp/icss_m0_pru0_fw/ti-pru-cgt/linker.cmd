-e main
--stack_size=0
--heap_size=0

MEMORY
{
    PAGE 0:
        PRU_IMEM    : org = 0x00000000  len = 0x00002F00
    PAGE 1:
        PRU_DMEM    : org = 0x00000100  len = 0x00001F00
}

SECTIONS
{
    .text       >  PRU_IMEM,    PAGE 0
    .bss        >  PRU_DMEM,    PAGE 1
    .data       >  PRU_DMEM,    PAGE 1
    .rodata     >  PRU_DMEM,    PAGE 1
}
