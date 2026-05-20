#ifndef ATA_H
#define ATA_H

#include <stdbool.h>
#include <stdint.h>

struct ata_dev {
	uint16_t bus;
	uint8_t drive;
	uint32_t lba_28_sector_count;
	bool lba28;
	bool lba48;
};

#define MAX_POLLS 10000
#define SECTOR_SIZE 512

void register_ata();

#endif
