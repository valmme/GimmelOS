#include "wm.h"
#include "lib/kstring.h"
#include "lib/math.h"

wm_t wm;

static wm_canvas_t current_canvas;

void wm_init(void) {
    wm.count     = 0;
    wm.focused   = -1;
    wm.prev_left = 0;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm.windows[i].visible       = 0;
        wm.windows[i].focused       = 0;
        wm.windows[i].dragging      = 0;
        wm.windows[i].resizing      = 0;
        wm.windows[i].widgets_count = 0;
    }
}

int wm_create(const char* title, rec bounds, gfx_color_t bg) {
    if (wm.count >= WM_MAX_WINDOWS) return -1;

    int id = wm.count++;
    window_t* w = &wm.windows[id];

    kstrncpy(w->title, title, 64);
    w->bounds        = bounds;
    w->bg            = bg;
    w->visible       = 1;
    w->focused       = 0;
    w->dragging      = 0;
    w->resizing      = 0;
    w->widgets_count = 0;

    return id;
}

void wm_destroy(int id) {
    if (id < 0 || id >= wm.count) return;
    wm.windows[id].visible = 0;
}

void wm_bring_to_front(int id) {
    if (id < 0 || id >= wm.count) return;

    window_t tmp = wm.windows[id];
    for (int i = id; i < wm.count - 1; i++)
        wm.windows[i] = wm.windows[i + 1];

    wm.windows[wm.count - 1] = tmp;
    wm.focused = wm.count - 1;
}

static int wm_hit_resize(int id, vec2 pos) {
    window_t* w = &wm.windows[id];
    int32_t rx = w->bounds.x + (int32_t)w->bounds.w - WM_RESIZE_HIT;
    int32_t ry = w->bounds.y + (int32_t)w->bounds.h - WM_RESIZE_HIT;

    return pos.x >= rx && pos.y >= ry &&
           pos.x < w->bounds.x + (int32_t)w->bounds.w &&
           pos.y < w->bounds.y + (int32_t)w->bounds.h;
}

int wm_hit_titlebar(int id, vec2 pos) {
    window_t* w = &wm.windows[id];
    return pos.x >= w->bounds.x &&
           pos.x < w->bounds.x + (int32_t)w->bounds.w &&
           pos.y >= w->bounds.y &&
           pos.y < w->bounds.y + WM_TITLEBAR_H;
}

int wm_hit_body(int id, vec2 pos) {
    window_t* w = &wm.windows[id];
    return pos.x >= w->bounds.x &&
           pos.x < w->bounds.x + (int32_t)w->bounds.w &&
           pos.y >= w->bounds.y + WM_TITLEBAR_H &&
           pos.y < w->bounds.y + (int32_t)w->bounds.h;
}

int wm_hit_test(vec2 pos) {
    for (int i = wm.count - 1; i >= 0; i--) {
        window_t* w = &wm.windows[i];
        if (!w->visible) continue;

        if (pos.x >= w->bounds.x &&
            pos.x < w->bounds.x + (int32_t)w->bounds.w &&
            pos.y >= w->bounds.y &&
            pos.y < w->bounds.y + (int32_t)w->bounds.h)
            return i;
    }

    return -1;
}

