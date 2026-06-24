#include "lib/types.h"
#include "lib/kstring.h"
#include "gfx/gfx.h"
#include "apps/game/textures.h"
#include "apps/game/game.h"

#define map_w 64
#define map_h 64
#define move_speed 1
#define mouse_sens 0.003f
#define brightness 0.9f

static int map[map_h][map_w];

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

static player_t player = {3.5f, 3.5f, 0};
static uint8_t key_w, key_s, key_a, key_d, key_esc;
uint8_t lines = 2;

static float pitch_w = 0.0f;
static float jump_vel = 0;
static uint8_t is_jumping = 0;
static uint8_t gravitation = 9;

float camera_height = 5000.0f;

void game_init(int wid) {
    player.x = 3.5f; player.y = 3.5f; player.a = 0;
    
    for(int i = 0; i < map_h; i++) {
        for(int j = 0; j < map_w; j++) {
            map[i][j] = get_random() % 3; 
        }
    }
}
static int is_wall(int x, int y) {
    return map[y & (map_h - 1)][x & (map_w - 1)];
}

static ray_hit_t dda_ray(float px, float py, float dx, float dy) {
    int map_x = (int)px, map_y = (int)py;

    float delta_dist_x = (dx == 0) ? 1e30f : fabsf(1.0f/dx);
    float delta_dist_y = (dy == 0) ? 1e30f : fabsf(1.0f/dy);

    int step_x = (dx < 0) ? -1 : 1;
    int step_y = (dy < 0) ? -1 : 1;

    float side_dist_x = (dx < 0) ? (px - map_x) * delta_dist_x : (map_x + 1.0f - px) * delta_dist_x;
    float side_dist_y = (dy < 0) ? (py - map_y) * delta_dist_y : (map_y + 1.0f - py) * delta_dist_y;

    int side = 0;
    for (int i = 0; i < 64; i++) {
        if (side_dist_x < side_dist_y) { 
            side_dist_x += delta_dist_x; 
            map_x += step_x; 
            side = 0; 
        } 
        
        else { 
            side_dist_y += delta_dist_y; 
            map_y += step_y; 
            side = 1; 
        }

        if (map[map_y & (map_h - 1)][map_x & (map_w - 1)]) break;
    }

    float dist, hit_x;
    if (side == 0) { 
        dist = side_dist_x - delta_dist_x; 
        hit_x = py + dist * dy; 
    } 
    
    else { 
        dist = side_dist_y - delta_dist_y; 
        hit_x = px + dist * dx; 
    }

    hit_x -= (int)hit_x;
    return (ray_hit_t){dist, hit_x, side};
}

