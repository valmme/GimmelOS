#pragma once
#include "lib/types.h"

#define FS_MAX_ENTRIES 16
#define FS_NAME_LEN  16

typedef struct {
    char name[16];
    uint32_t sector;
    uint32_t parent;
    uint32_t size;
    uint8_t type;
} entry_t;

#define FS_TYPE_FREE 0
#define FS_TYPE_FILE 1
#define FS_TYPE_DIR  2

void fs_read(const char* name, uint32_t parent, uint8_t* buf);
void fs_write(const char* name, uint32_t parent, uint8_t* data);
void fs_list(uint32_t parent);
int fs_mkdir(const char* name, uint32_t parent);
int fs_mk(const char* name, uint32_t parent);