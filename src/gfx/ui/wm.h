#pragma once
#include "lib/types.h"
#include "gfx/gfx.h"

#define WM_MAX_WINDOWS 16
#define WM_TITLEBAR_H  20

#define WM_RESIZE_BORDER 12
#define WM_MIN_W 100
#define WM_MIN_H 60

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
} window_t;

typedef struct {
    window_t windows[WM_MAX_WINDOWS];
    int count;
    int focused;
} wm_t;

extern wm_t wm;

void wm_init(void);
int wm_create(const char* title, rec bounds, gfx_color_t bg);
void wm_destroy(int id);

void wm_draw(int id);
void wm_draw_all(void);

void wm_handle_mouse(vec2 pos, uint8_t left);
void wm_bring_to_front(int id);

int wm_hit_test(vec2 pos);
int wm_hit_titlebar(int id, vec2 pos);
int wm_hit_body(int id, vec2 pos);