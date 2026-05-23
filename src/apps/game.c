#include "lib/types.h"
#include "gfx/ui/wm.h"
#include "gfx/gfx.h"
#include "game.h"
#include "drivers/keyboard/keyboard.h"

#define MAP_W 8
#define MAP_H 8

#define MOVE_SPEED 0.09f
#define ROT_SPEED 0.09f

static int map[MAP_H][MAP_W] = {
    {1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,1},
    {1,0,1,0,1,0,0,1},
    {1,0,1,0,1,0,0,1},
    {1,0,0,0,0,0,0,1},
    {1,0,1,1,1,0,0,1},
    {1,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1},
};

typedef struct {
    float x;
    float y;
    float a;
} player_t;

static player_t p = {3.5f, 3.5f, 0};

static int key_w = 0;
static int key_s = 0;
static int key_a = 0;
static int key_d = 0;

void keyboard_update_game() {
    char c;

    key_w = 0;
    key_s = 0;
    key_a = 0;
    key_d = 0;

    while ((c = keyboard_getchar_nonblocking())) {
        if (c == 'w' || c == 'W') key_w = 1;
        if (c == 's' || c == 'S') key_s = 1;
        if (c == 'a' || c == 'A') key_a = 1;
        if (c == 'd' || c == 'D') key_d = 1;
    }
}

static int is_wall(int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
        return 1;
    return map[y][x];
}

static float dda_ray(float px, float py, float dx, float dy) {
    int mapX = (int)px;
    int mapY = (int)py;

    float deltaDistX = (dx == 0) ? 1e30f : (dx < 0 ? -1.0f / dx : 1.0f / dx);
    float deltaDistY = (dy == 0) ? 1e30f : (dy < 0 ? -1.0f / dy : 1.0f / dy);

    int stepX;
    int stepY;

    float sideDistX;
    float sideDistY;

    if (dx < 0) {
        stepX = -1;
        sideDistX = (px - mapX) * deltaDistX;
    } else {
        stepX = 1;
        sideDistX = (mapX + 1.0f - px) * deltaDistX;
    }

    if (dy < 0) {
        stepY = -1;
        sideDistY = (py - mapY) * deltaDistY;
    } else {
        stepY = 1;
        sideDistY = (mapY + 1.0f - py) * deltaDistY;
    }

    int hit = 0;
    int side = 0;

    for (int i = 0; i < 64; i++) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
        }

        if (is_wall(mapX, mapY)) {
            hit = 1;
            break;
        }
    }

    float dist;
    if (side == 0)
        dist = sideDistX - deltaDistX;
    else
        dist = sideDistY - deltaDistY;

    return dist;
}

void game_update(int wid) {
    wm_begin_draw(wid);

    vec2 size = get_size(wm_get_canvas(wid));
    int w = size.x;
    int h = size.y;

    float fov = PI / 3.5f;

    float ca = cosf(p.a);
    float sa = sinf(p.a);

    if (key_a) p.a -= ROT_SPEED;
    if (key_d) p.a += ROT_SPEED;

    float dx = 0;
    float dy = 0;

    if (key_w) {
        dx += ca * MOVE_SPEED;
        dy += sa * MOVE_SPEED;
    }

    if (key_s) {
        dx -= ca * MOVE_SPEED;
        dy -= sa * MOVE_SPEED;
    }

    if (!is_wall((int)(p.x + dx), (int)p.y))
        p.x += dx;

    if (!is_wall((int)p.x, (int)(p.y + dy)))
        p.y += dy;

    float inv_w = 1.0f / w;

    for (int x = 0; x < w; x++) {
        float t = x * inv_w;
        float ray_a = p.a - fov * 0.5f + t * fov;

        float rx = cosf(ray_a);
        float ry = sinf(ray_a);

        float dist = dda_ray(p.x, p.y, rx, ry);
        float corrected = dist * cosf(ray_a - p.a);
        int line_h = (int)(h / (corrected + 0.0001f));

        int start = (h - line_h) / 2;
        int end = start + line_h;

        if (start < 0) start = 0;
        if (end >= h) end = h - 1;

        int shade = 255 - (int)(corrected * 40);
        if (shade < 0) shade = 0;
        if (shade > 255) shade = 255;

        gfx_color_t c = (gfx_color_t){shade, shade, shade, 255};

        wm_draw_line((vec2){x, start}, (vec2){x, end}, c);
    }

    wm_end_draw();
}