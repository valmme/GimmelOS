ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot ALIGN(4) : {
        *(.multiboot)
    }

    .text ALIGN(4K) : {
        *(.text*)
    }

    .rodata ALIGN(4K) : {
        *(.rodata*)
    }

    .data ALIGN(4K) : {
        *(.data*)
    }

    .bss ALIGN(4K) : {
        *(COMMON)
        *(.bss*)
    }

    _end = .;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}