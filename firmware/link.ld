OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    . = 0x80000000;

    .text : {
        *(.text.init)
        *(.text .text.*)
    } > RAM

    . = ALIGN(4096);

    .rodata : {
        *(.rodata .rodata.*)
    } > RAM

    . = ALIGN(4096);

    .data : {
        *(.data .data.*)
        *(.sdata .sdata.*)
    } > RAM

    . = ALIGN(4096);

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        __bss_end = .;
    } > RAM

    . = ALIGN(4096);

    /* Heap: 16MB */
    __heap_start = .;
    . = . + 16M;
    __heap_end = .;

    /* Stack: 64KB */
    . = ALIGN(4096);
    __stack_bottom = .;
    . = . + 64K;
    __stack_top = .;

    _end = .;
}
