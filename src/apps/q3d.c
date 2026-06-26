#include "apps/q3d.h"
#include "lib/math.h"
#include "kernel/time.h"
#include "gfx/wm.h"
#include "drivers/keyboard.h"

typedef struct {
    int indices[3];
} face_t;

typedef struct {
    int face_idx;
    float z_depth;
    float light;
} sort_face_t;

static vec2f rot = {0, 0};
static vec3f verts[8];
static face_t faces[12];
static int last_cube_size = -1;
static uint8_t auto_rot = 0;

static gfx_color_t apply_lighting(gfx_color_t color, float intensity) {
    if (intensity < 0.15f) intensity = 0.15f;
    if (intensity > 1.0f) intensity = 1.0f;
    return (gfx_color_t){
        (uint8_t)(color.r * intensity),
        (uint8_t)(color.g * intensity),
        (uint8_t)(color.b * intensity),
        255
    };
}

static void draw_fill_triangle(vec2 v1, vec2 v2, vec2 v3, gfx_color_t color) {
    if (v1.y > v2.y) { vec2 t = v1; v1 = v2; v2 = t; }
    if (v1.y > v3.y) { vec2 t = v1; v1 = v3; v3 = t; }
    if (v2.y > v3.y) { vec2 t = v2; v2 = v3; v3 = t; }

    int32_t total_height = v3.y - v1.y;
    if (total_height <= 0) return;

    for (int32_t i = 0; i <= total_height; i++) {
        int32_t y = v1.y + i;
        float alpha = (float)i / (float)total_height;
        vec2 start = { v1.x + (int32_t)((v3.x - v1.x) * alpha), y };
        vec2 end;

        if (i < (v2.y - v1.y)) {
            if (v2.y == v1.y) continue;
            end = (vec2){
                v1.x + (int32_t)((v2.x - v1.x) * ((float)i / (float)(v2.y - v1.y))),
                y
            };
        } else {
            if (v3.y == v2.y) continue;
            float beta = (float)(i - (v2.y - v1.y)) / (float)(v3.y - v2.y);
            end = (vec2){
                v2.x + (int32_t)((v3.x - v2.x) * beta),
                y
            };
        }

        if (start.x > end.x) {
            vec2 t = start;
            start = end;
            end = t;
        }

        wm_draw_line(start, end, color);
    }
}

static vec3f vec3_sub(vec3f a, vec3f b) {
    return (vec3f){a.x - b.x, a.y - b.y, a.z - b.z};
}

