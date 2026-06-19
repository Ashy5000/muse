#include <muse/io.h>
#include <muse/logging.h>
#include <muse/ps2.h>
#include <stdbool.h>

void ps2_send_0(uint8_t d) {
	PS2_POLL_WRITE;
	outb(PS2_DATA, d);
}

void ps2_send_1(uint8_t d) {
	outb(PS2_CMD, 0xD4);
	PS2_POLL_WRITE;
	outb(PS2_DATA, d);
}

void init_ps2() {
	/* Disable devices */
	outb(PS2_CMD, PS2_CMD_DISABLE_0);
	outb(PS2_CMD, PS2_CMD_DISABLE_1);

	/* Disable interrupts (temporarily) and enable clock */
	outb(PS2_CMD, PS2_CMD_CONF_R);
	PS2_POLL_READ;
	uint8_t conf = inb(PS2_DATA);
	conf &= ~(PS2_INT_0 | PS2_INT_1 | PS2_CLK_0);
	outb(PS2_CMD, PS2_CMD_CONF_W);
	PS2_POLL_WRITE;
	outb(PS2_DATA, conf);

	/* Self test */
	outb(PS2_CMD, PS2_CMD_CHK);
	PS2_POLL_READ;
	if (inb(PS2_DATA) != 0x55) {
		log(LOG_ERROR, LOG_PS2, "PS/2 self test failed!\n");
		__asm__ volatile("cli; hlt");
		return;
	}

	/* Restore the configuration byte (the self test might reset the 8042)
	 */
	outb(PS2_CMD, PS2_CMD_CONF_W);
	PS2_POLL_WRITE;
	outb(PS2_DATA, conf);

	/* Check if the controller is dual-channel */
	outb(PS2_CMD, PS2_CMD_ENABLE_0);
	outb(PS2_CMD, PS2_CMD_ENABLE_1);
	bool dual_channel = true;
	outb(PS2_CMD, PS2_CMD_CONF_R);
	PS2_POLL_READ;
	if (inb(PS2_DATA) & PS2_CLK_1) {
		dual_channel = false;
	}
	log(LOG_INFO, LOG_PS2, "PS/2 controller is %s-channel.\n",
	    dual_channel ? "dual" : "mono");

	/* Enable interrupts */
	conf |= PS2_INT_0;
	if (dual_channel) {
		conf |= PS2_INT_1;
	}
	outb(PS2_CMD, PS2_CMD_CONF_W);
	PS2_POLL_WRITE;
	outb(PS2_DATA, conf);

	ps2_send_0(0xFF);
	PS2_POLL_READ;
	uint8_t a = inb(PS2_DATA);
	PS2_POLL_READ;
	uint8_t b = inb(PS2_DATA);
	if ((a != 0xFA || b != 0xAA) && (a != 0xAA || b != 0xFA)) {
		log(LOG_ERROR, LOG_PS2,
		    "Resetting first PS/2 device failed!\n");
	}
}
