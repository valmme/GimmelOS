#include "apps/debug.h"
#include "gfx/wm.h"
#include "drivers/keyboard.h"

void debug_init(int wid) {}
void debug_update(int wid) {}
void debug_draw(int wid) {
    extern mouse_state_t mouse;
    extern wm_t wm;

    wm_begin_draw(wid);

    wm_draw_text("CX: ", (vec2){5,  5}, GFX_WHITE); wm_draw_int(mouse.pos.x, (vec2){30,  5}, GFX_WHITE);
    wm_draw_text("CY: ", (vec2){5, 15}, GFX_WHITE); wm_draw_int(mouse.pos.y, (vec2){30, 15}, GFX_WHITE);

    wm_draw_text("L: ", (vec2){5, 30}, GFX_WHITE); wm_draw_int(mouse.left,   (vec2){15, 30}, GFX_WHITE);
    wm_draw_text("M: ", (vec2){5, 40}, GFX_WHITE); wm_draw_int(mouse.middle, (vec2){15, 40}, GFX_WHITE);
    wm_draw_text("R: ", (vec2){5, 50}, GFX_WHITE); wm_draw_int(mouse.right,  (vec2){15, 50}, GFX_WHITE);

    wm_draw_text("kbd_bk: ", (vec2){5, 65}, GFX_WHITE); wm_draw_int(keyboard_getchar_nonblocking(), (vec2){70, 65}, GFX_WHITE); 

    wm_draw_text("FPS: ", (vec2){5, 80}, GFX_WHITE);
    wm_draw_int(get_fps(), (vec2){45, 80}, GFX_WHITE);

    wm_end_draw();
}