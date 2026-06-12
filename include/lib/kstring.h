#ifndef GOS_KSTRING_H
#define GOS_KSTRING_H

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

static inline void kstrcpy(char* dst, const char* src, size_t max) {
    size_t i = 0;
    for (; i < max - 1 && src[i]; i++) {
        dst[i] = src[i];
    }
    
    dst[i] = '\0';
}


static inline void kstrncpy(char* dst, const char* src, size_t max) {
    size_t i = 0;
    if (max == 0) return;

    for (; i < max - 1 && src[i]; i++) {
        dst[i] = src[i];
    }

    dst[i] = '\0';
}

static inline char* kstrcat(char* dst, const char* src) {
    int i = 0;
    int j = 0;

    while (dst[i] != '\0') i++;
    while (src[j] != '\0') {
        dst[i + j] = src[j];
        j++;
    }

    dst[i + j] = '\0';
    return dst;
}

static inline char* kstrcatc(char* dst, char c) {
    int i = 0;
    
    while (dst[i] != '\0') i++;

    dst[i] = c;
    dst[i + 1] = '\0';

    return dst;
}

static inline void kstrncat(char* dst, const char* src, size_t n) {
    size_t dlen = kstrlen(dst);
    size_t i = 0;
    
    while (i < n && src[i]) dst[dlen + i] = src[i++];
    dst[dlen + i] = '\0';
}

static inline void* kmemcpy(void* dest, const void* src, unsigned int n) {
    unsigned char *d = dest;
    const unsigned char *s = src;

    for (unsigned int i = 0; i < n; i++) {
        d[i] = s[i];
    }

    return dest;
}

#endif // GOS_KSTRING_H