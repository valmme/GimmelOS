#include "gfx/wm.h"
#include "lib/kstring.h"
#include "lib/math.h"

wm_t wm;

static wm_canvas_t current_canvas;

#define BAYER_MATRIX_SIZE 16

#define WM98_FACE           ((gfx_color_t){192, 192, 192, 255})
#define WM98_SHADOW         ((gfx_color_t){128, 128, 128, 255})
#define WM98_DARK_SHADOW    ((gfx_color_t){ 64,  64,  64, 255})
#define WM98_HILIGHT        ((gfx_color_t){255, 255, 255, 255})
#define WM98_TITLE_ACTIVE   ((gfx_color_t){  0,   0, 128, 255})
#define WM98_TITLE_INACTIVE ((gfx_color_t){128, 128, 128, 255})
#define WM98_TITLE_TEXT     ((gfx_color_t){255, 255, 255, 255})
#define WM98_BLACK          ((gfx_color_t){  0,   0,   0, 255})

static uint32_t wm98_isqrt(uint32_t n) {
    uint32_t res = 0;
    uint32_t bit = 1u << 30;

    while (bit > n) bit >>= 2;
    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res += bit << 1;
        }

        res >>= 1;
        bit >>= 2;
    }

    return res;
}

static uint32_t wm98_hash(int32_t x, int32_t y) {
    uint32_t h = (uint32_t)(x * 374761393 + y * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    return (h ^ (h >> 16)) & 0xFF;
}

void wm98_generate_bayer_matrix(uint32_t size, uint8_t* out_matrix) {
    if (size == 2) {
        out_matrix[0] = 0;
        out_matrix[1] = 2;
        out_matrix[2] = 3;
        out_matrix[3] = 1;
        return;
    }
    
    uint32_t half = size / 2;
    
    uint8_t temp[64];
    wm98_generate_bayer_matrix(half, temp); 
    
    for (uint32_t y = 0; y < half; y++) {
        for (uint32_t x = 0; x < half; x++) {
            uint8_t val = temp[y * half + x] * 4;
            
            out_matrix[y * size + x]                   = val + 0;
            out_matrix[y * size + (x + half)]          = val + 2;
            out_matrix[(y + half) * size + x]          = val + 3;
            out_matrix[(y + half) * size + (x + half)] = val + 1;
        }
    }
}

void wm98_draw_diagonal_dither_gradient(rec r, gfx_color_t color_tl, gfx_color_t color_br) {
    if (r.w == 0 || r.h == 0) return;

    uint32_t packed_tl = ((uint32_t)color_tl.r << 16) | ((uint32_t)color_tl.g << 8) | (uint32_t)color_tl.b;
    uint32_t packed_br = ((uint32_t)color_br.r << 16) | ((uint32_t)color_br.g << 8) | (uint32_t)color_br.b;

    static uint8_t bayer_matrix[BAYER_MATRIX_SIZE * BAYER_MATRIX_SIZE];
    static int is_initialized = 0;
    
    if (!is_initialized) {
        wm98_generate_bayer_matrix(BAYER_MATRIX_SIZE, bayer_matrix);
        is_initialized = 1;
    }

    uint32_t total_levels = BAYER_MATRIX_SIZE * BAYER_MATRIX_SIZE;

    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) {
        uint32_t* row = backbuffer + y * width;
        uint32_t progress_y = ((y - r.y) * total_levels) / r.h;

        for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) {
            uint32_t progress_x = ((x - r.x) * total_levels) / r.w; 
            uint32_t progress = (progress_x + progress_y) / 2;
            uint8_t threshold = bayer_matrix[(y % BAYER_MATRIX_SIZE) * BAYER_MATRIX_SIZE + (x % BAYER_MATRIX_SIZE)];
            
            row[x] = (progress > threshold) ? packed_br : packed_tl;
        }
    }
}

