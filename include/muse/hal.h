#ifndef HAL_H
#define HAL_H

#include <muse/gpt.h>
#include <muse/vfs.h>
#include <stdint.h>

typedef uint32_t lba_t;

#define DRV_ERRORS                                                             \
	E(DRV_SUCCESS)                                                         \
	E(DRV_ERR_NO_DRIVE)                                                    \
	E(DRV_ERR_NO_SUPPORTED_MODE)                                           \
	E(DRV_ERR_IO_FAILED) E(DRV_ERR_BAD_ARGS) E(DRV_ERR_TIMEOUT)
#define E(x) x,
enum hal_drive_res { DRV_ERRORS DRV_ERR_TOP };
#undef E
#define E(x) #x,
extern const char *const hal_drive_errs[DRV_ERR_TOP];

struct hal_drive {
	uint32_t sector_size;
	__attribute__((warn_unused_result)) enum hal_drive_res (*transfer)(
	    struct partition *partition, lba_t lba, uint8_t sector_count,
	    void *data, enum data_dir dir);
	void *backend_data;
};

#endif
