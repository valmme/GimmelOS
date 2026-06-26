#include "lib/types.h"
#include "lib/kstring.h"
#include "gfx/gfx.h"
#include "apps/game/textures.h"
#include "apps/game/game.h"

#define map_w 64
#define map_h 64
#define MOVE_SPEED 5.0f
#define MOUSE_SENS 0.003f
#define ROT_SPEED 3.0f
#define FLOOR_STEP 2
#define HEIGHT_SPEED 500.0f

static int RENDER_SCALE = 1;

static int map[map_h][map_w];

typedef struct {
    float x;
    float y;
    float a;
} player_t;

static player_t player = {3.5f, 3.5f, 0};

static uint8_t key_w, key_s, key_a, key_d, key_esc;
uint8_t lines = 2;

static float pitch_w = 0.0f;
static float jump_vel = 0;
static uint8_t is_jumping = 0;
static uint8_t gravitation = 9;

float camera_height = 0.0f;

static inline gfx_color_t mix_color(gfx_color_t a, gfx_color_t b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    gfx_color_t out;
    out.r = (uint8_t)(a.r * (1.0f - t) + b.r * t);
    out.g = (uint8_t)(a.g * (1.0f - t) + b.g * t);
    out.b = (uint8_t)(a.b * (1.0f - t) + b.b * t);
    out.a = a.a;
    return out;
}

static inline gfx_color_t texel_to_color(uint32_t px) {
    gfx_color_t c;
    c.a = (px >> 24) & 0xFF;
    c.r = (px >> 16) & 0xFF;
    c.g = (px >> 8) & 0xFF;
    c.b = px & 0xFF;
    return c;
}

static inline void draw_block(int x, int y, int size, gfx_color_t c, int w, int h) {
    for (int yy = 0; yy < size; yy++) {
        int py = y + yy;
        if (py < 0 || py >= h) continue;

        for (int xx = 0; xx < size; xx++) {
            int px = x + xx;
            if (px < 0 || px >= w) continue;
            wm_draw_pixel((vec2){px, py}, c);
        }
    }
}

void game_init(int wid) {
    player.x = 3.5f;
    player.y = 3.5f;
    player.a = 0.0f;
    camera_height = 0.0f;

    for (int y = 0; y < map_h; y++) {
        for (int x = 0; x < map_w; x++) {
            map[y][x] = get_random() % 3;
        }
    }
}

void game_update(int wid) {
    extern mouse_state_t mouse;
    extern wm_t wm;

    float pitch_w_limit = 1.0f;
    float dt = 1.0f / (float)get_fps();
    if (dt > 0.05f) dt = 0.05f;

    key_w = keyboard_is_key_down(SC_W);
    key_s = keyboard_is_key_down(SC_S);
    key_a = keyboard_is_key_down(SC_A);
    key_d = keyboard_is_key_down(SC_D);
    key_esc = keyboard_is_key_pressed(SC_ESC);

    if (keyboard_is_key_down(SC_LEFT))  player.a -= ROT_SPEED * dt;
    if (keyboard_is_key_down(SC_RIGHT)) player.a += ROT_SPEED * dt;
    if (keyboard_is_key_down(SC_UP))    pitch_w  += ROT_SPEED * dt;
    if (keyboard_is_key_down(SC_DOWN))  pitch_w  -= ROT_SPEED * dt;

    if (keyboard_is_key_down(SC_SPACE))  camera_height += HEIGHT_SPEED * dt;
    if (keyboard_is_key_down(SC_LSHIFT)) camera_height -= HEIGHT_SPEED * dt;

    if (keyboard_is_key_pressed(SC_KP_PLUS)) { RENDER_SCALE++; }
    if (keyboard_is_key_pressed(SC_KP_MINUS) && RENDER_SCALE - 1 > 0) { RENDER_SCALE--; }

    if (camera_height < -200.0f) camera_height = -200.0f;
    if (camera_height > 5000.0f) camera_height = 5000.0f;

    window_t* win = &wm.windows[wm.focused];
    win->wants_mouse_capture = 1;

    if (key_esc) win->mouse_capture = 0;

    if (wm.focused == wid && win->mouse_capture) {
        int cx = win->bounds.x + (int32_t)win->bounds.w / 2;
        int cy = win->bounds.y + WM_TITLEBAR_H + (int32_t)(win->bounds.h - WM_TITLEBAR_H) / 2;
        int dx = mouse.pos.x - cx;
        int dy = mouse.pos.y - cy;

        player.a += dx * MOUSE_SENS;
        pitch_w -= dy * MOUSE_SENS;

        if (pitch_w > pitch_w_limit) pitch_w = pitch_w_limit;
        if (pitch_w < -pitch_w_limit) pitch_w = -pitch_w_limit;

        mouse.pos.x = cx;
        mouse.pos.y = cy;
    }

    float dir_x = cosf(player.a);
    float dir_y = sinf(player.a);

    float move_x = 0.0f;
    float move_y = 0.0f;

    if (key_w) { move_x += dir_x * MOVE_SPEED * dt; move_y += dir_y * MOVE_SPEED * dt; }
    if (key_s) { move_x -= dir_x * MOVE_SPEED * dt; move_y -= dir_y * MOVE_SPEED * dt; }
    if (key_a) { move_x += dir_y * MOVE_SPEED * dt; move_y -= dir_x * MOVE_SPEED * dt; }
    if (key_d) { move_x -= dir_y * MOVE_SPEED * dt; move_y += dir_x * MOVE_SPEED * dt; }

    if (is_jumping) {
        camera_height += jump_vel;
        jump_vel -= gravitation;

        if (camera_height <= 0.0f) {
            camera_height = 0.0f;
            jump_vel = 0;
            is_jumping = 0;
        }
    }

    player.x += move_x;
    player.y += move_y;
}

