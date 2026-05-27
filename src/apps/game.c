#include "lib/types.h"
#include "lib/kstring.h"
#include "gfx/ui/wm.h"
#include "gfx/gfx.h"

#include "gfx/textures/game.h"

#include "drivers/keyboard/keyboard.h"
#include "kernel/cpu/io.h"

#include "game.h"

#define MAP_W 8
#define MAP_H 8

#define MOVE_SPEED 0.09f
#define MOUSE_SENS 0.003f

#define BRIGHTNESS 0.9f

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

typedef struct {
    float dist;
    float hit_x;
    int side;
} ray_hit_t;

static player_t p = {3.5f, 3.5f, 0};

static int key_w;
static int key_s;
static int key_a;
static int key_d;
static int key_esc;

int lines = 4;

static int last_mouse_x = 0;

void game_init() {
    // used for generating textures
}

void keyboard_update_game() {
    char c;

    key_w = 0;
    key_s = 0;
    key_a = 0;
    key_d = 0;
    key_esc = 0;

    while ((c = keyboard_getchar_nonblocking())) {
        if (c == 'w' || c == 'W') key_w = 1;
        if (c == 's' || c == 'S') key_s = 1;
        if (c == 'a' || c == 'A') key_a = 1;
        if (c == 'd' || c == 'D') key_d = 1;
        if (c == KEY_ESCAPE) key_esc = 1;
    }
}

static int is_wall(int x, int y) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H)
        return 1;

    return map[y][x];
}

static ray_hit_t dda_ray(float px, float py, float dx, float dy) {
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
    }
    
    else {
        stepX = 1;
        sideDistX = (mapX + 1.0f - px) * deltaDistX;
    }

    if (dy < 0) {
        stepY = -1;
        sideDistY = (py - mapY) * deltaDistY;
    }
    
    else {
        stepY = 1;
        sideDistY = (mapY + 1.0f - py) * deltaDistY;
    }

    int side = 0;

    for (int i = 0; i < 64; i++) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = 0;
        } 
        
        else {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = 1;
        }

        if (map[mapY][mapX])
            break;
    }

    float dist;
    float hitX;

    if (side == 0) {
        dist = sideDistX - deltaDistX;
        hitX = py + dist * dy;
    } 
    
    else {
        dist = sideDistY - deltaDistY;
        hitX = px + dist * dx;
    }

    hitX -= (int)hitX;

    return (ray_hit_t){dist, hitX, side};
}

