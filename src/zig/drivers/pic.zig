const modules = @import("../modules.zig");
const io = @import("../utils/io.zig");

const pic1: io.Port = 0x20;
const pic2: io.Port = 0xA0;
const data_offset: io.Port = 0x1;

var offset: ?u8 = null;

fn init(new_offset: u8) modules.ModuleInitError!void {
    const command_init: u8 = 0x11;
    io.out(8, pic1, command_init);
    io.wait();
    io.out(8, pic2, command_init);
    io.wait();

    io.out(8, pic1 + data_offset, new_offset);
    io.wait();
    io.out(8, pic2 + data_offset, new_offset + 0x10);
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

    io.out(8, pic1 + data_offset, 0xFF);
    io.out(8, pic2 + data_offset, 0xFF);

    offset = new_offset;
}

fn unmask(irq: u8) void {
    const line: u5 = @intCast(irq - offset.?);
    if (line < 16)
        io.out(
            8,
            pic1 + data_offset,
            io.in(
                8,
                pic1 + data_offset,
            ) | (@as(u8, 1) << @as(u4, @intCast(line))),
        )
    else
        io.out(
            8,
            pic2 + data_offset,
            io.in(
                8,
                pic2 + data_offset,
            ) | (@as(u8, 1) << @as(u4, @intCast(line - 16))),
        );
}

pub var mod: modules.Module = .{
    .name = "PIC",
    .init = init,
};
