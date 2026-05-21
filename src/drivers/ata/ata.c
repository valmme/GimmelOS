#include "ata.h"
#include "kernel/cpu/io.h"
#include "../vga/vga.h"

static int ata_wait(uint8_t set_mask, uint8_t clear_mask) {
    for (int i = 0; i < 1000000; i++) {
        uint8_t s = inb(ATA_STATUS);

        if (s & ATA_SR_ERR) return -1;
        if (!(s & clear_mask) && (s & set_mask)) return 1;
    }

    return 0;
}

static void lba28_setup(uint32_t lba, uint8_t drive_bits) {
    while (inb(ATA_STATUS) & ATA_SR_BSY);

    outb(ATA_DRIVE, 0xE0 | drive_bits | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LO, (uint8_t)lba);
    outb(ATA_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HI, (uint8_t)(lba >> 16));
}

void ata_read28(uint32_t lba, uint8_t *buf) {
    lba28_setup(lba, 0);
    outb(ATA_CMD, ATA_CMD_READ);
 
    if (ata_wait(ATA_SR_DRQ, ATA_SR_BSY) != 1) {
        vga_error("ATA read failed");
        return;
    }
 
    for (int i = 0; i < 256; i++)
        ((uint16_t *)buf)[i] = inw(ATA_DATA);
}
 
void ata_write28(uint32_t lba, uint8_t *buf) {
    lba28_setup(lba, 0);
    outb(ATA_CMD, ATA_CMD_WRITE);
 
    if (ata_wait(ATA_SR_DRQ, ATA_SR_BSY) != 1) {
        vga_error("ATA write failed");
        return;
    }
 
    for (int i = 0; i < 256; i++)
        outw(ATA_DATA, ((uint16_t *)buf)[i]);
 
    outb(ATA_CMD, ATA_CMD_FLUSH);
    ata_wait(ATA_SR_DRDY, ATA_SR_BSY);
}

void detect_disk(void) {
    outb(ATA_DRIVE, 0xA0);
    for (int i = 0; i < 15; i++) inb(ATA_STATUS);

    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LO,   0);
    outb(ATA_LBA_MID,  0);
    outb(ATA_LBA_HI,   0);
    outb(ATA_CMD, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_STATUS);
    if (status == 0x00) { vga_error("ATA: no drive present"); return; }

    int timeout = 1000000;
    while (--timeout) {
        status = inb(ATA_STATUS);
        if (!(status & ATA_SR_BSY)) break;
    }
    if (!timeout) { vga_error("ATA: no response"); return; }

    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HI) != 0) {
        vga_warn("ATA: ATAPI device, skipping");
        return;
    }

    while (1) {
        status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) { vga_error("ATA: IDENTIFY error"); return; }
        if (status & ATA_SR_DRQ) break;
    }

    uint16_t id[256];
    for (int i = 0; i < 256; i++) id[i] = inw(ATA_DATA);
    (void)id;

    vga_info("Disk found");
}