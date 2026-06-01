ENTRY(_start)

MEMORY
{
  /* 0x2000000 is offset where bootloader places kernel */
  VRAM (rwx) : ORIGIN = 0xFFFFFFC002000000, LENGTH = 16M
}

SECTIONS
{
    . = ORIGIN(VRAM);
    __kernel_start = .;

    .text : { 
        *(.text.init)
        *(.text*) 
    }

    .rodata : { 
        *(.rodata*) 
    }

    .data : { 
        *(.data*) 
    }

    .bss : { 
        __bss_start = .;
        *(.sbss*) *(.bss*)
        __bss_end = .; 
    }

    __stack_top = ORIGIN(VRAM) + LENGTH(VRAM);
    __kernel_end = __stack_top;
}
