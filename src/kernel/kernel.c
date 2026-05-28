#include "cpu/io.h"
#include "gfx/gfx.h"
#include "drivers/serial.h"

#include "drivers/vga/vga.h"
#include "drivers/keyboard/keyboard.h"
#include "filesystem/fs.h"

#include "apps/wolfenstein/game.h"
#include "apps/editor/editor.h"

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


void gfx_render_frame() {
    mouse_init();
    wm_init();

    // wm_create_app("wolfenstein", (rec){50, 50, 400, 300}, (gfx_color_t){30,30,30,255}, game_init, game_update);
    int win =  wm_create_app("Lito Editor", (rec){50, 50, 400, 300}, (gfx_color_t){30,30,30,255}, editor_init, editor_update, editor_draw);
    wm.windows[win].mouse_capture = 0;


    uint8_t prev_left = 0;

    while (1) {
        mouse_poll();
        wm_handle_mouse(mouse.pos, mouse.left);

        if (mouse.left && !prev_left) {
            int id = wm_hit_test(mouse.pos);

            if (id >= 0) {
                window_t* w = &wm.windows[id];

                if (wm_hit_body(id, mouse.pos)) {
                    int32_t rx = w->bounds.x + (int32_t)w->bounds.w - WM_RESIZE_HIT;
                    int32_t ry = w->bounds.y + (int32_t)w->bounds.h - WM_RESIZE_HIT;

                    int in_resize = mouse.pos.x >= rx &&
                                    mouse.pos.y >= ry;

                    if (!in_resize && w->wants_mouse_capture)
                        w->mouse_capture = 1;
                }
            }
        }

        if (!mouse.left) {
            for (int i = 0; i < wm.count; i++)
                wm.windows[i].mouse_capture = 0;
        }

        prev_left = mouse.left;

        gfx_begin_frame(GFX_DARK_BLUE);

        for (int i = 0; i < wm.count; i++) {
            window_t* w = &wm.windows[i];

            if (!w->visible || w->minimized)
                continue;

            if (w->on_update)
                w->on_update(i);
        }

        wm_draw_all();

        int captured = 0;

        for (int i = 0; i < wm.count; i++) {
            if (wm.windows[i].mouse_capture) {
                captured = 1;
                break;
            }
        }

        if (!captured)
            gfx_draw_cursor(mouse);

        gfx_end_frame();
    }
}