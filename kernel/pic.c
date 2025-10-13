#include "pic.h"
#include "io.h"

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_CMD PIC1
#define PIC1_DATA (PIC1 + 1)
#define PIC2_CMD PIC2
#define PIC2_DATA (PIC2 + 1)

#define ICW1_ICW4 0x01
#define ICW1_SINGLE 0x02
#define ICW1_INTERVAL4 0x04
#define ICW1_LEVEL 0x08
#define ICW1_INIT 0x10

#define ICW4_8086 0x01
#define ICW4_AUTO 0x02
#define ICW4_BUF_SLAVE 0x08
#define ICW4_BUF_MASTER 0x0C

#define CASCADE_IRQ 2

#define PIC_MASTER_OFFSET 0x20
#define PIC_SLAVE_OFFSET 0x28

void init_pic(void) {
	outb(PIC1_CMD, ICW1_INIT | ICW1_ICW4); // Start init sequence
	io_wait(); // Necessary on older machines
	outb(PIC2_CMD, ICW1_INIT | ICW1_ICW4);
	io_wait();
	outb(PIC1_DATA, PIC_MASTER_OFFSET);
	io_wait();
	outb(PIC2_DATA, PIC_SLAVE_OFFSET);
	io_wait();
	outb(PIC1_DATA, 1 << CASCADE_IRQ); // Tells master that slave can be triggered via IRQ2
	io_wait();
	outb(PIC2_DATA, 2); // Cascade identity (?)
				       // Perhaps this refers to the IRQ #?
	io_wait();
	outb(PIC1_DATA, ICW4_8086); // Set mode to 8086 instead of 8080
	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();
	outb(PIC1_DATA, 0xFD);
	outb(PIC2_DATA, 0xFF);
	
}
