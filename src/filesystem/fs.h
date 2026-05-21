#pragma once
#include "lib/types.h"

#define FS_MAX_INODES 128
#define FS_MAX_NAME  32
#define FS_MAX_BLOCKS 4096
#define FS_BLOCK_SIZE 512

typedef struct {
    char name[FS_MAX_NAME];
    uint8_t used;
    uint8_t is_dir;
    uint32_t size;
    uint32_t blocks[8];
    uint32_t parent;
} inode_t;

typedef struct {
    char magic[8];
    uint32_t inode_count;
} superblock_t;

void fs_init();
int fs_read(const char* name, uint32_t parent, uint8_t* out);
int fs_write(const char* name, uint32_t parent, uint8_t* data, uint32_t size);
void fs_list(uint32_t parent);
int fs_mkdir(const char* name, uint32_t parent);
int fs_mk(const char* name, uint32_t parent);
int fs_rm(const char* name, uint32_t parent);
int fs_rmdir(const char* name, uint32_t parent);

int fs_find_in(const char* name, uint32_t parent);
int fs_get_parent(int id);
int fs_is_dir(int id);
int fs_read_by_id(int id, uint8_t* out);
int fs_write_by_id(int id, uint8_t* data, uint32_t size);
void fs_split_path(const char* path, char* dir_out, char* name_out);
void fs_get_path(int id, char* out, size_t maxlen);