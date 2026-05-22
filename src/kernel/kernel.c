#include "cpu/io.h"

#include "drivers/vga/vga.h"
#include "drivers/keyboard/keyboard.h"
#include "filesystem/fs.h"

void shell_run(void);

void panic(const char* msg) {
    vga_error("--- KERNEL PANIC ---");
    vga_error(msg);
    halt();
}

void kernel_main(uint32_t magic, uint32_t addr) {
    vga_init();
    keyboard_init();

    if (magic != 0x2BADB002) {
        vga_warn("Not multiboot");
    }

    struct multiboot_info* mbi = (struct multiboot_info*)addr;

    if (!(mbi->flags & (1 << 12))) {
        vga_error("No framebuffer!");
        return;
    }

    framebuffer = (uint32_t*)(uint32_t)mbi->framebuffer_addr;
    width = mbi->framebuffer_width;
    height = mbi->framebuffer_height;
    pitch = mbi->framebuffer_pitch;

    gfx_render_frame();
    shell_run();
}