void wm_handle_mouse(vec2 pos, uint8_t left) {
    if (left && !wm.prev_left) {
        int id = wm_hit_test(pos);

        for (int i = 0; i < wm.count; i++)
            wm.windows[i].focused = 0;

        if (id >= 0) {
            wm_bring_to_front(id);
            wm.focused = wm.count - 1;

            window_t* w = &wm.windows[wm.focused];
            w->focused  = 1;

            if (wm_hit_resize(wm.focused, pos)) {
                w->resizing = 1;
                w->resize_start.x = pos.x;
                w->resize_start.y = pos.y;
                w->resize_start.w = w->bounds.w;
                w->resize_start.h = w->bounds.h;
            } 
            
            else if (wm_hit_titlebar(wm.focused, pos)) {
                w->dragging = 1;
                w->drag_off.x = pos.x - w->bounds.x;
                w->drag_off.y = pos.y - w->bounds.y;
            } 
            
            else if (wm_hit_body(wm.focused, pos)) {
                wm_handle_widgets_mouse(wm.focused, pos, left);
            }
        } 
        
        else {
            wm.focused = -1;
        }
    }

    if (!left) {
        for (int i = 0; i < wm.count; i++) {
            wm.windows[i].dragging = 0;
            wm.windows[i].resizing = 0;
        }
    }

    if (left) {
        for (int i = 0; i < wm.count; i++) {
            window_t* w = &wm.windows[i];

            if (w->dragging) {
                w->bounds.x = pos.x - w->drag_off.x;
                w->bounds.y = pos.y - w->drag_off.y;
            }

            if (w->resizing) {
                int32_t new_w = (int32_t)w->resize_start.w + (pos.x - w->resize_start.x);
                int32_t new_h = (int32_t)w->resize_start.h + (pos.y - w->resize_start.y);

                if (new_w < WM_MIN_W) new_w = WM_MIN_W;
                if (new_h < WM_MIN_H) new_h = WM_MIN_H;

                w->bounds.w = (uint32_t)new_w;
                w->bounds.h = (uint32_t)new_h;
            }
        }

        if (wm.focused >= 0)
            wm_handle_widgets_mouse(wm.focused, pos, left);
    }

    if (!left && wm.focused >= 0)
        wm_handle_widgets_mouse(wm.focused, pos, left);

    wm.prev_left = left;
}

void wm_handle_widgets_mouse(int wid, vec2 pos, uint8_t left) {
    window_t* w = &wm.windows[wid];

    for (int i = 0; i < w->widgets_count; i++) {
        widget_t* wg = &w->widgets[i];

        rec r = {
            w->bounds.x + wg->bounds.x,
            w->bounds.y + WM_TITLEBAR_H + wg->bounds.y,
            wg->bounds.w,
            wg->bounds.h
        };

        int hit = pos.x >= r.x && pos.x < r.x + (int32_t)r.w &&
                  pos.y >= r.y && pos.y < r.y + (int32_t)r.h;

        wg->hovered = hit;

        if (hit && left && !wm.prev_left) {
            if (wg->type == WIDGET_INPUT) {
                for (int j = 0; j < w->widgets_count; j++)
                    w->widgets[j].focused = 0;
                wg->focused = 1;
            }

            if (wg->type == WIDGET_BUTTON && wg->on_click)
                wg->on_click();
        }
    }
}

void wm_handle_widgets_key(int wid, char c) {
    window_t* w = &wm.windows[wid];

    for (int i = 0; i < w->widgets_count; i++) {
        widget_t* wg = &w->widgets[i];
        if (wg->type != WIDGET_INPUT || !wg->focused) continue;

        int len = kstrlen(wg->text);

        if (c == '\b') {
            if (len > 0) wg->text[len - 1] = '\0';
        } 
        
        else if (c >= 32 && c < 127 && len < 63) {
            wg->text[len]     = c;
            wg->text[len + 1] = '\0';
        }
    }
}

int wm_add_button(int wid, const char* text, rec bounds, void (*on_click)(void)) {
    window_t* w = &wm.windows[wid];
    if (w->widgets_count >= WM_MAX_WIDGETS) return -1;

    int id = w->widgets_count++;
    widget_t* wg = &w->widgets[id];

    wg->type     = WIDGET_BUTTON;
    wg->bounds   = bounds;
    wg->fg       = GFX_WHITE;
    wg->bg       = GFX_DARK_GRAY;
    wg->hovered  = 0;
    wg->focused  = 0;
    wg->on_click = on_click;
    kstrncpy(wg->text, text, 64);

    return id;
}

int wm_add_label(int wid, const char* text, rec bounds, gfx_color_t fg) {
    window_t* w = &wm.windows[wid];
    if (w->widgets_count >= WM_MAX_WIDGETS) return -1;

    int id = w->widgets_count++;
    widget_t* wg = &w->widgets[id];

    wg->type     = WIDGET_LABEL;
    wg->bounds   = bounds;
    wg->fg       = fg;
    wg->bg       = w->bg;
    wg->hovered  = 0;
    wg->focused  = 0;
    wg->on_click = 0;
    kstrncpy(wg->text, text, 64);

    return id;
}

