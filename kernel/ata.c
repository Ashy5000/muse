#include <stdbool.h>
#include "pci.h"
#include "alloc.h"
#include "io.h"
#include "ata.h"
#include "gpt.h"

#define ATA_BUS_PRIMARY 0x1F0
#define ATA_BUS_SECONDARY 0x170

#define ATA_CONTROL_OFFSET 0x206

#define ATA_DRIVE_MASTER 0xA0
#define ATA_DRIVE_SLAVE 0xB0

#define ATA_REG_DATA 0
#define ATA_REG_ERR 1
#define ATA_REG_FEAT 1
#define ATA_REG_SECT_CNT 2
#define ATA_REG_LBA_LO 3
#define ATA_REG_LBA_MID 4
#define ATA_REG_LBA_HI 5
#define ATA_REG_DRIVE 6
#define ATA_REG_STAT 7
#define ATA_REG_CMD 7

#define ATA_CTRL_REG_ALT_STATUS 0
#define ATA_CTRL_REG_CTRL 0
#define ATA_CTRL_REG_DRIVE 1

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30
#define ATA_CMD_FLUSH 0xE7

#define ATA_FLAG_BSY 0x80
#define ATA_FLAG_ERR 0x01
#define ATA_FLAG_DRIVE_FLT 0x20
#define ATA_FLAG_DRQ 0x08
#define ATA_FLAG_NIEN 0x02

#define MAX_POLLS 1000

bool detect_float(uint16_t bus) {
	return inb(bus + ATA_REG_STAT) == 0xFF;
}

enum ata_res poll_data(struct ata_dev *dev) {
	uint32_t polls;
	for (;;) {
		polls++;
		if (polls >= MAX_POLLS) {
			return ATA_ERR_TIMEOUT;
		}
		uint8_t status = inb(dev->bus + ATA_REG_STAT);
		if (status & ATA_FLAG_BSY) {
			continue;
		}
		if ((status & ATA_FLAG_ERR) || (status & ATA_FLAG_DRIVE_FLT)) {
			return ATA_ERR_IO_FAILED;
		}
		if (status & ATA_FLAG_DRQ) {
			return ATA_SUCCESS;
		}
	}
}

void delay_400ns(struct ata_dev *dev) {
	for (uint32_t i = 0; i < 15; i++) {
		inb(dev->bus + ATA_REG_STAT);
	}
}

enum ata_res select_region(struct ata_dev *dev, uint32_t lba, uint8_t sector_count) {
	uint32_t polls = 0;
	while (inb(dev->bus + ATA_REG_STAT) & (ATA_FLAG_BSY | ATA_FLAG_DRQ)) {
		polls++;
		if (polls >= MAX_POLLS) {
			return ATA_ERR_TIMEOUT;
		}
	}
	outb(dev->bus + ATA_REG_DRIVE, dev->drive | ((lba >> 24) & 0xf) | 0xE0);
	delay_400ns(dev);
	if (inb(dev->bus + ATA_REG_ERR) | inb(dev->bus + ATA_REG_STAT) & (ATA_FLAG_BSY | ATA_FLAG_DRQ)) {
		return ATA_ERR_IO_FAILED;
	}
	outb(dev->bus + ATA_REG_SECT_CNT, sector_count);
	outb(dev->bus + ATA_REG_LBA_LO, lba);
	outb(dev->bus + ATA_REG_LBA_MID, lba >> 8);
	outb(dev->bus + ATA_REG_LBA_HI, lba >> 16);
	return ATA_SUCCESS;
}

enum ata_res flush_cache(struct ata_dev *dev) {
	outb(dev->bus + ATA_REG_CMD, ATA_CMD_FLUSH);
	uint32_t polls = 0;
	while (inb(dev->bus + ATA_REG_STAT) & ATA_FLAG_BSY) {
		polls++;
		if (polls >= MAX_POLLS) {
			return ATA_ERR_TIMEOUT;
		}
	}
	return ATA_SUCCESS;
}

