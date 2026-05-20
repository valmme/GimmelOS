#include "interrupts/idt.h"
#include "interrupts/irq.h"

#include "drivers/vga/vga.h"
#include "drivers/keyboard/keyboard.h"
#include "filesystem/fs.h"

extern void* multiboot_info_ptr;

void shell_run(void);

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info) {
    (void)multiboot_info;

    vga_init();
    keyboard_init();

    irq_remap();
    idt_init();

    detect_disk();
    fs_init();

    if (multiboot_magic != 0x2BADB002) {
        vga_warn("Not loaded by a Multiboot-compliant bootloader!");
    }

    shell_run();
    
    __asm__ volatile("cli; hlt");
}