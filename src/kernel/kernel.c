#include "interrupts/idt.h"
#include "interrupts/irq.h"
#include "timer/pit.h"
#include "memory/heap.h"

#include "cpu/io.h"

#include "drivers/vga/vga.h"
#include "drivers/keyboard/keyboard.h"
#include "filesystem/fs.h"

extern void* multiboot_info_ptr;

void shell_run(void);
void panic(const char* msg);

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info) {
    (void)multiboot_info;

    vga_init();
    keyboard_init();

    irq_remap();
    idt_init();

    pit_init(100);
    heap_init();

    detect_disk();
    fs_init();

    if (multiboot_magic != 0x2BADB002) {
        vga_warn("Not loaded by a Multiboot-compliant bootloader!");
    }

    shell_run();
    
    halt();
}

void panic(const char* msg) {
    vga_error(("KERNEL PANIC: %s", msg));
    halt();
}