enum ata_res ata_transfer(struct ata_dev *dev, uint32_t lba, uint8_t sector_count, uint16_t *data, enum ata_dir dir) {
	if (!dev->lba28) {
		return ATA_ERR_NO_SUPPORTED_MODE;
	}
	enum ata_res res = select_region(dev, lba, sector_count);
	if (res) {
		return res;
	}
	if (dir == ATA_READ) {
		outb(dev->bus + ATA_REG_CMD, ATA_CMD_READ_SECTORS);
	} else if (dir == ATA_WRITE) {
		outb(dev->bus + ATA_REG_CMD, ATA_CMD_WRITE_SECTORS);
	} else {
		return ATA_ERR_BAD_ARGS;
	}
	// Make sure ERR and DF bits from last command are clear
	for (uint32_t i = 0; i < 4; i++) {
		inb(dev->bus + ATA_REG_STAT);
	}
	for (uint32_t i = 0; i < sector_count; i++) {
		res = poll_data(dev);
		if (res) {
			return res;
		}
		for (uint32_t j = 0; j < 256; j++) {
			if (dir == ATA_READ) {
				data[j + (i * 256)] = inw(dev->bus + ATA_REG_DATA);
			} else {
				outw(dev->bus + ATA_REG_DATA, data[j + (i * 256)]);
			}
		}

		delay_400ns(dev);
	}

	flush_cache(dev);

	uint8_t status = inb(dev->bus + ATA_REG_STAT);
	if (status & (ATA_FLAG_ERR | ATA_FLAG_DRIVE_FLT)) {
		return ATA_ERR_IO_FAILED;
	}

	return ATA_SUCCESS;
}

struct ata_dev detect_drive(uint16_t bus, uint8_t drive) {
	struct ata_dev dev;
	dev.bus = bus;
	dev.drive = drive;
	dev.status = ATA_ERR_NO_DRIVE;
	outb(bus + ATA_REG_DRIVE, drive);
	for (uint16_t port = bus + ATA_REG_SECT_CNT; port <= bus + ATA_REG_LBA_HI; port++) {
		outb(port, 0);
	}
	outb(bus + ATA_REG_CMD, ATA_CMD_IDENTIFY);
	if (inb(bus + ATA_REG_STAT) == 0) {
		return dev;
	}
	uint32_t polls = 0;
	while ((inb(bus + ATA_REG_STAT) & ATA_FLAG_BSY) > 0) {
		polls++;
		if (polls > MAX_POLLS) {
			dev.status = ATA_ERR_TIMEOUT;
			return dev;
		}
	}
	if (inb(bus + ATA_REG_LBA_MID) || inb(bus + ATA_REG_LBA_HI)) {
		return dev;
	}
	polls = 0;
	while ((inb(bus + ATA_REG_STAT) & ATA_FLAG_DRQ) == 0 && (inb(bus + ATA_REG_STAT) & ATA_FLAG_ERR) == 0) {
		polls++;
		if (polls > MAX_POLLS) {
			dev.status = ATA_ERR_TIMEOUT;
			return dev;
		}
	}
	if ((inb(bus + ATA_REG_STAT) & 0x1) > 0) {
		return dev;
	}
	dev.status = ATA_SUCCESS;
	for (uint32_t i = 0; i < 256; i++) {
		uint16_t data = inw(bus + ATA_REG_DATA);
		if (i == 83) {
			if (data & 0x400) {
				dev.lba48 = true;
			}
		}
		if (i == 60) {
			dev.lba_28_sector_count = data << 16;
		}
		if (i == 61) {
			dev.lba_28_sector_count |= data;
			if (dev.lba_28_sector_count) {
				dev.lba28 = true;
			}
		}
	}

	// Disable interrupts
	outb(bus + ATA_CTRL_REG_CTRL, inb(bus + ATA_CTRL_REG_CTRL) | ATA_FLAG_NIEN);

	init_gpt(&dev);

	return dev;
}

void detect_bus(uint16_t bus) {
	if (detect_float(bus)) {
		return;
	}
	detect_drive(bus, ATA_DRIVE_MASTER);
	detect_drive(bus, ATA_DRIVE_SLAVE);
}

void init_ata(struct pci_func pci_fn) {
	// Skip buses in PCI native mode (not supported yet)
	if ((pci_fn.prog_if & 1) == 0) {
		detect_bus(ATA_BUS_PRIMARY);
	}
	if ((pci_fn.prog_if & 4) == 0) {
		detect_bus(ATA_BUS_SECONDARY);
	}
}

void register_ata() {
	struct pci_handler *handler = (struct pci_handler*)kmalloc(sizeof(struct pci_handler));
	// Initialize ATA when the IDE controller is found during PCI enumeration
	handler->class_code = 0x1; // Mass storage controller
	handler->subclass_code = 0x1; // IDE controller
	handler->init = init_ata;
	pci_register_handler(handler);
}