static void wm98_draw_bevel(rec r, int raised) {
    gfx_color_t outer_tl = raised ? WM98_HILIGHT     : WM98_DARK_SHADOW;
    gfx_color_t outer_br = raised ? WM98_DARK_SHADOW : WM98_HILIGHT;
    gfx_color_t inner_tl = raised ? WM98_FACE        : WM98_SHADOW;
    gfx_color_t inner_br = raised ? WM98_SHADOW      : WM98_FACE;

    int32_t x0 = r.x, y0 = r.y;
    int32_t x1 = r.x + (int32_t)r.w - 1;
    int32_t y1 = r.y + (int32_t)r.h - 1;

    gfx_draw_line((vec2){x0, y0}, (vec2){x1, y0}, outer_tl);
    gfx_draw_line((vec2){x0, y0}, (vec2){x0, y1}, outer_tl);
    gfx_draw_line((vec2){x1, y0}, (vec2){x1, y1}, outer_br);
    gfx_draw_line((vec2){x0, y1}, (vec2){x1, y1}, outer_br);

    gfx_draw_line((vec2){x0+1, y0+1}, (vec2){x1-1, y0+1}, inner_tl);
    gfx_draw_line((vec2){x0+1, y0+1}, (vec2){x0+1, y1-1}, inner_tl);
    gfx_draw_line((vec2){x1-1, y0+1}, (vec2){x1-1, y1-1}, inner_br);
    gfx_draw_line((vec2){x0+1, y1-1}, (vec2){x1-1, y1-1}, inner_br);
}

static void wm98_draw_bevel_w(rec r, int raised) {
    gfx_color_t outer_tl = raised ? WM98_HILIGHT     : WM98_DARK_SHADOW;
    gfx_color_t outer_br = raised ? WM98_DARK_SHADOW : WM98_HILIGHT;
    gfx_color_t inner_tl = raised ? WM98_FACE        : WM98_SHADOW;
    gfx_color_t inner_br = raised ? WM98_SHADOW      : WM98_FACE;

    int32_t x0 = r.x, y0 = r.y;
    int32_t x1 = r.x + (int32_t)r.w - 1;
    int32_t y1 = r.y + (int32_t)r.h - 1;

    wm_draw_line((vec2){x0, y0}, (vec2){x1, y0}, outer_tl);
    wm_draw_line((vec2){x0, y0}, (vec2){x0, y1}, outer_tl);
    wm_draw_line((vec2){x1, y0}, (vec2){x1, y1}, outer_br);
    wm_draw_line((vec2){x0, y1}, (vec2){x1, y1}, outer_br);

    wm_draw_line((vec2){x0+1, y0+1}, (vec2){x1-1, y0+1}, inner_tl);
    wm_draw_line((vec2){x0+1, y0+1}, (vec2){x0+1, y1-1}, inner_tl);
    wm_draw_line((vec2){x1-1, y0+1}, (vec2){x1-1, y1-1}, inner_br);
    wm_draw_line((vec2){x0+1, y1-1}, (vec2){x1-1, y1-1}, inner_br);
}

void wm98_draw_dither_gradient(rec r, gfx_color_t near_color, gfx_color_t far_color) {
    uint32_t packed_near = ((uint32_t)near_color.r << 16) | ((uint32_t)near_color.g << 8) | (uint32_t)near_color.b;
    uint32_t packed_far  = ((uint32_t)far_color.r << 16) | ((uint32_t)far_color.g << 8) | (uint32_t)far_color.b;

    static uint8_t bayer_matrix[BAYER_MATRIX_SIZE * BAYER_MATRIX_SIZE];
    static int is_initialized = 0;
    
    if (!is_initialized) {
        wm98_generate_bayer_matrix(BAYER_MATRIX_SIZE, bayer_matrix);
        is_initialized = 1;
    }

    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) {
        uint32_t* row = backbuffer + y * width;
        for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) {
            uint32_t progress = ((x - r.x) * BAYER_MATRIX_SIZE * BAYER_MATRIX_SIZE) / r.w; 
            uint8_t threshold = bayer_matrix[(y % BAYER_MATRIX_SIZE) * BAYER_MATRIX_SIZE + (x % BAYER_MATRIX_SIZE)];
            
            row[x] = (progress > threshold) ? packed_far : packed_near;
        }
    }
}

// unused for some reasons about optimization
void wm98_draw_dither(rec r, vec2 origin, gfx_color_t near_color, gfx_color_t far_color, int32_t max_dist) {
    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) {
        for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) {
            int32_t dx = x - origin.x;
            int32_t dy = y - origin.y;
            uint32_t dist = wm98_isqrt((uint32_t)(dx*dx + dy*dy));

            uint32_t threshold = (dist * 255) / (uint32_t)max_dist;
            if (threshold > 255) threshold = 255;

            uint32_t noise = wm98_hash(x, y);

            gfx_color_t color = (noise > threshold) ? near_color : far_color;
            gfx_put_pixel_clipped((vec2){x, y}, color);
        }
    }
}

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
        wm.windows[i].minimized     = 0;
        wm.windows[i].maximized     = 0;

        wm.windows[i].on_init       = 0;
        wm.windows[i].on_update     = 0;
        wm.windows[i].on_key        = 0;
        wm.windows[i].on_destroy    = 0;
        wm.windows[i].on_draw       = 0;
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
    w->minimized     = 0;
    w->maximized     = 0;

    return id;
}