void game_update(int wid) {
    wm_begin_draw(wid);

    mouse_poll();

    extern mouse_state_t mouse;
    extern wm_t wm; 

    window_t* win = &wm.windows[wm.focused];

    if (key_esc) {
        win->mouse_capture = 0;
    }

    if (wm.focused == wid && win->mouse_capture) {
        int cx = win->bounds.x + (int32_t)win->bounds.w / 2;
        int cy = win->bounds.y + WM_TITLEBAR_H + (int32_t)(win->bounds.h - WM_TITLEBAR_H) / 2;

        int dx = mouse.pos.x - cx;
        p.a += dx * MOUSE_SENS;

        mouse.pos.x = cx;
        mouse.pos.y = cy;
    }


    last_mouse_x = mouse.pos.x;

    if (mouse.delta.x || mouse.delta.y)
        p.a += mouse.delta.x * MOUSE_SENS;

    vec2 size = get_size(wm_get_canvas(wid));

    int w = size.x;
    int h = size.y;

    float dirX = cosf(p.a);
    float dirY = sinf(p.a);

    float planeX = -dirY * 0.66f;
    float planeY =  dirX * 0.66f;

    float dx = 0;
    float dy = 0;

    if (key_w) {
        dx += dirX * MOVE_SPEED;
        dy += dirY * MOVE_SPEED;
    }

    if (key_s) {
        dx -= dirX * MOVE_SPEED;
        dy -= dirY * MOVE_SPEED;
    }

    if (key_a) {
        dx += dirY * MOVE_SPEED;
        dy -= dirX * MOVE_SPEED;
    }

    if (key_d) {
        dx -= dirY * MOVE_SPEED;
        dy += dirX * MOVE_SPEED;
    }

    if (!is_wall((int)(p.x + dx), (int)p.y))
        p.x += dx;

    if (!is_wall((int)p.x, (int)(p.y + dy)))
        p.y += dy;

    for (int y = h / 2; y < h; y += lines) {
        float pz = h * 0.5f;
        float rowDist = pz / (y - h / 2);

        float rayDirX0 = dirX - planeX;
        float rayDirY0 = dirY - planeY;

        float rayDirX1 = dirX + planeX;
        float rayDirY1 = dirY + planeY;

        float stepX = rowDist * (rayDirX1 - rayDirX0) / w;
        float stepY = rowDist * (rayDirY1 - rayDirY0) / w;

        float floorX = p.x + rowDist * rayDirX0;
        float floorY = p.y + rowDist * rayDirY0;

        for (int x = 0; x < w; x += 2) {
            int cellX = (int)floorX;
            int cellY = (int)floorY;

            int tx = (int)(TEX_W * (floorX - cellX)) & (TEX_W - 1);
            int ty = (int)(TEX_H * (floorY - cellY)) & (TEX_H - 1);

            floorX += stepX * 2;
            floorY += stepY * 2;

            uint32_t fc = floor_tex[ty * TEX_W + tx];
            uint32_t cc = floor_tex[ty * TEX_W + tx];

            uint8_t fr = (fc >> 16) & 0xFF;
            uint8_t fg = (fc >> 8) & 0xFF;
            uint8_t fb = fc & 0xFF;

            uint8_t cr = (cc >> 16) & 0xFF;
            uint8_t cg = (cc >> 8) & 0xFF;
            uint8_t cb = cc & 0xFF;

            gfx_color_t floor_col = {fr, fg, fb, 255};
            gfx_color_t ceil_col  = {cr, cg, cb, 255};

            wm_draw_pixel((vec2){x, y}, floor_col);
            wm_draw_pixel((vec2){x, h - y - 1}, ceil_col);
        }
    }

    float inv_w = 1.0f / w;
    float fov = PI / 3.0f;

    for (int x = 0; x < w; x += lines) {
        float t = x * inv_w;

        float ray_a = p.a - fov * 0.5f + t * fov;

        float rx = cosf(ray_a);
        float ry = sinf(ray_a);

        ray_hit_t hit = dda_ray(p.x, p.y, rx, ry);

        float corrected = hit.dist * cosf(ray_a - p.a);

        int line_h = (int)(h / (corrected + 0.0001f));

        int start = h / 2 - line_h / 2;
        int end = start + line_h;

        if (start < 0) start = 0;
        if (end >= h) end = h - 1;

        int tex_x = (int)(hit.hit_x * TEX_W);

        if (tex_x < 0) tex_x = 0;
        if (tex_x >= TEX_W) tex_x = TEX_W - 1;

        int tex_step = (TEX_H << 16) / line_h;
        int tex_pos = 0;

        float light = 1.0f / (1.0f + hit.dist * hit.dist * 0.12f);

        if (hit.side)
            light *= BRIGHTNESS;

        if (light < 0.1f)
            light = 0.1f;

        for (int y = start; y < end; y++) {
            int tex_y = tex_pos >> 16;
            tex_pos += tex_step;

            uint32_t color = wall_tex[tex_y * TEX_W + tex_x];

            uint8_t r = ((color >> 16) & 0xFF) * light;
            uint8_t g = ((color >> 8) & 0xFF) * light;
            uint8_t b = (color & 0xFF) * light;

            gfx_color_t c = {r, g, b, 255};

            for (int lx = 0; lx < lines; lx++) {
                wm_draw_pixel((vec2){x + lx, y}, c);
            }
        }
    }

    wm_draw_text_ex("Use WASD to move, mouse to look around", (vec2){10, 10}, GFX_BLACK, GFX_WHITE, 2);

/*
    char buf[32];
    buf[0] = '\0';

    char key = keyboard_getchar_nonblocking();

    kstrcat(buf, "PRESSED_KEY: ");
    kstrcatc(buf, key ? key : ' ');

    wm_draw_text_ex(buf, (vec2){0, win->bounds.h - 8}, GFX_BLACK, GFX_WHITE, 1);
*/

    wm_end_draw();
}