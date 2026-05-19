#pragma once
#include "lib/types.h"

void ata_read28(uint32_t lba, uint8_t* buf);
void ata_write28(uint32_t lba, uint8_t* buf);
void ata_init(void);
void detect_disk();