static vec3f vec3_cross(vec3f a, vec3f b) {
    return (vec3f){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static float vec3_dot(vec3f a, vec3f b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static vec3f vec3_normalize(vec3f v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len <= 0.0001f) return (vec3f){0, 0, 0};
    return (vec3f){v.x / len, v.y / len, v.z / len};
}

static void generate(float size) {
    for (uint8_t i = 0; i < 8; i++) {
        verts[i].x = (i & 1) ? size : -size;
        verts[i].y = (i & 2) ? size : -size;
        verts[i].z = (i & 4) ? size : -size;
    }

    int f_list[12][3] = {
        {0,1,3}, {0,3,2},
        {4,5,7}, {4,7,6},
        {0,1,5}, {0,5,4},
        {2,3,7}, {2,7,6},
        {0,2,6}, {0,6,4},
        {1,3,7}, {1,7,5}
    };

    for (int i = 0; i < 12; i++) {
        faces[i].indices[0] = f_list[i][0];
        faces[i].indices[1] = f_list[i][1];
        faces[i].indices[2] = f_list[i][2];
    }
}

void q3d_init(int wid) {
    last_cube_size = -1;
    rot.x = 0.0f;
    rot.y = 0.0f;
}

void q3d_update(int wid) {
    float dt = 1.0f / (float)get_fps();
    if (dt > 0.05f) dt = 0.05f;

    const float speed = 15.0f;

    if (keyboard_is_key_pressed(SC_SPACE)) {
        auto_rot = !auto_rot;
    }

    if (keyboard_is_key_down(SC_LEFT))  rot.y -= speed * dt;
    if (keyboard_is_key_down(SC_RIGHT)) rot.y += speed * dt;
    if (keyboard_is_key_down(SC_UP))    rot.x -= speed * dt;
    if (keyboard_is_key_down(SC_DOWN))  rot.x += speed * dt;

    if (auto_rot) {
        rot = addf(rot, speed * dt);
    }
}

void q3d_draw(int wid) {
    wm_canvas_t canvas = wm_get_canvas(wid);
    float center_x = (float)canvas.w * 0.5f;
    float center_y = (float)canvas.h * 0.5f;

    int cube_size = (canvas.w < canvas.h ? canvas.w : canvas.h) / 8;
    if (cube_size < 8) cube_size = 8;

    if (cube_size != last_cube_size) {
        generate((float)cube_size);
        last_cube_size = cube_size;
    }

    vec3f transformed[8];
    vec2f projected[8];
    sort_face_t sorted_faces[6];

    float cx = cosf(rot.x), sx = sinf(rot.x);
    float cy = cosf(rot.y), sy = sinf(rot.y);

    for (int i = 0; i < 8; i++) {
        vec3f v = verts[i];

        float tx = v.x * cy + v.z * sy;
        float tz = -v.x * sy + v.z * cy;
        v.x = tx;
        v.z = tz;

        float ty = v.y * cx - v.z * sx;
        tz = v.y * sx + v.z * cx;
        v.y = ty;
        v.z = tz;

        transformed[i] = v;

        float z_val = v.z + cube_size * 5.0f;
        if (z_val < 1.0f) z_val = 1.0f;

        float factor = (float)(cube_size * 10) / z_val;
        projected[i].x = v.x * factor + center_x;
        projected[i].y = v.y * factor + center_y;
    }

    vec3f light_dir = vec3_normalize((vec3f){-0.5f, -0.7f, -1.0f});

    for (int i = 0; i < 6; i++) {
        int tri = i * 2;
        int i0 = faces[tri].indices[0];
        int i1 = faces[tri].indices[1];
        int i2 = faces[tri].indices[2];

        vec3f v0 = transformed[i0];
        vec3f v1 = transformed[i1];
        vec3f v2 = transformed[i2];

        vec3f e1 = vec3_sub(v1, v0);
        vec3f e2 = vec3_sub(v2, v0);
        vec3f normal = vec3_normalize(vec3_cross(e1, e2));

        float diffuse = -vec3_dot(normal, light_dir);
        if (diffuse < 0.0f) diffuse = 0.0f;

        int a = faces[tri].indices[0];
        int b = faces[tri].indices[1];
        int c = faces[tri].indices[2];
        int d = faces[tri + 1].indices[2];

        sorted_faces[i].face_idx = tri;
        sorted_faces[i].z_depth =
            (transformed[a].z + transformed[b].z + transformed[c].z + transformed[d].z) * 0.25f;
        sorted_faces[i].light = 0.2f + diffuse * 0.8f;
    }

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5 - i; j++) {
            if (sorted_faces[j].z_depth < sorted_faces[j + 1].z_depth) {
                sort_face_t temp = sorted_faces[j];
                sorted_faces[j] = sorted_faces[j + 1];
                sorted_faces[j + 1] = temp;
            }
        }
    }

    wm_begin_draw(wid);

    for (int i = 0; i < 6; i++) {
        int base = sorted_faces[i].face_idx;
        gfx_color_t color = apply_lighting(GFX_WHITE, sorted_faces[i].light);

        for (int t = 0; t < 2; t++) {
            int idx = base + t;

            vec2 p0 = {
                (int32_t)projected[faces[idx].indices[0]].x,
                (int32_t)projected[faces[idx].indices[0]].y
            };
            vec2 p1 = {
                (int32_t)projected[faces[idx].indices[1]].x,
                (int32_t)projected[faces[idx].indices[1]].y
            };
            vec2 p2 = {
                (int32_t)projected[faces[idx].indices[2]].x,
                (int32_t)projected[faces[idx].indices[2]].y
            };

            draw_fill_triangle(p0, p1, p2, color);
        }
    }

    wm_end_draw();
}