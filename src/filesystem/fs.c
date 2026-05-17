#include "fs.h"
#include "../vga.h"
#include "../kstring.h"


static entry_t entries[FS_MAX_ENTRIES];

void fs_init(void) {
    ata_read28(1, (uint8_t*)entries);
}

entry_t* fs_find(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_ENTRIES; i++) {
        if (entries[i].type != 0 && kstrcmp(entries[i].name, name) == 0 && entries[i].parent == parent) {
            return &entries[i];
        }
    }
    return 0;
}

void fs_read(const char* name, uint32_t parent, uint8_t* buf) {
    entry_t* f = fs_find(name, parent);
    if (!f) return;

    ata_read28(f->sector, buf);
}

void fs_write(const char* name, uint32_t parent, uint8_t* data) {
    entry_t* f = fs_find(name, parent);
    if (!f) return;

    ata_write28(f->sector, data);
}

int fs_mkdir(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_ENTRIES; i++) {
        if (entries[i].type == 0) {
            kmemset(entries[i].name, 0, FS_NAME_LEN);
            kstrcpy(entries[i].name, name, FS_NAME_LEN);

            entries[i].type   = FS_TYPE_DIR;
            entries[i].parent = parent;
            entries[i].sector = 0;
            entries[i].size   = 0;

            return 1;
        }
    }

    return 0;
}

int fs_mk(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_ENTRIES; i++) {
        if (entries[i].type == 0) {
            kmemset(entries[i].name, 0, FS_NAME_LEN);
            kstrcpy(entries[i].name, name, FS_NAME_LEN);

            entries[i].type   = FS_TYPE_FILE;
            entries[i].parent = parent;
            entries[i].sector = 0;
            entries[i].size   = 0;

            return 1;
        }
    }

    return 0;
}

void fs_list(uint32_t parent) {
    for (int i = 0; i < FS_MAX_ENTRIES; i++) {
        if (entries[i].parent == parent && entries[i].type != 0) {
            if (entries[i].type == FS_TYPE_DIR) vga_print("/");
            
            vga_println(entries[i].name);
        }
    }
}