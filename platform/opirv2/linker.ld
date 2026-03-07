ENTRY(_start)

MEMORY
{
  RAM (rwx) : ORIGIN = 0x00200000, LENGTH = 64M
}

SECTIONS
{
    . = 0x00200000;

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
}
