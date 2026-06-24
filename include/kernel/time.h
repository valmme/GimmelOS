#ifndef GOS_TIME_H
#define GOS_TIME_H

#include "lib/types.h"

static inline uint64_t get_cycles(void) {
    uint32_t low, high;
    __asm__ volatile ("rdtsc" : "=a" (low), "=d" (high));
    return ((uint64_t)high << 32) | (uint64_t)low;
}

void calibrate_timer(void);
void time_sleep(uint8_t sec);
void delay_ms(uint32_t ms);
void update_fps_counter(void);
uint32_t get_fps(void);

#endif