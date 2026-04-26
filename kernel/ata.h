#ifndef ATA_H
#define ATA_H

#include <stdint.h>

enum ata_res {
	ATA_SUCCESS,
	ATA_ERR_NO_SUPPORTED_MODE,
	ATA_ERR_IO_FAILED,
};

struct ata_dev {
	uint16_t bus;
	uint8_t drive;
	bool present;
	uint32_t lba_28_sector_count;
	bool lba28;
	bool lba48;
};

void register_ata();

#endif