static void draw_sky(int h, int w) {
    int horizon = (int)(h * 0.5f + pitch_w * h);
    if (horizon < 0) horizon = 0;
    if (horizon > h) horizon = h;

    gfx_color_t top = {90, 140, 220, 255};
    gfx_color_t bot = {170, 210, 255, 255};

    for (int y = 0; y < horizon; y += RENDER_SCALE) {
        float t = (float)y / (float)(horizon ? horizon : 1);
        gfx_color_t c = mix_color(top, bot, t);
        draw_block(0, y, RENDER_SCALE, c, w, h);
        for (int x = RENDER_SCALE; x < w; x += RENDER_SCALE) {
            draw_block(x, y, RENDER_SCALE, c, w, h);
        }
    }
}

static void draw_floor(float dir_x, float dir_y, float plane_x, float plane_y, int h, int w) {
    float horizon = h * 0.5f + pitch_w * h;
    float pz = camera_height;
    gfx_color_t fog_col = {140, 180, 255, 255};

    int start_y = (int)horizon + 1;
    if (start_y < 0) start_y = 0;
    if (start_y >= h) return;

    int step = FLOOR_STEP * RENDER_SCALE;
    if (step < 1) step = 1;

    for (int y = start_y; y < h; y += step) {
        float dy = (float)y - horizon;
        if (dy <= 0.001f) continue;

        float row_dist = pz / dy;

        float floor_x = player.x + row_dist * (dir_x - plane_x);
        float floor_y = player.y + row_dist * (dir_y - plane_y);

        float step_x = row_dist * (2.0f * plane_x) / w * step;
        float step_y = row_dist * (2.0f * plane_y) / w * step;

        float fog = row_dist / 40.0f;
        if (fog > 1.0f) fog = 1.0f;
        if (fog < 0.0f) fog = 0.0f;

        for (int x = 0; x < w; x += step) {
            int cell_x = (int)floor_x;
            int cell_y = (int)floor_y;

            float frac_x = floor_x - cell_x;
            float frac_y = floor_y - cell_y;
            if (frac_x < 0) frac_x += 1.0f;
            if (frac_y < 0) frac_y += 1.0f;

            int tx = (int)(frac_x * G_W) & (G_W - 1);
            int ty = (int)(frac_y * G_H) & (G_H - 1);

            int block_type = map[cell_y & (map_h - 1)][cell_x & (map_w - 1)];

            uint32_t fc;
            if (block_type == 1) fc = stone[ty * G_W + tx];
            else if (block_type == 2) fc = dirt[ty * G_W + tx];
            else fc = grass[ty * G_W + tx];

            gfx_color_t col = texel_to_color(fc);
            if (col.a) {
                col = mix_color(col, fog_col, fog);
                draw_block(x, y, step, col, w, h);
            }

            floor_x += step_x;
            floor_y += step_y;
        }
    }
}

void game_draw(int wid) {
    wm_begin_draw(wid);

    vec2 size = get_size(wm_get_canvas(wid));
    int w = size.x;
    int h = size.y;

    float dir_x = cosf(player.a);
    float dir_y = sinf(player.a);
    float plane_x = -dir_y * 0.66f;
    float plane_y = dir_x * 0.66f;

    draw_sky(h, w);
    draw_floor(dir_x, dir_y, plane_x, plane_y, h, w);

    wm_draw_text("fps:", (vec2){5, 5}, GFX_WHITE);
    wm_draw_int(get_fps(), (vec2){40, 5}, GFX_WHITE);

    wm_draw_text("render:", (vec2){5, 20}, GFX_WHITE);
    wm_draw_int(RENDER_SCALE, (vec2){65, 20}, GFX_WHITE);

    wm_end_draw();
}