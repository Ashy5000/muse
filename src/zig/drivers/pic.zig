const modules = @import("../modules.zig");
const io = @import("../utils/io.zig");

fn init() modules.ModuleInitError!void {
    const pic1: io.Port = 0x20;
    const pic2: io.Port = 0xA0;
    const data_offset: io.Port = 0x1;
    io.out(8, pic1 + data_offset, 0xFF);
    io.out(8, pic2 + data_offset, 0xFF);
}

pub var mod: modules.Module = .{
    .name = "PIC",
    .init = init,
};
