#include "cpu/io.h"
#include "gfx/gfx.h"
#include "drivers/serial.h"

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
    // vga_init()
    serial_init();
    keyboard_init();

    if (magic != 0x2BADB002) {
        // vga_warn("Not multiboot");
    }

    struct multiboot_info* mbi = (struct multiboot_info*)addr;

    if (!(mbi->flags & (1 << 12))) {
        // vga_error("No framebuffer!");
        return;
    }

    framebuffer = (uint32_t*)mbi->framebuffer_addr_low;
    width = mbi->framebuffer_width;
    height = mbi->framebuffer_height;
    pitch = mbi->framebuffer_pitch;

    gfx_init((vec2){width, height}, pitch, framebuffer);

    // info
    serial_print("addr:   "); serial_print_hex((uint32_t)mbi->framebuffer_addr_low); serial_putchar('\n');
    serial_print("width:  "); serial_print_uint(mbi->framebuffer_width);             serial_putchar('\n');
    serial_print("height: "); serial_print_uint(mbi->framebuffer_height);            serial_putchar('\n');
    serial_print("pitch:  "); serial_print_uint(mbi->framebuffer_pitch);             serial_putchar('\n');
    serial_print("bpp:    "); serial_print_uint(mbi->framebuffer_bpp);               serial_putchar('\n');
    serial_print("flags:  "); serial_print_hex(mbi->flags);                          serial_putchar('\n');

    gfx_render_frame();
}

void on_click(void) {
    // handle button click
}

void gfx_render_frame() {
    mouse_init();
    wm_init();

    int win = wm_create("My Window", (rec){100, 100, 300, 200}, GFX_DARK_GRAY);
    wm_add_label (win, "Enter name:", (rec){10, 10,  150, 16},      GFX_WHITE);
    wm_add_input (win,               (rec){10, 30,  200, 24});
    wm_add_button(win, "OK",         (rec){10, 64,  80,  24}, on_click);
    wm_add_button(win, "Cancel",     (rec){100, 64, 80,  24}, 0);

    while (1) {
        mouse_poll();
        char key = keyboard_getchar_nonblocking();

        wm_handle_mouse(mouse.pos, mouse.left);
        if (wm.focused >= 0 && key)
            wm_handle_widgets_key(wm.focused, key);

        gfx_begin_frame(GFX_DARK_BLUE);
        wm_draw_all();
        gfx_draw_cursor(mouse);
        gfx_end_frame();
    }
}