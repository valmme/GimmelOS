#include "cpu/io.h"
#include "drivers/serial.h"

#include "drivers/vga/vga.h"
#include "drivers/vga/gfx.h"
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
    serial_init();
    keyboard_init();

    if (magic != 0x2BADB002) {
        vga_warn("Not multiboot");
    }

    struct multiboot_info* mbi = (struct multiboot_info*)addr;

    if (!(mbi->flags & (1 << 12))) {
        vga_error("No framebuffer!");
        return;
    }

    framebuffer = (uint32_t*)mbi->framebuffer_addr_low;
    width = mbi->framebuffer_width;
    height = mbi->framebuffer_height;
    pitch = mbi->framebuffer_pitch;

    // info
    serial_print("addr:   "); serial_print_hex((uint32_t)mbi->framebuffer_addr_low); serial_putchar('\n');
    serial_print("width:  "); serial_print_uint(mbi->framebuffer_width);             serial_putchar('\n');
    serial_print("height: "); serial_print_uint(mbi->framebuffer_height);            serial_putchar('\n');
    serial_print("pitch:  "); serial_print_uint(mbi->framebuffer_pitch);             serial_putchar('\n');
    serial_print("bpp:    "); serial_print_uint(mbi->framebuffer_bpp);               serial_putchar('\n');
    serial_print("flags:  "); serial_print_hex(mbi->flags);                          serial_putchar('\n');

    gfx_render_frame();
}