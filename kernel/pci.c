#include <stdbool.h>
#include "pci.h"
#include "io.h"
#include "../drivers/text.h"

#define CONFIG_ADDR 0xCF8
#define CONFIG_DATA 0xCFC

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
	uint32_t addr = (1 << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) | ((uint32_t)func << 8) | offset;
	outl(CONFIG_ADDR, addr);
	return inl(CONFIG_DATA);
}

uint16_t get_vendor_id(uint8_t bus, uint8_t slot, uint8_t func) {
	return pci_config_read(bus, slot, func, 0x0);
}

uint16_t get_device_id(uint8_t bus, uint8_t slot, uint8_t func) {
	return pci_config_read(bus, slot, func, 0x0) >> 16;
}

uint8_t get_header_type(uint8_t bus, uint8_t slot, uint8_t func) {
	return pci_config_read(bus, slot, func, 0xC) >> 16;
}

uint8_t get_class(uint8_t bus, uint8_t slot, uint8_t func) {
	return pci_config_read(bus, slot, func, 0x8) >> 24;
}

uint8_t get_subclass(uint8_t bus, uint8_t slot, uint8_t func) {
	return pci_config_read(bus, slot, func, 0x8) >> 16;
}

uint8_t get_revision(uint8_t bus, uint8_t slot, uint8_t func) {
	return pci_config_read(bus, slot, func, 0x8);
}

uint8_t get_secondary_bus(uint8_t bus, uint8_t slot, uint8_t func) {
	return pci_config_read(bus, slot, func, 0x18) >> 8;
}

struct pci_handler *first_handler;
struct pci_handler *last_handler;

void pci_register_handler(struct pci_handler *handler) {
	handler->next = 0;
	if (last_handler) {
		last_handler->next = handler;
		last_handler = handler;
	} else {
		first_handler = handler;
		last_handler = handler;
	}
}

void init_function(struct pci_func fn) {
	struct pci_handler *handler = first_handler;
	while (handler) {
		if (handler->class_code == fn.class_code && handler->subclass_code == fn.subclass_code) {
			handler->init(fn);
		}
		handler = handler->next;
	}
}

struct pci_func scan_pci_func(uint8_t bus, uint8_t slot, uint8_t func) {
	struct pci_func res;
	uint16_t vendor = get_vendor_id(bus, slot, func);
	if (vendor == 0xFFFF) {
		return res;
	} else {
		res.vendor = vendor;
		res.class_code = get_class(bus, slot, func);
		res.subclass_code = get_subclass(bus, slot, func);
		res.type = get_header_type(bus, slot, func);
		res.multi_function = false;
		if ((res.type & 0x80) != 0) {
			res.multi_function = true;
			res.type &= ~0x80;
		}
		if (res.type == 0x1) { // PCI-PCI bridge
			scan_pci_bus(get_secondary_bus(bus, slot, func));
		}
		kprint("Found function with vendor 0x");
		kprint_int(res.vendor, 16);
		kprint(", class 0x");
		kprint_int(res.class_code, 16);
		kprint(" and subclass 0x");
		kprint_int(res.subclass_code, 16);
		kprint(".\n");
		init_function(res);
		return res;
	}
}

struct pci_dev scan_pci_dev(uint8_t bus, uint8_t slot) {
	struct pci_dev dev;
	dev.present = false;
	dev.bus = bus;
	dev.slot = slot;
	struct pci_func func = scan_pci_func(bus, slot, 0);
	if (func.vendor == 0xFFFF) {
		return dev;
	}
	dev.present = true;
	dev.func_count = 1;
	dev.funcs[0] = func;
	if (func.multi_function) {
		for (uint8_t i = 1; i < 8; i++) {
			func = scan_pci_func(bus, slot, i);
			if (func.vendor == 0xFFFF) {
				break;
			}
			dev.func_count++;
			dev.funcs[i] = func;
		}
	}
	return dev;
}

struct pci_bus scan_pci_bus(uint8_t bus) {
	struct pci_bus res;
	for (uint8_t i = 0; i < 32; i++) {
		res.devs[i] = scan_pci_dev(bus, i);
	}
	return res;
}

void init_pci() {
	struct pci_dev host_bridge = scan_pci_dev(0, 0);

	for (uint8_t i = 0; i < host_bridge.func_count; i++) {
		scan_pci_bus(i);
	}
}
