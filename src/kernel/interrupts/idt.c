#include "idt.h"

idt_entry_t idt[256];
idt_ptr_t idtp;

extern void idt_load(uint32_t);

extern void isr0();
extern void irq0();
extern void irq1();

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_high = (base >> 16) & 0xFFFF;
}

void idt_init() {
    idtp.limit = sizeof(idt_entry_t) * 256 - 1;
    idtp.base = (uint32_t)&idt;

    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0, 0);
    }

    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(32, (uint32_t)irq0, 0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);

    __asm__ volatile("lidt (%0)" :: "r"(&idtp));
    __asm__ volatile("sti");
}

