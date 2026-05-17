#include "vga.h"
#include "keyboard.h"
#include "filesystem/fs.h"

extern void* multiboot_info_ptr;

void shell_run(void);

void kernel_main(uint32_t multiboot_magic, uint32_t multiboot_info) {
    (void)multiboot_info;

    vga_init();
    keyboard_init();
    fs_init();

    if (multiboot_magic != 0x2BADB002) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        vga_println("WARNING: Not loaded by a Multiboot-compliant bootloader!");
    }

    shell_run();
    
    __asm__ volatile("cli; hlt");
}