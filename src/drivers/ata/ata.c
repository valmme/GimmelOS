#include "ata.h"
#include "kernel/cpu/io.h"
#include "../vga/vga.h"

static int ata_wait() {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(0x1F7);

        if (s & 0x01) return -1;
        if (!(s & 0x80) && (s & 0x08)) return 1;
    }

    return 0;
}

void ata_read28(uint32_t lba, uint8_t* buf) {
    while (inb(0x1F7) & 0x80);

    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    outb(0x1F2, 1);
    outb(0x1F3, (uint8_t)lba);
    outb(0x1F4, (uint8_t)(lba >> 8));
    outb(0x1F5, (uint8_t)(lba >> 16));
    outb(0x1F7, 0x20);

    if (ata_wait() != 1) return;

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

    if (ata_wait() != 1) {
        vga_error("ATA write failed");
        return;
    }

    for (int i = 0; i < 256; i++) {
        outw(0x1F0, ((uint16_t*)buf)[i]);
    }

    outb(0x1F7, 0xE7);

    ata_wait();
}

void detect_disk() {
    outb(0x1F6, 0xA0);

    for (int i = 0; i < 1000000; i++) {
        if (!(inb(0x1F7) & 0x80)) break;
    }

    outb(0x1F7, 0xEC);

    if (!ata_wait(0x80, 0)) {
        vga_error("ATA no response");
        return;
    }

    if (inb(0x1F7) & 0x01) {
        vga_error("ATA error");
        return;
    }

    if (!ata_wait(0x88, 0x08)) {
        vga_error("No disk (DRQ fail)");
        return;
    }

    uint16_t id[256];

    for (int i = 0; i < 256; i++) {
        id[i] = inw(0x1F0);
    }

    vga_info("Disk OK");
}