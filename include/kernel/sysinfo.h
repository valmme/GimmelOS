#ifndef GOS_SYSINFO_H
#define GOS_SYSINFO_H

#include "lib/types.h"

uint32_t sys_get_cpu_freq(void);
uint32_t sys_get_ram_mb(void);
uint32_t sys_get_screen_width(void);
uint32_t sys_get_screen_height(void);
uint32_t sys_get_screen_bpp(void);

const char* sys_get_bootloader(void);
const char* sys_get_arch(void);
const char* sys_get_os(void);
const char* sys_get_cpu_name(void);
const char* sys_get_cpu_vendor(void);

#endif // GOS_SYSINFO_H