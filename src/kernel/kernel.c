#include "kernel/kernel.h"
#include "kernel/io.h"
#include "kernel/time.h"
#include "gfx/gfx.h"
#include "drivers/serial.h"

#include "drivers/keyboard.h"
#include "fs.h"

#include "apps/game/game.h"
#include "apps/shell.h"
#include "apps/debug.h"

struct multiboot_info* multiboot = 0;
uint8_t debug_created = 0;

void shell_run(void);

void panic(const char* msg) {
    serial_print("--- KERNEL PANIC ---");
    serial_print(msg);
    halt();
}

void kernel_main(uint32_t magic, uint32_t addr) {
    // vga_init()
    serial_init();
    keyboard_init();
    fs_init();

    random_init();

    if (magic != 0x2BADB002) {
        // vga_warn("Not multiboot");
    }

    multiboot = (struct multiboot_info*)addr;

    if (!(multiboot->flags & (1 << 12)))
        return;

    framebuffer = (uint32_t*)multiboot->framebuffer_addr_low;
    width = multiboot->framebuffer_width;
    height = multiboot->framebuffer_height;
    pitch = multiboot->framebuffer_pitch;

    gfx_init((vec2){width, height}, pitch, framebuffer);
    gfx_render_frame();
}

void debug_destroy(int wid) {
    debug_created = 0;
}

void gfx_render_frame() {
    mouse_init();
    wm_init();

    wm_create_app("Shell", (rec){50, 50, 200, 200}, GFX_BLANK, shell_init, shell_update, shell_draw);
    wm.windows[0].focused = 1;

    uint8_t debug_id = 0;
    uint8_t prev_left = 0;

    calibrate_timer();

    while (1) {
        update_fps_counter();

        io_poll();
        wm_handle_mouse(mouse);

        if (mouse.left && !prev_left) {
            int id = wm_hit_test(mouse.pos);

            if (id >= 0) {
                window_t* w = &wm.windows[id];

                if (wm_hit_body(id, mouse.pos)) {
                    int32_t rx = w->bounds.x + (int32_t)w->bounds.w - WM_RESIZE_HIT;
                    int32_t ry = w->bounds.y + (int32_t)w->bounds.h - WM_RESIZE_HIT;

                    int in_resize = mouse.pos.x >= rx && mouse.pos.y >= ry;

                    if (!in_resize && w->wants_mouse_capture)
                        w->mouse_capture = 1;
                }
            }
        }

        if (!debug_created && keyboard_is_key_down(SC_CTRL) && keyboard_is_key_pressed(SC_E)) {
            debug_created = 1;
            debug_id = wm_create_app("Debug", (rec){100, 100, 200, 200}, GFX_BLACK, debug_init, debug_update, debug_draw);
            wm.windows[debug_id].on_destroy = debug_destroy;
        }

        prev_left = mouse.left;

        gfx_begin_frame(GFX_DARK_BLUE);
        
        // unused for some reason
        // gfx_paint_desktop();

        for (int i = 0; i < wm.count; i++) {
            window_t* w = &wm.windows[i];

            if (!w->visible || w->minimized)
                continue;

            if (w->on_update) {
                keyboard_set_enabled(i == wm.focused);
                w->on_update(i);
            }
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