#pragma once 
#include <stddef.h>

static inline size_t kstrlen(const char* s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static inline int kstrcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char )*a - (unsigned char)*b;
}

static inline int kstrncmp(const char* a, const char* b, size_t n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    if (n == (size_t)-1) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}
 
static inline void kmemset(void* dst, int val, size_t n) {
    unsigned char *p = (unsigned char*)dst;
    while (n--) *p++ = (unsigned char)val;
}