int wm_add_input(int wid, rec bounds) {
    window_t* w = &wm.windows[wid];
    if (w->widgets_count >= WM_MAX_WIDGETS) return -1;

    int id       = w->widgets_count++;
    widget_t* wg = &w->widgets[id];

    wg->type     = WIDGET_INPUT;
    wg->bounds   = bounds;
    wg->fg       = GFX_WHITE;
    wg->bg       = GFX_BLACK;
    wg->hovered  = 0;
    wg->focused  = 0;
    wg->on_click = 0;
    wg->text[0]  = '\0';

    return id;
}

static void wm_draw_widget(window_t* w, widget_t* wg) {
    rec r = {
        w->bounds.x + wg->bounds.x,
        w->bounds.y + WM_TITLEBAR_H + wg->bounds.y,
        wg->bounds.w,
        wg->bounds.h
    };

    switch (wg->type) {
        case WIDGET_LABEL:
            gfx_print(wg->text, (vec2){r.x + 2, r.y + 2}, wg->fg, w->bg);
            break;

        case WIDGET_BUTTON: {
            gfx_color_t bg = wg->hovered ? GFX_GRAY : wg->bg;
            gfx_draw_fill_rec(r, bg);
            gfx_draw_rec(r, GFX_GRAY);

            int tx = r.x + ((int32_t)r.w - (int32_t)kstrlen(wg->text) * FB_CHAR_W) / 2;
            int ty = r.y + ((int32_t)r.h - FB_CHAR_H) / 2;
            gfx_print(wg->text, (vec2){tx, ty}, wg->fg, bg);
            break;
        }

        case WIDGET_INPUT: {
            gfx_color_t border = wg->focused ? GFX_WHITE : GFX_GRAY;
            gfx_draw_fill_rec(r, wg->bg);
            gfx_draw_rec(r, border);
            gfx_print(wg->text, (vec2){r.x + 4, r.y + 4}, wg->fg, wg->bg);

            if (wg->focused) {
                int cx = r.x + 4 + kstrlen(wg->text) * FB_CHAR_W;
                gfx_draw_fill_rec((rec){cx, r.y + 4, 2, FB_CHAR_H}, GFX_WHITE);
            }
            break;
        }

        default: break;
    }
}

void wm_draw(int id) {
    if (id < 0 || id >= wm.count) return;

    window_t* w = &wm.windows[id];
    if (!w->visible) return;

    gfx_color_t border   = w->focused ? GFX_WHITE     : GFX_GRAY;
    gfx_color_t title_fg = w->focused ? GFX_WHITE     : GFX_LIGHT_GRAY;
    gfx_color_t title_bg = w->focused ? GFX_DARK_BLUE : GFX_DARK_GRAY;

    gfx_draw_fill_rec(w->bounds, w->bg);

    rec title_bar = { w->bounds.x, w->bounds.y, w->bounds.w, WM_TITLEBAR_H };
    gfx_draw_fill_rec(title_bar, title_bg);
    gfx_print(w->title, (vec2){ w->bounds.x + 4, w->bounds.y + 6 }, title_fg, title_bg);
    gfx_draw_rec(w->bounds, border);

    rec handle = {
        w->bounds.x + (int32_t)w->bounds.w - WM_RESIZE_BORDER,
        w->bounds.y + (int32_t)w->bounds.h - WM_RESIZE_BORDER,
        WM_RESIZE_BORDER,
        WM_RESIZE_BORDER
    };
    gfx_draw_fill_rec(handle, border);

    for (int i = 0; i < w->widgets_count; i++)
        wm_draw_widget(w, &w->widgets[i]);
}

void wm_draw_all(void) {
    for (int i = 0; i < wm.count; i++)
        wm_draw(i);
}

wm_canvas_t wm_get_canvas(int wid) {
    window_t* w = &wm.windows[wid];
    return (wm_canvas_t){
        .x = w->bounds.x + 1,
        .y = w->bounds.y + WM_TITLEBAR_H,
        .w = w->bounds.w - 2,
        .h = w->bounds.h - WM_TITLEBAR_H - 1
    };
}

void wm_begin_draw(int wid) {
    current_canvas = wm_get_canvas(wid);
    gfx_set_clip((rec){
        current_canvas.x,
        current_canvas.y,
        current_canvas.w,
        current_canvas.h
    });
}

void wm_end_draw(void) {
    gfx_reset_clip();
}

void wm_draw_pixel(vec2 pos, gfx_color_t color) {
    gfx_put_pixel_clipped(addv(get_pos(current_canvas), pos), color);
}

