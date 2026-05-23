#pragma once
#include "lib/types.h"
#include "gfx/gfx.h"

#define WM_MAX_WINDOWS 16
#define WM_TITLEBAR_H  20

#define WM_RESIZE_BORDER 12
#define WM_RESIZE_HIT 16
#define WM_MIN_W 100
#define WM_MIN_H 60

#define WM_MAX_WIDGETS 32

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

typedef struct {
    rec bounds;
    char title[64];
    gfx_color_t bg;

    uint8_t visible;
    uint8_t focused;
    uint8_t dragging;
    uint8_t resizing;

    vec2 drag_off;
    rec resize_start;

    widget_t widgets[WM_MAX_WIDGETS];
    int widgets_count;
} window_t;

typedef struct {
    window_t windows[WM_MAX_WINDOWS];
    int count;
    int focused;

    uint8_t prev_left;
} wm_t;

extern wm_t wm;

void wm_init(void);
int wm_create(const char* title, rec bounds, gfx_color_t bg);
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

static void wm_draw_widget(window_t* w, widget_t* wg);
int wm_add_button(int wid, const char* text, rec bounds, void (*on_click)(void));
int wm_add_label(int wid, const char* text, rec bounds, gfx_color_t fg);
int wm_add_input(int wid, rec bounds);