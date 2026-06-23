#include "kernel/time.h"

static uint64_t last_time = 0;
static uint64_t frame_count = 0;
static uint32_t current_fps = 0;

static const uint64_t CPU_FREQ = 2000000000;

uint32_t get_fps(void) {
    frame_count++;
    uint64_t current_time = get_cycles();
    
    if (current_time - last_time >= CPU_FREQ) {
        current_fps = frame_count;
        frame_count = 0;
        last_time = current_time;
    }
    
    return current_fps;
}