int wm_create_app(const char* title, rec bounds, gfx_color_t bg, void (*on_init)(int), void (*on_update)(int), void (*on_draw)(int)) {
    int id = wm_create(title, bounds, bg);
    if (id < 0) return -1;

    wm.windows[id].on_init = on_init;
    wm.windows[id].on_update = on_update;
    wm.windows[id].on_draw = on_draw;

    if (on_init) on_init(id);

    return id;
} 

window_t* wm_get_by_id(int id) {
    return &wm.windows[id];
}

void wm_destroy(int id) {
    if (id < 0 || id >= wm.count) return;
    if (wm.windows[id].on_destroy) wm.windows[id].on_destroy(id);
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

        if (!w->visible)
            continue;

        if (pos.x >= w->bounds.x &&
            pos.x < w->bounds.x + (int32_t)w->bounds.w &&
            pos.y >= w->bounds.y &&
            pos.y < w->bounds.y + (int32_t)w->bounds.h)
            return i;
    }

    return -1;
}

static void wm_click_close(int id) {
    wm_destroy(id);
}

static void wm_click_maximize(int id) {
    window_t* w = &wm.windows[id];

    w->mouse_capture = 0;
    if (w->maximized) {
        w->bounds = w->saved_bounds;
        w->maximized = 0;
    }

    else if (w->minimized) {
        w->minimized = 0;
        w->saved_bounds = w->bounds;
        w->bounds = (rec){0, 0, width, height};
        w->maximized = 1;
    }
    
    else {
        w->saved_bounds = w->bounds;
        w->bounds = (rec){0, 0, width, height};
        w->maximized = 1;
    }
}

static rec wm_btn_rect(window_t* w, int btn) {
    int32_t x = w->bounds.x + (int32_t)w->bounds.w - WM_BTN_MARGIN - (btn + 1) * (WM_BTN_SIZE + WM_BTN_MARGIN);
    int32_t y = w->bounds.y + (WM_TITLEBAR_H - WM_BTN_SIZE) / 2;

    return (rec){x, y, WM_BTN_SIZE, WM_BTN_SIZE};
}

static void wm_click_minimize(int id) {
    window_t* w = &wm.windows[id];
    w->minimized = !w->minimized;
    w->mouse_capture = 0;
}

void wm_handle_mouse(mouse_state_t mouse) {
    if (wm_hit_test(mouse.pos) == -1) {
        if (mouse.left && !wm.prev_left) {
            wm.focused = -1;
            for (int i = 0; i < wm.count; i++) wm.windows[i].focused = 0;
        }
    }

    if (mouse.left && !wm.prev_left) {
        int id = wm_hit_test(mouse.pos);
        if (id < 0) {
            for (int i = 0; i < wm.count; i++) wm.windows[i].focused = 0;
            wm.focused = -1;
        } else {
            for (int i = 0; i < wm.count; i++) wm.windows[i].focused = 0;
            wm_bring_to_front(id);
            wm.focused = wm.count - 1;
            window_t* w = &wm.windows[wm.focused];
            w->focused = 1;

            if (!w->minimized && !w->maximized && wm_hit_resize(wm.focused, mouse.pos)) {
                w->resizing = 1;
                w->resize_start.x = mouse.pos.x;
                w->resize_start.y = mouse.pos.y;
                w->resize_start.w = w->bounds.w;
                w->resize_start.h = w->bounds.h;
            } else if (wm_hit_titlebar(wm.focused, mouse.pos)) {
                if (HIT(wm_btn_rect(w, 0), mouse.pos)) wm_click_close(wm.focused);
                else if (HIT(wm_btn_rect(w, 1), mouse.pos)) wm_click_maximize(wm.focused);
                else if (HIT(wm_btn_rect(w, 2), mouse.pos)) wm_click_minimize(wm.focused);
                else {
                    w->dragging = 1;
                    w->drag_off.x = mouse.pos.x - w->bounds.x;
                    w->drag_off.y = mouse.pos.y - w->bounds.y;
                }
            }
        }
    }

    if (mouse.right && !wm.prev_right) {
        int id = wm_hit_test(mouse.pos);
        if (id >= 0) {
            wm_bring_to_front(id);
            wm.focused = wm.count - 1;
            wm.windows[wm.focused].focused = 1;
        }
    }

    if (wm.focused >= 0) {
        wm_handle_widgets_mouse(wm.focused, mouse.pos, mouse.left);
    }

    if (!mouse.left) {
        for (int i = 0; i < wm.count; i++) {
            wm.windows[i].dragging = 0;
            wm.windows[i].resizing = 0;
        }
    } 
    
    else {
        for (int i = 0; i < wm.count; i++) {
            window_t* w = &wm.windows[i];
            if (w->dragging && !w->maximized) {
                w->bounds.x = mouse.pos.x - w->drag_off.x;
                w->bounds.y = mouse.pos.y - w->drag_off.y;
            }
            if (w->resizing) {
                int32_t new_w = (int32_t)w->resize_start.w + (mouse.pos.x - w->resize_start.x);
                int32_t new_h = (int32_t)w->resize_start.h + (mouse.pos.y - w->resize_start.y);
                if (new_w < WM_MIN_W) new_w = WM_MIN_W;
                if (new_h < WM_MIN_H) new_h = WM_MIN_H;
                w->bounds.w = (uint32_t)new_w;
                w->bounds.h = (uint32_t)new_h;
            }
        }
    }

    wm.prev_left = mouse.left;
    wm.prev_right = mouse.right;
}

