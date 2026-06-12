#ifndef GOS_TYPES_H
#define GOS_TYPES_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef unsigned long size_t;

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;

typedef struct {
    int32_t x;
    int32_t y;
} vec2;

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
} rec;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} gfx_color_t;

#endif // GOS_TYPES_H