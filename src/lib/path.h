#pragma once
#include "types.h"
#include "filesystem/fs.h"
#include "kstring.h"
#include "drivers/vga/vga.h"
#define PATH_MAX 512

static inline int32_t cwd_inode = 0;
static inline char cwd_path[PATH_MAX] = "/";

static inline void path_push(char* path, const char* seg) {
    size_t len = kstrlen(path);

    if (path[len - 1] != '/') {
        path[len++] = '/';
        path[len]   = '\0';
    }

    kstrncat(path, seg, PATH_MAX - len - 1);
}

static inline void path_pop(char* path) {
    size_t len = kstrlen(path);

    if (len <= 1) return;
    if (path[len - 1] == '/') len--;

    while (len > 0 && path[len - 1] != '/') len--;

    if (len == 0) len = 1;
    path[len] = '\0';
}

static int resolve_path(const char* path) {
    if (!path || path[0] == '\0') return (int)cwd_inode;

    int node = (path[0] == '/') ? 0 : (int)cwd_inode;
    char buf[PATH_MAX];

    kstrncpy(buf, path, PATH_MAX);

    char *p = buf;
    if (*p == '/') p++;

    while (*p) {
        char seg[FS_MAX_NAME];
        size_t i = 0;

        while (*p && *p != '/') seg[i++] = *p++;

        seg[i] = '\0';

        if (*p == '/') p++;
        if (i == 0) continue;
        if (seg[0] == '.' && i == 1) continue;

        if (seg[0] == '.' && seg[1] == '.' && i == 2) {
            int parent = fs_get_parent(node);
            node = (parent < 0) ? 0 : parent;
            continue;
        }

        int child = fs_find_in(seg, (uint32_t)node);
        if (child < 0) return -1;
        node = child;
    }

    return node;
}
