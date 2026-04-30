#ifndef ATA_H
#define ATA_H

#include <stdint.h>

enum ata_res {
	ATA_SUCCESS,
	ATA_ERR_NO_DRIVE,
	ATA_ERR_NO_SUPPORTED_MODE,
	ATA_ERR_IO_FAILED,
	ATA_ERR_BAD_ARGS,
	ATA_ERR_TIMEOUT,
};

enum ata_dir {
	ATA_READ,
	ATA_WRITE,
};

struct ata_dev {
	enum ata_res status;
	uint16_t bus;
	uint8_t drive;
	uint32_t lba_28_sector_count;
	bool lba28;
	bool lba48;
};

#define MAX_POLLS 1000

void register_ata();
enum ata_res ata_transfer(struct ata_dev dev, uint32_t lba, uint8_t sector_count, uint16_t *data, enum ata_dir dir);

#endif
