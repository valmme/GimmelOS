bits 32

global isr0
global irq0
global irq1

extern pit_handler
extern keyboard_handler

isr0:
    cli
    pusha
.hang:
    hlt
    jmp .hang

irq0:
    cli
    pusha

    call pit_handler

    popa
    sti
    iretd

irq1:
    cli
    pusha

    call keyboard_handler

    popa
    sti
    iretd