// widgets
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
    rec r = wg->bounds;

    switch (wg->type) {
        case WIDGET_LABEL:
            wm_draw_text(wg->text, (vec2){r.x + 2, r.y + 2}, wg->fg);
            break;

        case WIDGET_BUTTON: {
            gfx_color_t face = wg->hovered ? WM98_HILIGHT : WM98_FACE;

            wm98_draw_bevel_w(r, 1);
            wm_draw_fill_rec((rec){r.x + 2, r.y + 2, r.w - 4, r.h - 4}, face);

            int tx = r.x + ((int32_t)r.w - (int32_t)kstrlen(wg->text) * FB_CHAR_W) / 2;
            int ty = r.y + ((int32_t)r.h - FB_CHAR_H) / 2;
            wm_draw_text(wg->text, (vec2){tx, ty}, WM98_BLACK);
            break;
        }

        case WIDGET_INPUT: {
            wm98_draw_bevel_w(r, 0);
            wm_draw_fill_rec((rec){r.x + 2, r.y + 2, r.w - 4, r.h - 4}, WM98_HILIGHT);
            wm_draw_text(wg->text, (vec2){r.x + 4, r.y + 4}, WM98_BLACK);

            if (wg->focused) {
                int cx = r.x + 4 + kstrlen(wg->text) * FB_CHAR_W;
                wm_draw_fill_rec((rec){cx, r.y + 4, 2, FB_CHAR_H}, WM98_BLACK);
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

    gfx_color_t title_bg = w->focused ? WM98_TITLE_ACTIVE : WM98_TITLE_INACTIVE;
    gfx_color_t title_fg = WM98_TITLE_TEXT;
    
    wm98_draw_bevel(w->bounds, 1);

    rec title_bar = {
        w->bounds.x + 2, w->bounds.y + 2,
        w->bounds.w - 4, WM_TITLEBAR_H - 2
    };

    gfx_color_t near_color = w->focused ? (gfx_color_t){ 16, 16, 200, 255 } : WM98_TITLE_INACTIVE;
    gfx_color_t far_color = w->focused ? (gfx_color_t){ 0, 0, 80, 255 } : (gfx_color_t){ 64, 64, 64, 255 };

    wm98_draw_dither_gradient(title_bar, near_color, far_color);
    gfx_print(w->title, (vec2){ title_bar.x + 4, title_bar.y + 5 }, title_fg);

    rec rc = wm_btn_rect(w, 0);
    wm98_draw_bevel(rc, 1);
    gfx_draw_fill_rec((rec){rc.x+1, rc.y+1, rc.w-2, rc.h-2}, WM98_FACE);
    gfx_draw_line((vec2){rc.x+3, rc.y+3}, (vec2){rc.x+rc.w-4, rc.y+rc.h-4}, WM98_BLACK);
    gfx_draw_line((vec2){rc.x+rc.w-4, rc.y+3}, (vec2){rc.x+3, rc.y+rc.h-4}, WM98_BLACK);

    rec rm = wm_btn_rect(w, 1);
    wm98_draw_bevel(rm, 1);
    gfx_draw_fill_rec((rec){rm.x+1, rm.y+1, rm.w-2, rm.h-2}, WM98_FACE);
    gfx_draw_rec((rec){rm.x+3, rm.y+3, rm.w-6, rm.h-6}, WM98_BLACK);

    rec rn = wm_btn_rect(w, 2);
    wm98_draw_bevel(rn, 1);
    gfx_draw_fill_rec((rec){rn.x+1, rn.y+1, rn.w-2, rn.h-2}, WM98_FACE);
    int32_t my = rn.y + rn.h - 5;
    gfx_draw_line((vec2){rn.x+3, my}, (vec2){rn.x+rn.w-4, my}, WM98_BLACK);

    if (w->minimized) return;

    rec body = {
        w->bounds.x + 2,
        w->bounds.y + WM_TITLEBAR_H,
        w->bounds.w - 4,
        w->bounds.h - WM_TITLEBAR_H - 2
    };
    gfx_draw_fill_rec(body, w->bg);

    if (w->on_draw) w->on_draw(id);

    if (!w->maximized) {
        rec handle = {
            w->bounds.x + (int32_t)w->bounds.w - 2 - WM_RESIZE_BORDER,
            w->bounds.y + (int32_t)w->bounds.h - 2 - WM_RESIZE_BORDER,
            WM_RESIZE_BORDER,
            WM_RESIZE_BORDER
        };
        gfx_draw_fill_rec(handle, WM98_SHADOW);
    }

    gfx_set_clip((rec){
        w->bounds.x + 1,
        w->bounds.y + WM_TITLEBAR_H,
        w->bounds.w - 2,
        w->bounds.h - WM_TITLEBAR_H - 1
    });

    for (int i = 0; i < w->widgets_count; i++)
        wm_draw_widget(w, &w->widgets[i]);

    gfx_reset_clip();
}

void wm_draw_all(void) {
    for (int i = 0; i < wm.count; i++)
        wm_draw(i);
}


// graphics
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

void wm_draw_rec(rec r, gfx_color_t color) {
    // top, bottom, left, right
    for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) { wm_draw_pixel((vec2){x, r.y}, color); }
    for (int32_t x = r.x; x < r.x + (int32_t)r.w; x++) { wm_draw_pixel((vec2){x, r.y + (int32_t)r.h - 1}, color); }
    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) { wm_draw_pixel((vec2){r.x, y}, color); }
    for (int32_t y = r.y; y < r.y + (int32_t)r.h; y++) { wm_draw_pixel((vec2){r.x + (int32_t)r.w - 1, y}, color); }
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

void wm_putchar_ex(char c, vec2 pos, gfx_color_t fg, int scale) {
    const uint8_t* glyph = font[(uint8_t)c];

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];

        for (int col = 0; col < 8; col++) {
            int is_fg = bits & (0x80 >> (7 - col));
            if (!is_fg) continue;

            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    gfx_put_pixel_clipped((vec2){
                        current_canvas.x + pos.x + col * scale + sx,
                        current_canvas.y + pos.y + row * scale + sy
                    }, fg);
                }
            }
        }
    }
}

void wm_putchar(char c, vec2 pos, gfx_color_t fg) {
    wm_putchar_ex(c, pos, fg, 1);
}

void wm_draw_text_ex(const char* str, vec2 pos, gfx_color_t fg, int scale) {
    int start_x = pos.x;
    while (*str) {
        if (*str == '\n') {
            pos.y += FB_CHAR_H * scale;
            pos.x  = start_x;
        } 
        
        else {
            wm_putchar_ex(*str, pos, fg, scale);
            pos.x += FB_CHAR_W * scale;
        }

        str++;
    }
}

void wm_draw_text(const char* str, vec2 pos, gfx_color_t fg) {
    wm_draw_text_ex(str, pos, fg, 1);
}

void wm_draw_int(int n, vec2 pos, gfx_color_t fg) {
    char tmp[12];
    int i = 0;

    if (n == 0) { tmp[i++] = '0'; }
    else {
        int rev = 0, cnt = 0, x = n;
        while (x > 0) { rev = rev * 10 + x % 10; x /= 10; cnt++; }
        for (int j = 0; j < cnt; j++) { tmp[i++] = '0' + (rev % 10); rev /= 10; }
    }

    tmp[i] = 0;
    wm_draw_text(tmp, pos, fg);
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