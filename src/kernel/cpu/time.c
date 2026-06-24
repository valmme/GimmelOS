#include "kernel/time.h"
#include "kernel/io.h"

static uint64_t last_time = 0;
static uint64_t frame_count = 0;
static uint32_t current_fps = 0;
static uint64_t ticks_per_sec = 0;
static uint32_t ticks_per_ms = 0;

void calibrate_timer(void) {
    uint64_t start = get_cycles();
    delay_ms(100);
    uint64_t end = get_cycles();
    ticks_per_ms = (uint32_t)((end - start) / 100);
    ticks_per_sec = (uint64_t)ticks_per_ms * 1000;
    last_time = get_cycles();
}

void update_fps_counter(void) {
    frame_count++;
    uint64_t current_time = get_cycles();

    if (current_time - last_time >= ticks_per_sec) {
        current_fps = (uint32_t)frame_count;
        frame_count = 0;
        last_time = current_time;
    }
}

uint32_t get_fps(void) {
    return current_fps;
}

void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 11932; i++) {
        outb(0x80, 0);
    }
}

void time_sleep(uint8_t sec) {
    delay_ms(1000 * sec);
}