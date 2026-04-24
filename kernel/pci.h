#ifndef PCI_H
#define PCI_H

#include <stdint.h>

struct pci_func {
	uint16_t vendor;
	uint8_t class_code;
	uint8_t subclass_code;
	uint8_t prog_if;
	uint8_t type;
	bool multi_function;
};

struct pci_dev {
	uint8_t bus;
	uint8_t slot;
	uint8_t func_count;
	struct pci_func funcs[8];
	bool present;
};

struct pci_bus {
	struct pci_dev devs[32];
};

typedef void (*pci_fn_init_t)(struct pci_func);

struct pci_handler {
	uint8_t class_code;
	uint8_t subclass_code;
	pci_fn_init_t init;
	struct pci_handler *next;
};

void pci_register_handler(struct pci_handler *handler);

struct pci_func scan_pci_func(uint8_t bus, uint8_t slot, uint8_t func);
struct pci_dev scan_pci_dev(uint8_t bus, uint8_t slot);
struct pci_bus scan_pci_bus(uint8_t bus);

void init_pci();

#endif
