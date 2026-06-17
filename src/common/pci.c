#include <muse/alloc.h>
#include <muse/io.h>
#include <muse/logging.h>
#include <muse/pci.h>
#include <stdbool.h>

#define CONFIG_ADDR 0xCF8
#define CONFIG_DATA 0xCFC

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func,
                         uint8_t offset) {
	uint32_t addr = (1 << 31) | ((uint32_t)bus << 16) |
	                ((uint32_t)slot << 11) | ((uint32_t)func << 8) | offset;
	outl(CONFIG_ADDR, addr);
	return inl(CONFIG_DATA);
}

#define get_vendor_id(B, S, F) pci_config_read(B, S, F, 0x0);

#define get_device_id(B, S, F) (pci_config_read(B, S, F, 0x0) >> 16);

#define get_header_type(B, S, F) (pci_config_read(B, S, F, 0xC) >> 16)

#define get_class(B, S, F) (pci_config_read(B, S, F, 0x8) >> 24)

#define get_subclass(B, S, F) (pci_config_read(bus, slot, func, 0x8) >> 16)

#define get_prog_if(B, S, F) (pci_config_read(B, S, F, 0x8) >> 8)

#define get_revision(B, S, F) pci_config_read(B, S, F, 0x8)

#define get_secondary_bus(B, S, F) (pci_config_read(B, S, F, 0x18) >> 18)

struct pci_bar get_bar(uint8_t bus, uint8_t slot, uint8_t func, uint8_t idx) {
	struct pci_bar bar;
	uint32_t reg =
	    (pci_config_read(bus, slot, func, 0x10 + (idx * sizeof(uint32_t))));
	if (reg & 0x1) {
		// I/O space
		bar.type = PCI_BAR_IO;
		bar.addr = reg & 0xFFFFFFFC;
	} else {
		// Memory space
		bar.type = PCI_BAR_MEM;
		bar.addr = reg & 0xFFFFFFF0;
	}
	return bar;
}

struct pci_handler *first_handler = 0;
struct pci_handler *last_handler  = 0;

void pci_register_handler(struct pci_handler *handler) {
	handler->next = 0;
	if (last_handler) {
		last_handler->next = handler;
		last_handler       = handler;
	} else {
		first_handler = handler;
		last_handler  = handler;
	}
}

void init_function(struct pci_func fn) {
	struct pci_handler *handler = first_handler;
	while (handler) {
		if (handler->class_code == fn.class_code &&
		    handler->subclass_code == fn.subclass_code) {
			handler->init(fn);
		}
		handler = handler->next;
	}
}

struct pci_func scan_pci_func(uint8_t bus, uint8_t slot, uint8_t func) {
	struct pci_func res;
	uint16_t vendor = get_vendor_id(bus, slot, func);
	res.vendor      = vendor;
	if (vendor == 0xFFFF) {
		return res;
	}
	res.class_code     = get_class(bus, slot, func);
	res.subclass_code  = get_subclass(bus, slot, func);
	res.prog_if        = get_prog_if(bus, slot, func);
	res.type           = get_header_type(bus, slot, func);
	res.multi_function = false;
	if ((res.type & 0x80) != 0) {
		res.multi_function = true;
		res.type &= ~0x80;
	}
	if (res.type == 0x0) {
		for (uint32_t i = 0; i < 6; i++) {
			res.bars[i] = get_bar(bus, slot, func, i);
		}
	}
	if (res.type == 0x1) { // PCI-PCI bridge
		uint8_t secondary_bus = get_secondary_bus(bus, slot, func);
		log(LOG_INFO, LOG_PCI, "Found PCI-PCI bridge to bus %x.\n",
		    (uint32_t)secondary_bus);
		scan_pci_bus(secondary_bus);
		for (uint32_t i = 0; i < 2; i++) {
			res.bars[i] = get_bar(bus, slot, func, i);
		}
	}
	log(LOG_INFO, LOG_PCI,
	    "Found function with vendor %x and class %x:%x.\n",
	    (uint32_t)res.vendor, (uint32_t)res.class_code,
	    (uint32_t)res.subclass_code);
	init_function(res);
	return res;
}

struct pci_dev *scan_pci_dev(uint8_t bus, uint8_t slot) {
	struct pci_dev *dev  = kmalloc(sizeof(*dev));
	dev->bus             = bus;
	dev->slot            = slot;
	struct pci_func func = scan_pci_func(bus, slot, 0);
	if (func.vendor == 0xFFFF) {
		kfree(dev);
		return 0;
	}
	dev->present    = true;
	dev->func_count = 1;
	dev->funcs[0]   = func;
	if (func.multi_function) {
		for (uint8_t i = 1; i < 8; i++) {
			func = scan_pci_func(bus, slot, i);
			if (func.vendor == 0xFFFF) {
				break;
			}
			dev->func_count++;
			dev->funcs[i] = func;
		}
	}
	return dev;
}

struct pci_bus scan_pci_bus(uint8_t bus) {
	struct pci_bus res;
	for (uint8_t i = 0; i < 32; i++) {
		log(LOG_DEBUG, LOG_PCI, "Scanning PCI bus %x.\n", i);
		res.devs[i] = scan_pci_dev(bus, i);
	}
	return res;
}

void init_pci() {
	struct pci_dev *host_bridge = scan_pci_dev(0, 0);

	for (uint8_t i = 0; i < host_bridge->func_count; i++) {
		scan_pci_bus(i);
	}
}
