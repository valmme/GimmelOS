#ifndef GOS_TIME_H
#define GOS_TIME_H

#include "lib/types.h"

extern uint64_t ticks_per_sec;
extern uint32_t ticks_per_ms;


static inline uint64_t get_cycles(void) {
    uint32_t low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t)high << 32) | (uint64_t)low;
}

void time_sleep(uint8_t sec);
void time_sleep_ms(uint32_t ms);
uint32_t get_fps(void);

#endif // GOS_TIME_H