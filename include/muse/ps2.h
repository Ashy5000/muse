#ifndef PS2_H
#define PS2_H

#include <stdint.h>

#define PS2_DATA 0x60
#define PS2_STAT 0x64
#define PS2_CMD  0x64

#define PS2_OBUF_STAT 0x1
#define PS2_IBUF_STAT 0x2
#define PS2_CMD_DATA  0x8
#define PS2_TIMEOUT   0x40
#define PS2_PARITY    0x80

#define PS2_INT_0 0x1
#define PS2_INT_1 0x2
#define PS2_SYS   0x4
#define PS2_CLK_0 0x10
#define PS2_CLK_1 0x20
#define PS2_TRNS  0x40

#define PS2_CMD_DISABLE_0 0xAD
#define PS2_CMD_DISABLE_1 0xA7
#define PS2_CMD_CONF_R    0x20
#define PS2_CMD_CONF_W    0x60
#define PS2_CMD_CHK       0xAA
#define PS2_CMD_ENABLE_0  0xAE
#define PS2_CMD_ENABLE_1  0xA8

#define PS2_POLL_WRITE                                                         \
	while (inb(PS2_STAT) & PS2_IBUF_STAT) {                                \
	}

#define PS2_POLL_READ                                                          \
	while (!(inb(PS2_STAT) & PS2_OBUF_STAT)) {                             \
	}

void ps2_send_0(uint8_t d);
void ps2_send_1(uint8_t d);
void init_ps2();

#endif
