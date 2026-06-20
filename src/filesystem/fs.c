#include "fs.h"
#include "drivers/ata/ata.h"
#include "drivers/serial.h"
#include "lib/kstring.h"

static inode_t inodes[FS_MAX_INODES];
static uint8_t bitmap[FS_MAX_BLOCKS / 8];

static int alloc_block() {
    for (int i = 0; i < FS_MAX_BLOCKS; i++) {
        if (!(bitmap[i / 8] & (1 << (i % 8)))) {
            bitmap[i / 8] |= (1 << (i % 8));
            return i + 3;
        }
    }

    return -1;
}

static void free_block_lba(uint32_t lba) {
    if (lba < 3 || lba >= FS_MAX_BLOCKS + 3) return;
    int b = (int)lba - 3;
    bitmap[b / 8] &= ~(1 << (b % 8));
}

void fs_init() {
    uint8_t buf[512];
    ata_read28(0, buf);
    superblock_t* sb = (superblock_t*)buf;

    if (sb->magic[0] != 'M') {
        serial_print("Formatting disk...");
        superblock_t new_sb = { "MINISF", 0 };
        ata_write28(0, (uint8_t*)&new_sb);

        kmemset(inodes, 0, sizeof(inodes));
        kmemset(bitmap, 0, sizeof(bitmap));

        inodes[0].used = 1;
        inodes[0].is_dir = 1;
        inodes[0].parent = 0;
        kstrcpy(inodes[0].name, "/", FS_MAX_NAME);

        ata_write28(1, (uint8_t*)inodes);
        ata_write28(2, bitmap);
    } 
    
    else {
        ata_read28(1, (uint8_t*)inodes);
        ata_read28(2, bitmap);
        if (!inodes[0].used) {
            inodes[0].used = 1;
            inodes[0].is_dir = 1;
            inodes[0].parent = 0;

            kstrcpy(inodes[0].name, "/", FS_MAX_NAME);
            ata_write28(1, (uint8_t*)inodes);
        }
    }
}

static int fs_find(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_INODES; i++) {
        if (inodes[i].used && inodes[i].parent == parent && kstrcmp(inodes[i].name, name) == 0) {
            return i;
        }
    }

    return -1;
}

int fs_create(const char* name, uint32_t parent, uint8_t is_dir) {
    for (int i = 1; i < FS_MAX_INODES; i++) {
        if (!inodes[i].used) {

            inodes[i].used = 1;
            inodes[i].is_dir = is_dir;
            inodes[i].parent = parent;
            kstrcpy(inodes[i].name, name, FS_MAX_NAME);
            inodes[i].size = 0;

            if (!is_dir) {
                int lba = alloc_block();
                if (lba < 0) {
                    kmemset(&inodes[i], 0, sizeof(inode_t));
                    return -1;
                }

                inodes[i].blocks[0] = lba;
                uint8_t zero[512];
                kmemset(zero, 0, sizeof(zero));
                ata_write28(lba, zero);
            }

            ata_write28(1, (uint8_t*)inodes);
            ata_write28(2, bitmap);
            return i;
        }
    }
    return -1;
}

int fs_write(const char* name, uint32_t parent, uint8_t* data, uint32_t size) {
    int id = fs_find(name, parent);
    if (id < 0) return 0;
    if (inodes[id].is_dir) return 0;
    if (size > 511) size = 511;

    uint8_t buf[512];

    kmemset(buf, 0, sizeof(buf));
    kmemcpy(buf, data, size);
    buf[size] = 0;

    ata_write28(inodes[id].blocks[0], buf);
    inodes[id].size = size;

    ata_write28(1, (uint8_t*)inodes);
    ata_write28(2, bitmap);
    return 1;
}

int fs_read(const char* name, uint32_t parent, uint8_t* out) {
    int id = fs_find(name, parent);
    if (id < 0) return 0;
    if (!inodes[id].used || inodes[id].is_dir) return 0;

    ata_read28(inodes[id].blocks[0], out);
    return 1;
}

int fs_mk(const char* name, uint32_t parent) {
    return fs_create(name, parent, 0);
}

int fs_mkdir(const char* name, uint32_t parent) {
    return fs_create(name, parent, 1);
}

int fs_rm(const char* name, uint32_t parent) {
    int id = fs_find(name, parent);
    if (id < 0) return 0;
    return fs_rm_by_id(id);
}

int fs_rmdir(const char* name, uint32_t parent) {
    int id = fs_find(name, parent);
    if (id < 0) return 0;
    return fs_rmdir_by_id(id);
}

int fs_rm_by_id(int id) {
    if (id <= 0 || id >= FS_MAX_INODES || !inodes[id].used) return 0;
    if (inodes[id].is_dir) return 0;

    free_block_lba(inodes[id].blocks[0]);
    kmemset(&inodes[id], 0, sizeof(inode_t));
    ata_write28(1, (uint8_t*)inodes);
    ata_write28(2, bitmap);
    return 1;
}