void wm_draw_fill_rec(rec r, gfx_color_t color) {
    for (int32_t row = r.y; row < r.y + (int32_t)r.h; row++)
        for (int32_t col = r.x; col < r.x + (int32_t)r.w; col++)
            wm_draw_pixel((vec2){col, row}, color);
}

void wm_draw_line(vec2 a, vec2 b, gfx_color_t color) {
    int dx = (b.x > a.x) ? b.x - a.x : a.x - b.x;
    int sx = (a.x < b.x) ? 1 : -1;
    int dy = (b.y > a.y) ? b.y - a.y : a.y - b.y;
    int sy = (a.y < b.y) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        wm_draw_pixel(a, color);

        if (a.x == b.x && a.y == b.y) break;
        int e2 = 2 * err;

        if (e2 > -dy) { err -= dy; a.x += sx; }
        if (e2 <  dx) { err += dx; a.y += sy; }
    }
}

void wm_draw_text(const char* str, vec2 pos, gfx_color_t fg, gfx_color_t bg) {
    vec2 abs_pos = {current_canvas.x + pos.x, current_canvas.y + pos.y};

    while (*str) {
        if (*str == '\n') {
            abs_pos.y += FB_CHAR_H;
            abs_pos.x  = current_canvas.x + pos.x;
        } 
        
        else {
            const uint8_t* glyph = font[(uint8_t)*str];
            for (int row = 0; row < 8; row++) {
                uint8_t bits = reverse_bits(glyph[row]);
                for (int col = 0; col < 8; col++) {
                    gfx_color_t c = (bits & (0x80 >> col)) ? fg : bg;
                    gfx_put_pixel_clipped((vec2){abs_pos.x + col, abs_pos.y + row}, c);
                }
            }

            abs_pos.x += FB_CHAR_W;
        }

        str++;
    }
}

void wm_draw_circle(vec2 pos, int32_t radius, gfx_color_t color) {
    int32_t x = 0, y = radius, d = 1 - radius;

    while (x <= y) {
        wm_draw_pixel((vec2){pos.x+x, pos.y+y}, color);
        wm_draw_pixel((vec2){pos.x-x, pos.y+y}, color);
        wm_draw_pixel((vec2){pos.x+x, pos.y-y}, color);
        wm_draw_pixel((vec2){pos.x-x, pos.y-y}, color);

        wm_draw_pixel((vec2){pos.x+y, pos.y+x}, color);
        wm_draw_pixel((vec2){pos.x-y, pos.y+x}, color);
        wm_draw_pixel((vec2){pos.x+y, pos.y-x}, color);
        wm_draw_pixel((vec2){pos.x-y, pos.y-x}, color);

        if (d < 0) d += 2 * x + 3;
        else {
            d += 2 * (x - y) + 5;
            y--;
        }

        x++;
    }
}

void wm_draw_fill_circle(vec2 pos, int32_t radius, gfx_color_t color) {
    int32_t x = 0, y = radius, d = 1 - radius;

    while (x <= y) {
        for (int32_t i = pos.x - x; i <= pos.x + x; i++) {
            wm_draw_pixel((vec2){i, pos.y+y}, color);
            wm_draw_pixel((vec2){i, pos.y-y}, color);
        }

        for (int32_t i = pos.x - y; i <= pos.x + y; i++) {
            wm_draw_pixel((vec2){i, pos.y+x}, color);
            wm_draw_pixel((vec2){i, pos.y-x}, color);
        }

        if (d < 0) d += 2 * x + 3;
        else {
            d += 2 * (x - y) + 5;
            y--;
        }

        x++;
    }
}

void wm_draw_texture(const uint32_t* tex, vec2 pos, vec2 size) {
    for (uint32_t row = 0; row < size.y; row++) {
        for (uint32_t col = 0; col < size.x; col++) {
            uint32_t packed = tex[row * size.x + col];

            uint8_t a = (packed >> 24) & 0xFF;
            if (a == 0) continue;

            gfx_color_t color = {
                .r = (packed >> 16) & 0xFF,
                .g = (packed >> 8)  & 0xFF,
                .b =  packed        & 0xFF,
                .a = a
            };

            wm_draw_pixel((vec2){pos.x + col, pos.y + row}, color);
        }
    }
}