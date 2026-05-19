#include "fs.h"
#include "../drivers/vga/vga.h"
#include "lib/kstring.h"

static entry_t entries[FS_MAX_ENTRIES];
static uint32_t next_free_sector = 100;

static uint32_t alloc_sector(void) {
    return next_free_sector++;
}

void fs_init(void) {
    kmemset(entries, 0, sizeof(entries));
}

entry_t* fs_find(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_ENTRIES; i++) {
        if (entries[i].type != FS_TYPE_FREE && kstrcmp(entries[i].name, name) == 0 && entries[i].parent == parent) {
            return &entries[i];
        }
    }
    return 0;
}

void fs_read(const char* name, uint32_t parent, uint8_t* buf) {
    entry_t* f = fs_find(name, parent);
    if (!f) return;

    ata_read28(f->sector, buf);
    buf[f->size] = '\0';
}

void fs_write(const char* name, uint32_t parent, uint8_t* data) {
    entry_t* f = fs_find(name, parent);
    if (!f) return;

    uint8_t block[512];
    kmemset(block, 0, 512);
    size_t data_len = kstrlen((char*)data);
    if (data_len > 511) data_len = 511;

    kstrncpy((char*)block, (char*)data, data_len);
    block[data_len] = '\0';

    ata_write28(f->sector, block);
    f->size = data_len;
    uint8_t debug_buf[512];
    ata_read28(f->sector, debug_buf);
}

int fs_mkdir(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_ENTRIES; i++) {
        if (entries[i].type == FS_TYPE_FREE) {
            kmemset(entries[i].name, 0, FS_NAME_LEN);
            kstrcpy(entries[i].name, name, FS_NAME_LEN);

            entries[i].type = FS_TYPE_DIR;
            entries[i].parent = parent;
            entries[i].sector = alloc_sector();
            entries[i].size = 0;

            uint8_t zero[512];
            kmemset(zero, 0, 512);
            ata_write28(entries[i].sector, zero);

            return 1;
        }
    }
    return 0;
}

int fs_mk(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_ENTRIES; i++) {
        if (entries[i].type == FS_TYPE_FREE) {
            kmemset(entries[i].name, 0, FS_NAME_LEN);
            kstrcpy(entries[i].name, name, FS_NAME_LEN);

            entries[i].type = FS_TYPE_FILE;
            entries[i].parent = parent;
            entries[i].sector = alloc_sector();
            entries[i].size = 0;

            uint8_t zero[512];
            kmemset(zero, 0, 512);
            ata_write28(entries[i].sector, zero);

            return 1;
        }
    }
    return 0;
}

void fs_list(uint32_t parent) {
    for (int i = 0; i < FS_MAX_ENTRIES; i++) {
        if (entries[i].type != FS_TYPE_FREE && entries[i].parent == parent) {
            if (entries[i].type == FS_TYPE_DIR) {
                vga_print("/");
            }

            vga_print(entries[i].name);
            vga_putchar('\n');
        }
    }
}