void game_update(int wid) {
    extern mouse_state_t mouse;
    extern wm_t wm;
    extern uint8_t key_pressed[256];

    float pitch_w_limit = 1.0f;

    key_w = key_s = key_a = key_d = key_esc = 0;

    if (keyboard_is_key_down(SC_W)) key_w = 1;
    if (keyboard_is_key_down(SC_S)) key_s = 1;
    if (keyboard_is_key_down(SC_A)) key_a = 1;
    if (keyboard_is_key_down(SC_D)) key_d = 1;

    if (keyboard_is_key_down(SC_SPACE)) camera_height += 200;
    if (keyboard_is_key_down(SC_LSHIFT)) camera_height -= 200;

    if (keyboard_is_key_down(SC_LEFT))  player.a -= 0.01f;
    if (keyboard_is_key_down(SC_RIGHT)) player.a += 0.01f;
    if (keyboard_is_key_down(SC_UP))    pitch_w += 0.01f;
    if (keyboard_is_key_down(SC_DOWN))  pitch_w -= 0.01f;

    if (keyboard_is_key_pressed(SC_ESC)) key_esc = 1;

    window_t* win = &wm.windows[wm.focused];
    win->wants_mouse_capture = 1;

    if (key_esc) {
        win->mouse_capture = 0;
    }

    if (wm.focused == wid && win->mouse_capture) {
        int cx = win->bounds.x + (int32_t)win->bounds.w / 2;
        int cy = win->bounds.y + WM_TITLEBAR_H + (int32_t)(win->bounds.h - WM_TITLEBAR_H) / 2;
        int dx = mouse.pos.x - cx;
        int dy = mouse.pos.y - cy;

        player.a += dx * mouse_sens;
        pitch_w -= dy * mouse_sens;
        
        if (pitch_w > pitch_w_limit) pitch_w = pitch_w_limit;
        if (pitch_w < -pitch_w_limit) pitch_w = -pitch_w_limit;

        mouse.pos.x = cx;
        mouse.pos.y = cy;
    }

    float dir_x = cosf(player.a), dir_y = sinf(player.a);
    float mx = 0, my = 0;

    if (key_w) { mx += dir_x * move_speed; my += dir_y * move_speed; }
    if (key_s) { mx -= dir_x * move_speed; my -= dir_y * move_speed; }
    if (key_a) { mx += dir_y * move_speed; my -= dir_x * move_speed; }
    if (key_d) { mx -= dir_y * move_speed; my += dir_x * move_speed; }

    if (is_jumping) {
        camera_height += jump_vel;
        jump_vel -= gravitation;

        if (camera_height <= 5000.0f) {
            camera_height = 5000.0f;
            jump_vel = 0;
            is_jumping = 0;
        }
    }

    player.x += mx;
    player.y += my;
}

void draw_floor(float dir_x, float dir_y, float plane_x, float plane_y, int h, int w) {
    float pz = camera_height;
    float horizon = h / 2.0f + pitch_w * h;

    for (int y = (int)horizon + 1; y < h; y++) {
        float row_dist = pz / (y - horizon);

        float floor_step_x = row_dist * (dir_x - plane_x) + player.x;
        float floor_step_y = row_dist * (dir_y - plane_y) + player.y;

        float step_x = row_dist * (2.0f * plane_x) / w;
        float step_y = row_dist * (2.0f * plane_y) / w;

        int cur_x = (int)(floor_step_x * 65536.0f);
        int cur_y = (int)(floor_step_y * 65536.0f);
        int s_x = (int)(step_x * 65536.0f);
        int s_y = (int)(step_y * 65536.0f);

        for (int x = 0; x < w; x++) {
            int tx = (cur_x >> 16) & (G_W - 1);
            int ty = (cur_y >> 16) & (G_H - 1);

            int grid_x = (cur_x >> 20);
            int grid_y = (cur_y >> 20);

            cur_x += s_x;
            cur_y += s_y;

            int block_type = map[grid_y & (map_h - 1)][grid_x & (map_w - 1)];

            uint32_t fc;
            if (block_type == 0) fc = grass[ty * G_W + tx];
            else if (block_type == 1) fc = stone[ty * G_W + tx];
            else fc = dirt[ty * G_W + tx];

            uint8_t alpha = (fc >> 24) & 0xFF;
            if (alpha == 0) continue;

            gfx_color_t col = {(fc >> 16) & 0xFF, (fc >> 8) & 0xFF, fc & 0xFF, alpha};
            wm_draw_pixel((vec2){x, y}, col);
        }
    }
}

void game_draw(int wid) {
    wm_begin_draw(wid);

    vec2 size = get_size(wm_get_canvas(wid));
    int w = size.x, h = size.y;

    float dir_x = cosf(player.a), dir_y = sinf(player.a);
    float plane_x = -dir_y * 0.66f, plane_y = dir_x * 0.66f;

    draw_floor(dir_x, dir_y, plane_x, plane_y, h, w);

    int text_scale = h / 180;
    if (text_scale < 1) text_scale = 1;
    if (text_scale > 6) text_scale = 6;

    wm_draw_text("FPS: ", (vec2){5, 5}, GFX_WHITE);
    wm_draw_int(get_fps(), (vec2){40, 5}, GFX_WHITE);

    wm_end_draw();
}