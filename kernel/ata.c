#include <stdbool.h>
#include "pci.h"
#include "alloc.h"
#include "io.h"
#include "../drivers/text.h"
#include "ata.h"

#define ATA_BUS_PRIMARY 0x1F0
#define ATA_BUS_SECONDARY 0x170

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

#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_READ_SECTORS 0x20

bool detect_float(uint16_t bus) {
	return inb(bus + ATA_REG_STAT) == 0xFF;
}

enum ata_res ata_read(struct ata_dev dev, uint32_t lba, uint8_t sector_count, uint16_t *dest) {
	if (!dev.lba28) {
		return ATA_ERR_NO_SUPPORTED_MODE;
	}
	outb(dev.bus + ATA_REG_DRIVE, dev.drive | ((lba >> 24) & 0xf) | 0xE0);
	outb(dev.bus + ATA_REG_SECT_CNT, sector_count);
	outb(dev.bus + ATA_REG_LBA_LO, lba);
	outb(dev.bus + ATA_REG_LBA_MID, lba >> 8);
	outb(dev.bus + ATA_REG_LBA_HI, lba >> 16);
	outb(dev.bus + ATA_REG_CMD, ATA_CMD_READ_SECTORS);
	for (uint32_t i = 0; i < sector_count; i++) {
		for (;;) {
			uint8_t status = inb(dev.bus + ATA_REG_STAT);
			if (status & 0x80) {
				continue;
			}
			if ((status & 0x01) || (status & 0x20)) {
				return ATA_ERR_IO_FAILED;
			}
			if (status & 0x40) {
				break;
			}
		}
		for (uint32_t j = 0; j < 256; j++) {
			dest[j + (i * 512)] = inw(dev.bus + ATA_REG_DATA);
		}
		// 200ns delay to allow new DRQ/BSY to be pushed onto bus
		for (uint32_t j = 0; j < 15; j++) {
			inb(dev.bus + ATA_REG_STAT);
		}
	}
	return ATA_SUCCESS;
}

struct ata_dev detect_drive(uint16_t bus, uint8_t drive) {
	struct ata_dev dev;
	dev.bus = bus;
	dev.drive = drive;
	dev.present = false;
	outb(bus + ATA_REG_DRIVE, drive);
	for (uint16_t port = bus + ATA_REG_SECT_CNT; port <= bus + ATA_REG_LBA_HI; port++) {
		outb(port, 0);
	}
	outb(bus + ATA_REG_CMD, ATA_CMD_IDENTIFY);
	if (inb(bus + ATA_REG_STAT) == 0) {
		return dev;
	}
	while ((inb(bus + ATA_REG_STAT) & 0x80) > 0) {}
	if (inb(bus + ATA_REG_LBA_MID) || inb(bus + ATA_REG_LBA_HI)) {
		return dev;
	}
	while ((inb(bus + ATA_REG_STAT) & 0x8) == 0 && (inb(bus + ATA_REG_STAT) & 0x1) == 0) {}
	if ((inb(bus + ATA_REG_STAT) & 0x1) > 0) {
		return dev;
	}
	dev.present = true;
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
	detect_bus(ATA_BUS_PRIMARY);
	detect_bus(ATA_BUS_SECONDARY);
}

void register_ata() {
	struct pci_handler *handler = (struct pci_handler*)kmalloc(sizeof(struct pci_handler));
	// Initialize ATA when the IDE controller is found during PCI enumeration
	handler->class_code = 0x1; // Mass storage controller
	handler->subclass_code = 0x1; // IDE controller
	handler->init = init_ata;
	pci_register_handler(handler);
}
