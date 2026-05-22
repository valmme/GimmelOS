#include "types.h"

// basic math
float fabsf(float x) {
    return x < 0 ? -x : x;
}

float sqrtf(float x) {
    if (x <= 0) return 0;
    float r = x;

    for (int i = 0; i < 16; i++)
        r = 0.5f * (r + x / r);

    return r;
}

float acosf(float x) {
    const float PI = 3.14159265f;

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
inline vec2 vec2zero() { return (vec2){0, 0}; }

inline vec2 add(vec2 a, int32_t v) { return (vec2){a.x+v,   a.y+v}; }
inline vec2 addv(vec2 a,  vec2 b) { return (vec2){a.x+b.x, a.y+b.y}; }

float angle(vec2 a, vec2 b) {
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

int check_collision_rec(vec2 p, rec r) {
    return (
        p.x >= r.x &&
        p.y >= r.y &&
        p.x <  r.x + r.w &&
        p.y <  r.y + r.h
    );
}

int distance(vec2 a, vec2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;

    return sqrtf(dx * dx + dy * dy);
}