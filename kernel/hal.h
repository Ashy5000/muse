#ifndef HAL_H
#define HAL_H

#include <stdint.h>

#define DEV_DRIVE 0

typedef uint32_t lba_t;

enum hal_drive_res {
	DRV_SUCCESS,
	DRV_ERR_NO_DRIVE,
	DRV_ERR_NO_SUPPORTED_MODE,
	DRV_ERR_IO_FAILED,
	DRV_ERR_BAD_ARGS,
	DRV_ERR_TIMEOUT,
};

enum hal_drive_dir {
	DRV_READ,
	DRV_WRITE,
};

struct hal_drive {
	uint8_t type;
	enum hal_drive_res status;
	__attribute__((warn_unused_result)) enum hal_drive_res (*transfer)(struct hal_drive *dev, lba_t lba, uint8_t sector_count, void *data, enum hal_drive_dir dir);
	void *backend_data;
};

#endif
