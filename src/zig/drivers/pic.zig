const io = @import("../utils/io.zig");

const pic1: io.Port = 0x20;
const pic2: io.Port = 0xA0;
const data_offset: io.Port = 0x1;

pub const offset: u8 = 32;

var inited = false;

fn init() void {
    const command_init: u8 = 0x11;
    io.out(8, pic1, command_init);
    io.wait();
    io.out(8, pic2, command_init);
    io.wait();

    io.out(8, pic1 + data_offset, offset);
    io.wait();
    io.out(8, pic2 + data_offset, offset + 8);
    io.wait();

    const cascade_irq: u3 = 2;
    io.out(8, pic1 + data_offset, 1 << cascade_irq);
    io.wait();
    io.out(8, pic2 + data_offset, cascade_irq);
    io.wait();

    const mode_8086: u8 = 0x01;
    io.out(8, pic1 + data_offset, mode_8086);
    io.wait();
    io.out(8, pic2 + data_offset, mode_8086);
    io.wait();

    io.out(8, pic1 + data_offset, 0xfb);
    io.out(8, pic2 + data_offset, 0xff);

    inited = true;
}

pub fn unmask(irq: u4) void {
    if (!inited) {
        init();
    }
    if (irq < 8)
        io.out(
            8,
            pic1 + data_offset,
            io.in(
                8,
                pic1 + data_offset,
            ) & ~(@as(u8, 1) << @as(u3, @intCast(irq))),
        )
    else
        io.out(
            8,
            pic2 + data_offset,
            io.in(
                8,
                pic2 + data_offset,
            ) & ~(@as(u8, 1) << @as(u3, @intCast(irq - 8))),
        );
}

pub fn eoi(irq: u4) void {
    const command_eoi: u8 = 0x20;
    if (irq >= 8)
        io.out(8, pic2, command_eoi);
    io.out(8, pic1, command_eoi);
}
