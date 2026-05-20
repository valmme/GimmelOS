#include "fs.h"
#include "drivers/ata/ata.h"
#include "lib/kstring.h"
#include "drivers/vga/vga.h"

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

void fs_init() {
    uint8_t buf[512];

    ata_read28(0, buf);
    superblock_t* sb = (superblock_t*)buf;

    if (sb->magic[0] != 'M') {
        vga_info("Formatting disk...");

        superblock_t new_sb = { "MINISF", 0 };
        ata_write28(0, (uint8_t*)&new_sb);

        kmemset(inodes, 0, sizeof(inodes));
        ata_write28(1, (uint8_t*)inodes);

        kmemset(bitmap, 0, 512);
        ata_write28(2, bitmap);
    }

    else {
        ata_read28(1, (uint8_t*)inodes);
        ata_read28(2, bitmap);
    }
}

static int fs_find(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_INODES; i++) {
        if (inodes[i].used &&
            inodes[i].parent == parent &&
            kstrcmp(inodes[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int fs_create(const char* name, uint32_t parent, uint8_t is_dir) {
    for (int i = 0; i < FS_MAX_INODES; i++) {
        if (!inodes[i].used) {
            inodes[i].used = 1;
            inodes[i].is_dir = is_dir;
            inodes[i].parent = parent;

            kstrcpy(inodes[i].name, name, FS_MAX_NAME);
            inodes[i].size = 0;

            return i;
        }
    }

    return -1;
}

int fs_write(const char* name, uint32_t parent, uint8_t* data, uint32_t size) {
    int id = fs_find(name, parent);

    if (id < 0) return 0;

    if (size > 511) size = 511;

    uint8_t buf[512];
    kmemset(buf, 0, 512);
    kstrncpy((char*)buf, (char*)data, size);

    int lba = 3 + id;

    ata_write28(lba, buf);

    inodes[id].blocks[0] = lba;
    inodes[id].size = size;

    ata_write28(1, (uint8_t*)inodes);

    return 1;
}

int fs_read(const char* name, uint32_t parent, uint8_t* out) {
    int id = fs_find(name, parent);
    if (id < 0) return 0;

    if (!inodes[id].used) return 0;

    ata_read28(inodes[id].blocks[0], out);
    return 1;
}

int fs_mk(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_INODES; i++) {
        if (!inodes[i].used) {
            inodes[i].used = 1;
            inodes[i].is_dir = 0;
            inodes[i].parent = parent;
            inodes[i].size = 0;

            kstrcpy(inodes[i].name, name, FS_MAX_NAME);

            uint8_t zero[512];
            kmemset(zero, 0, 512);

            int lba = 3 + i;
            inodes[i].blocks[0] = lba;

            ata_write28(lba, zero);
            ata_write28(1, (uint8_t*)inodes);

            return i;
        }
    }
    return -1;
}

int fs_mkdir(const char* name, uint32_t parent) {
    for (int i = 0; i < FS_MAX_INODES; i++) {
        if (!inodes[i].used) {
            inodes[i].used = 1;
            inodes[i].is_dir = 1;
            inodes[i].parent = parent;
            inodes[i].size = 0;

            kstrcpy(inodes[i].name, name, FS_MAX_NAME);

            inodes[i].blocks[0] = 0;

            ata_write28(1, (uint8_t*)inodes);

            return i;
        }
    }
    return -1;
}

int fs_rm(const char* name, uint32_t parent) {
    int id = fs_find(name, parent);
    if (id < 0) return 0;

    if (inodes[id].is_dir) return 0;

    if (inodes[id].blocks[0]) {
        bitmap[inodes[id].blocks[0] - 3] = 0;
    }

    kmemset(&inodes[id], 0, sizeof(inode_t));

    ata_write28(1, (uint8_t*)inodes);
    ata_write28(2, bitmap);

    return 1;
}

int fs_rmdir(const char* name, uint32_t parent) {
    int id = fs_find(name, parent);
    if (id < 0) return 0;

    if (!inodes[id].is_dir) return 0;

    for (int i = 0; i < FS_MAX_INODES; i++) {
        if (inodes[i].used && inodes[i].parent == id) {
            return 0;
        }
    }

    kmemset(&inodes[id], 0, sizeof(inode_t));

    ata_write28(1, (uint8_t*)inodes);

    return 1;
}

void fs_list(uint32_t parent) {
    for (int i = 0; i < FS_MAX_INODES; i++) {
        if (inodes[i].used && inodes[i].parent == parent) {

            vga_set_color(VGA_WHITE, VGA_BLACK);
            vga_print(inodes[i].name);

            if (inodes[i].is_dir) {
                vga_println("/");
            }

            else {
                vga_putchar('\n');
            }
        }
    }

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}