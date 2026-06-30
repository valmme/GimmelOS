#ifndef GOS_UNIFONT_H
#define GOS_UNIFONT_H

#include "lib/types.h"

typedef struct {
    uint32_t codepoint;
    uint8_t bitmap[32];
} glyph_t;

extern const uint16_t unifont_count;
extern const glyph_t unifont[];

#endif // GOS_UNIFONT_H