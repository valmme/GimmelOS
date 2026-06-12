#ifndef GOS_WM_H
#define GOS_WM_H

#include "lib/types.h"
#include "gfx/gfx.h"

#define WM_MAX_WINDOWS 16
#define WM_TITLEBAR_H  20

#define WM_RESIZE_BORDER 12
#define WM_RESIZE_HIT 16
#define WM_MIN_W 100
#define WM_MIN_H 60

#define WM_BTN_SIZE 14
#define WM_BTN_MARGIN 4

#define WM_MAX_WIDGETS 32

#define HIT(r, p) ((p).x >= (r).x && (p).x < (r).x + (int32_t)(r).w && (p).y >= (r).y && (p).y < (r).y + (int32_t)(r).h)

typedef enum {
    WIDGET_NONE,
    WIDGET_BUTTON,
    WIDGET_INPUT,
    WIDGET_LABEL
} widget_type_t;

typedef struct {
    widget_type_t type;
    rec bounds;
    char text[64];

    gfx_color_t fg;
    gfx_color_t bg;

    uint8_t hovered;
    uint8_t focused;

    void (*on_click)(void);
} widget_t;

#define wm_canvas_t rec

typedef struct {
    rec bounds;
    rec saved_bounds;

    char title[64];
    gfx_color_t bg;

    uint8_t maximized;
    uint8_t minimized;
    uint8_t visible;
    uint8_t focused;
    uint8_t dragging;
    uint8_t resizing;
    uint8_t mouse_capture;
    uint8_t wants_mouse_capture;

    void* user_data;

    vec2 drag_off;
    rec resize_start;

    widget_t widgets[WM_MAX_WIDGETS];
    int widgets_count;

    void (*on_init)(int wid); // runs when the window is created
    void (*on_update)(int wid); // runs every frame
    void (*on_draw)(int wid); // runs every frame, after on_update, used for custom drawing
    void (*on_key)(int wid, char c); // runs when a key is pressed while the window is focused
    void (*on_destroy)(int wid); // runs when the window is closed
} window_t;

typedef struct {
    window_t windows[WM_MAX_WINDOWS];
    int count;
    int focused;

    uint8_t prev_left;
} wm_t;

extern wm_t wm;

// windows
void wm_init(void);
int wm_create(const char* title, rec bounds, gfx_color_t bg);
int wm_create_app(const char* title, rec bounds, gfx_color_t bg, void (*on_init)(int), void (*on_update)(int), void (*on_draw)(int));
void wm_destroy(int id);

void wm_draw(int id);
void wm_draw_all(void);

void wm_handle_mouse(vec2 pos, uint8_t left);
void wm_handle_widgets_mouse(int wid, vec2 pos, uint8_t left);
void wm_handle_widgets_key(int wid, char c);
void wm_bring_to_front(int id);

int wm_hit_test(vec2 pos);
int wm_hit_titlebar(int id, vec2 pos);
int wm_hit_body(int id, vec2 pos);

// widgets
static void wm_draw_widget(window_t* w, widget_t* wg);
int wm_add_button(int wid, const char* text, rec bounds, void (*on_click)(void));
int wm_add_label(int wid, const char* text, rec bounds, gfx_color_t fg);
int wm_add_input(int wid, rec bounds);

// canvas
wm_canvas_t wm_get_canvas(int wid);
void wm_begin_draw(int wid);
void wm_end_draw(void);

// draw in canvas
void wm_draw_pixel(vec2 pos, gfx_color_t color);
void wm_draw_fill_rec(rec r, gfx_color_t color);
void wm_draw_line(vec2 a, vec2 b, gfx_color_t color);
void wm_putchar_ex(char c, vec2 pos, gfx_color_t fg, gfx_color_t bg, int scale);
void wm_putchar(char c, vec2 pos, gfx_color_t fg, gfx_color_t bg);
void wm_draw_text_ex(const char* str, vec2 pos, gfx_color_t fg, gfx_color_t bg, int scale);
void wm_draw_text(const char* str, vec2 pos, gfx_color_t fg, gfx_color_t bg);
void wm_draw_circle(vec2 pos, int32_t radius, gfx_color_t color);
void wm_draw_fill_circle(vec2 pos, int32_t radius, gfx_color_t color);
void wm_draw_texture(const uint32_t* tex, vec2 pos, vec2 size);

#endif // GOS_WM_H