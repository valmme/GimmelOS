#pragma once
#include "filesystem/fs.h"
#include "ata.h"

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf32_ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} elf32_phdr;

static inline int load_and_run(const char* path) {
    uint8_t buf[4096];
    int id = fs_find_in(path, 0);
    if (id < 0) return -1;

    uint32_t max = sizeof(buf);
    if (!fs_read_by_id(id, buf)) return -1;

    elf32_ehdr* eh = (elf32_ehdr*)buf;

    if (eh->e_ident[0] != 0x7F ||
        eh->e_ident[1] != 'E'  ||
        eh->e_ident[2] != 'L'  || 
        eh->e_ident[3] != 'F') {
        return -2; // not elf
    }

    elf32_phdr* ph = (elf32_phdr*)(buf + eh->e_phoff);

    for (int i = 0; i < eh->e_phnum; i++) {
        if (ph[i].p_type == 1) {
            uint8_t* target = (uint8_t*)ph[i].p_vaddr;
            uint32_t size = ph[i].p_filesz;

            uint8_t* src = buf + ph[i].p_offset;
            for (int j = 0; j < size; j++) {
                target[j] = src[j];
            }

            for (int j = size; j < ph[i].p_memsz; j++) {
                target[j] = 0;
            }
        }
    }

    void (*entry)() = (void(*)())eh->e_entry;
    entry(); // main
    
    return 0;
}