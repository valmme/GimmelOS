#include "wm.h"
#include "lib/kstring.h"

wm_t wm;

void wm_init(void) {
    wm.count = 0;
    wm.focused = -1;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm.windows[i].visible = 0;
        wm.windows[i].focused = 0;
        wm.windows[i].dragging = 0;
    }
}

int wm_create(const char* title, rec bounds, gfx_color_t bg) {
    if (wm.count >= WM_MAX_WINDOWS) return -1;

    int id = wm.count++;
    window_t* w = &wm.windows[id];

    kstrncpy(w->title, title, 64);

    w->bounds = bounds;
    w->bg = bg;
    w->visible = 1;
    w->focused = 0;
    w->dragging = 0;

    return id;
}

void wm_destroy(int id) {
    if (id < 0 || id >= wm.count) return;
    wm.windows[id].visible = 0;
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

    gfx_print(w->title, (vec2){ w->bounds.x+4, w->bounds.y+6 }, title_fg, title_bg);
    gfx_draw_rec(w->bounds, border);

    rec handle = {
        w->bounds.x + (int32_t)w->bounds.w - WM_RESIZE_BORDER,
        w->bounds.y + (int32_t)w->bounds.h - WM_RESIZE_BORDER,
        WM_RESIZE_BORDER,
        WM_RESIZE_BORDER
    };

    gfx_draw_fill_rec(handle, border);
}

void wm_draw_all(void) {
    for (int i = 0; i < wm.count; i++) {
        wm_draw(i);
    }
}

int wm_hit_test(vec2 pos) {
    for (int i = wm.count - 1; i >= 0; i--) {
        window_t* w = &wm.windows[i];
        if (!w->visible) continue;

        if (pos.x >= w->bounds.x && pos.x < w->bounds.x + (int32_t)w->bounds.w && pos.y >= w->bounds.y && pos.y < w->bounds.y + (int32_t)w->bounds.h)
            return i;
    }

    return -1;
}

int wm_hit_titlebar(int id, vec2 pos) {
    window_t* w = &wm.windows[id];
    return (
        pos.x >= w->bounds.x &&
        pos.x <  w->bounds.x + (int32_t)w->bounds.w &&
        pos.y >= w->bounds.y &&
        pos.y <  w->bounds.y + WM_TITLEBAR_H
    );
}

int wm_hit_body(int id, vec2 pos) {
    window_t* w = &wm.windows[id];
    return (
        pos.x >= w->bounds.x &&
        pos.x <  w->bounds.x + (int32_t)w->bounds.w &&
        pos.y >= w->bounds.y + WM_TITLEBAR_H &&
        pos.y <  w->bounds.y + (int32_t)w->bounds.h
    );
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
    int32_t rx = w->bounds.x + (int32_t)w->bounds.w - WM_RESIZE_BORDER;
    int32_t ry = w->bounds.y + (int32_t)w->bounds.h - WM_RESIZE_BORDER;

    return (
        pos.x >= rx && pos.y >= ry &&
        pos.x < w->bounds.x + (int32_t)w->bounds.w &&
        pos.y < w->bounds.y + (int32_t)w->bounds.h
    );
}

void wm_handle_mouse(vec2 pos, uint8_t left) {
    static uint8_t prev_left = 0;

    if (left && !prev_left) {
        int id = wm_hit_test(pos);

        for (int i = 0; i < wm.count; i++)
            wm.windows[i].focused = 0;

        if (id >= 0) {
            wm_bring_to_front(id);
            wm.focused = wm.count - 1;

            window_t* w = &wm.windows[wm.focused];
            w->focused = 1;

            if (wm_hit_resize(wm.focused, pos)) {
                w->resizing       = 1;
                w->resize_start.x = pos.x;
                w->resize_start.y = pos.y;
                w->resize_start.w = w->bounds.w;
                w->resize_start.h = w->bounds.h; 
            } 
            
            else if (wm_hit_titlebar(wm.focused, pos)) {
                w->dragging   = 1;
                w->drag_off.x = pos.x - w->bounds.x;
                w->drag_off.y = pos.y - w->bounds.y;
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
    }

    prev_left = left;
}