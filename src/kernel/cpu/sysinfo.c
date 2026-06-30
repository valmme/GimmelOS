#include "kernel/sysinfo.h"
#include "kernel/kernel.h"
#include "gfx/gfx.h"
#include "lib/kstring.h"

static void cpuid_call(uint32_t code, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    __asm__ volatile(
        "cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(code)
    );
}

const char* sys_get_cpu_vendor(void) {
    static char vendor[13];
    uint32_t a, b, c, d;

    cpuid_call(0, &a, &b, &c, &d);

    ((uint32_t*)vendor)[0] = b;
    ((uint32_t*)vendor)[1] = c;
    ((uint32_t*)vendor)[2] = d;
    vendor[12] = '\0';

    return vendor;
}

const char* sys_get_cpu_name(void) {
    static char name[49];
    uint32_t a, b, c, d;

    cpuid_call(0x80000002, &a, &b, &c, &d);
    ((uint32_t*)name)[0] = a;
    ((uint32_t*)name)[1] = b;
    ((uint32_t*)name)[2] = c;
    ((uint32_t*)name)[3] = d;

    cpuid_call(0x80000003, &a, &b, &c, &d);
    ((uint32_t*)name)[4] = a;
    ((uint32_t*)name)[5] = b;
    ((uint32_t*)name)[6] = c;
    ((uint32_t*)name)[7] = d;

    cpuid_call(0x80000004, &a, &b, &c, &d);
    ((uint32_t*)name)[8]  = a;
    ((uint32_t*)name)[9]  = b;
    ((uint32_t*)name)[10] = c;
    ((uint32_t*)name)[11] = d;

    name[48] = '\0';
    while (*name == ' ')
        kmemmove(name, name + 1, kstrlen(name));

    return name;
}

uint32_t sys_get_cpu_freq(void) {
    uint32_t max, a, b, c, d;
    cpuid_call(0, &max, &b, &c, &d);

    if (max < 0x16)
        return 0;

    cpuid_call(0x16, &a, &b, &c, &d);
    return a;
}

uint32_t sys_get_ram_mb(void) {
    if (!(multiboot->flags & (1 << 0)))
        return 0;

    return (multiboot->mem_lower + multiboot->mem_upper) / 1024;
}

uint32_t sys_get_screen_width(void) { return width; }
uint32_t sys_get_screen_height(void) { return height; }

const char* sys_get_bootloader(void) { return "GRUB Multiboot"; }
const char* sys_get_arch(void) { return "x86 32-bit"; }
const char* sys_get_os(void) { return "GimmelOS v0.2"; }