#ifndef HAL_H
#define HAL_H

#include <muse/gpt.h>
#include <muse/vfs.h>
#include <stdint.h>

typedef uint32_t lba_t;

enum hal_drive_res {
	DRV_SUCCESS,
	DRV_ERR_NO_DRIVE,
	DRV_ERR_NO_SUPPORTED_MODE,
	DRV_ERR_IO_FAILED,
	DRV_ERR_BAD_ARGS,
	DRV_ERR_TIMEOUT,
};

struct hal_drive {
	uint32_t sector_size;
	__attribute__((warn_unused_result)) enum hal_drive_res (*transfer)(
	    struct partition *partition, lba_t lba, uint8_t sector_count,
	    void *data, enum data_dir dir);
	void *backend_data;
};

#endif