int fs_rmdir_by_id(int id) {
    if (id <= 0 || id >= FS_MAX_INODES || !inodes[id].used) return 0;
    if (!inodes[id].is_dir) return 0;
    for (int i = 0; i < FS_MAX_INODES; i++) {
        if (i == id) continue;
        if (inodes[i].used && inodes[i].parent == (uint32_t)id) return 0;
    }

    kmemset(&inodes[id], 0, sizeof(inode_t));
    ata_write28(1, (uint8_t*)inodes);
    return 1;
}

int fs_find_in(const char *name, uint32_t parent) {
    return fs_find(name, parent);
}

int fs_get_parent(int id) {
    if (id < 0 || id >= FS_MAX_INODES || !inodes[id].used) return -1;
    return (int)inodes[id].parent;
}

int fs_is_dir(int id) {
    if (id < 0 || id >= FS_MAX_INODES || !inodes[id].used) return 0;
    return inodes[id].is_dir;
}

int fs_read_by_id(int id, uint8_t* out) {
    if (id < 0 || id >= FS_MAX_INODES || !inodes[id].used || inodes[id].is_dir) return 0;
    ata_read28(inodes[id].blocks[0], out);
    return 1;
}

int fs_write_by_id(int id, uint8_t* data, uint32_t size) {
    if (id < 0 || id >= FS_MAX_INODES || !inodes[id].used || inodes[id].is_dir) return 0;
    if (size > 511) size = 511;

    uint8_t buf[512];

    kmemset(buf, 0, sizeof(buf));
    kstrncpy((char *)buf, (char *)data, size);
    ata_write28(inodes[id].blocks[0], buf);
    inodes[id].size = size;

    ata_write28(1, (uint8_t *)inodes);
    return 1;
}

void fs_split_path(const char* path, char* dir_out, char* name_out) {
    int last = -1;
    for (int i = 0; path[i]; i++) if (path[i] == '/') last = i;

    if (last < 0) {
        dir_out[0] = '\0';
        kstrncpy(name_out, path, FS_MAX_NAME);
    } 
    
    else {
        if (last == 0) {
            dir_out[0] = '/';
            dir_out[1] = '\0';
        } 
        
        else {
            kstrncpy(dir_out, path, last);
            dir_out[last] = '\0';
        }
        kstrncpy(name_out, path + last + 1, FS_MAX_NAME);
    }
}

void fs_get_path(int id, char *out, size_t maxlen) {
    if (id == 0) {
        kstrncpy(out, "/", maxlen);
        return;
    }

    char segments[16][FS_MAX_NAME];
    int depth = 0;
    int cur = id;

    while (cur > 0 && cur < FS_MAX_INODES && depth < 16) {
        kstrncpy(segments[depth++], inodes[cur].name, FS_MAX_NAME);
        cur = (int)inodes[cur].parent;
    }

    size_t pos = 0;
    for (int i = depth - 1; i >= 0; i--) {
        if (pos < maxlen - 1) out[pos++] = '/';
        for (int j = 0; segments[i][j] && pos < maxlen - 1; j++) out[pos++] = segments[i][j];
    }

    if (pos == 0 && maxlen > 0) out[pos++] = '/';
    out[pos] = '\0';
}

void fs_list_names(uint32_t parent, char names[][FS_MAX_NAME], int* count) {
    int idx[32];
    int n = 0;

    for (int i = 0; i < FS_MAX_INODES && n < 32; i++) {
        if (inodes[i].used && inodes[i].parent == parent) {
            idx[n++] = i;
        }
    }

    for (int a = 0; a < n - 1; a++) {
        for (int b = 0; b < n - 1 - a; b++) {
            int i1 = idx[b];
            int i2 = idx[b + 1];
            int swap = 0;

            if (inodes[i1].is_dir != inodes[i2].is_dir) {
                if (!inodes[i1].is_dir && inodes[i2].is_dir) swap = 1;
            }
            
            else if (kstrcmp(inodes[i1].name, inodes[i2].name) > 0) {
                swap = 1;
            }

            if (swap) {
                int tmp = idx[b];
                idx[b] = idx[b + 1];
                idx[b + 1] = tmp;
            }
        }
    }

    *count = n;
    
    for (int k = 0; k < n; k++) {
        int i = idx[k];
        kstrncpy(names[k], inodes[i].name, FS_MAX_NAME);

        if (inodes[i].is_dir) {
            int nlen = kstrlen(names[k]);
            if (nlen < FS_MAX_NAME - 1) {
                names[k][nlen]     = '/';
                names[k][nlen + 1] = '\0';
            }
        }
    }
}