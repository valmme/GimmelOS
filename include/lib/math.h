#ifndef GOS_MATH_H
#define GOS_MATH_H

#include "types.h"

#define PI 3.14159265358979323846f
#define TWO_PI (2.0f * PI)

// basic math
static inline float wrap_angle(float x) {
    while (x > PI) x -= TWO_PI;
    while (x < -PI) x += TWO_PI;
    return x;
}

static inline float sinf(float x) {
    x = wrap_angle(x);
    float x2 = x*x;

    return (
        x - (x2 * x) / 6.0f
        + (x2 * x2 * x) / 120.0f
        - (x2 * x2 * x2 * x) / 5040.0f
        + (x2 * x2 * x2 * x2 * x) / 362880.0f
    );
}

static inline float cosf(float x) {
    x = wrap_angle(x);
    float x2 = x * x;

    return (
        1.0f
        - x2 / 2.0f
        + (x2 * x2) / 24.0f
        - (x2 * x2 * x2) / 720.0f
        + (x2 * x2 * x2 * x2) / 40320.0f
    );
}

static inline float fabsf(float x) {
    return x < 0 ? -x : x;
}

static inline float sqrtf(float x) {
    if (x <= 0) return 0;
    float r = x;

    for (int i = 0; i < 16; i++)
        r = 0.5f * (r + x / r);

    return r;
}

static inline float acosf(float x) {
    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;

    float negate = x < 0;
    x = fabsf(x);

    float ret = -0.0187293f;
    ret = ret * x + 0.0742610f;
    ret = ret * x - 0.2121144f;
    ret = ret * x + 1.5707288f;
    ret = ret * sqrtf(1.0f - x);

    return negate ? PI - ret : ret;
}

// vector math
static inline vec2 vec2zero() { return (vec2){0, 0}; }
static inline vec3 vec3zero() { return (vec3){0, 0, 0}; }

static inline vec2 add(vec2 a, int32_t v) { return (vec2){a.x+v, a.y+v}; }
static inline vec2 addv(vec2 a,  vec2 b) { return (vec2){a.x+b.x, a.y+b.y}; }
static inline vec2 get_pos(rec a) { return (vec2){a.x, a.y}; }
static inline vec2 get_size(rec a) { return (vec2){a.w, a.h}; }

static inline vec2f addf(vec2f a, float v) { return (vec2f){a.x+v, a.y+v}; }

static inline float angle(vec2 a, vec2 b) {
    float dot = (float)(a.x * b.x + a.y * b.y);

    float mag1 = sqrtf((float)(a.x * a.x + a.y * a.y));
    float mag2 = sqrtf((float)(b.x * b.x + b.y * b.y));

    if (mag1 == 0.0f || mag2 == 0.0f)
        return 0.0f;

    float cosv = dot / (mag1 * mag2);

    if (cosv > 1.0f) cosv = 1.0f;
    if (cosv < -1.0f) cosv = -1.0f;

    return acosf(cosv);
}

static inline int check_collision_rec(vec2 p, rec r) {
    return (
        p.x >= r.x &&
        p.y >= r.y &&
        p.x <  r.x + r.w &&
        p.y <  r.y + r.h
    );
}

static inline int distance(vec2 a, vec2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;

    return sqrtf(dx * dx + dy * dy);
}

#endif // GOS_MATH_H