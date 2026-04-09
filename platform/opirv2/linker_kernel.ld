ENTRY(_start)

MEMORY
{
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 16M
}

SECTIONS
{
    . = ORIGIN(RAM);
    __kernel_start = .;

    .text : { 
        *(.text.init)
        *(.text*) 
    } > RAM

    .rodata : { 
        *(.rodata*) 
    } > RAM

    .data : { 
        *(.data*) 
    } > RAM

    .bss : { 
        __bss_start = .;
        *(.sbss*) *(.bss*)
        __bss_end = .; 
    } > RAM

    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
    __kernel_end = __stack_top;
}
