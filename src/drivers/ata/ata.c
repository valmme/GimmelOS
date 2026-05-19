#include "ata.h"
#include "../vga/vga.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    __asm__ volatile ("inb %1, %0" : "=a"(data) : "d"(port));
    return data;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t data;
    __asm__ volatile ("inw %1, %0" : "=a"(data) : "d"(port));
    return data;
}

static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile ("outb %0, %1" : : "a"(data), "d"(port));
}

static inline void outw(uint16_t port, uint16_t data) {
    __asm__ volatile ("outw %0, %1" : : "a"(data), "d"(port));
}


void ata_read28(uint32_t lba, uint8_t* buf) {
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x20);

    while (inb(0x1F7) & 0x80);

    while (!(inb(0x1F7) & 0x08));

    for (int i = 0; i < 256; i++) {
        ((uint16_t*)buf)[i] = inw(0x1F0);
    }
}

void ata_write28(uint32_t lba, uint8_t* buf) {
    while (inb(0x1F7) & 0x80);

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x30);

    uint32_t timeout = 100000;
    uint8_t status;
    do {
        status = inb(0x1F7);

        if (status & 0x01) {
            vga_error("ATA error");
            return;
        }
        if (--timeout == 0) {
            vga_error("ATA timeout");
            return;
        }

        if (status & 0x20) {
            vga_error("Drive fault");
            return;
        }
    } 
    
    while (!(status & 0x08));

    for (int i = 0; i < 256; i++) {
        uint16_t data = ((uint16_t*)buf)[i];
        __asm__ volatile("outw %0, %1" :: "a"(data), "Nd"(0x1F0));
    }

    outb(0x1F7, 0xE7);
    while (inb(0x1F7) & 0x80);
    while (inb(0x1F7) & 0x08);
}

int ata_wait_ready() {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(0x1F7);
        if (!(s & 0x80)) return 1;
    }
}

int ata_wait_drq() {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(0x1F7);
        if (s & 0x08) return 1;
        if (s & 0x01) return 0;
    }

    return 0;
}

void detect_disk() {
    outb(0x1F6, 0xA0);
    outb(0x1F7, 0xEC);

    if (!ata_wait_ready()) {
        vga_error("ATA: not ready");
        return;
    }

    if (!ata_wait_drq()) {
        vga_error("ATA: no DRQ (no disk)");
        return;
    }

    uint16_t id[256];
    for (int i = 0; i < 256; i++) {
        id[i] = inw(0x1F0);
    }

    vga_info("Disk detected");
}