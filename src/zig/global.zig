const pmm = @import("pmm.zig");
const modules = @import("modules.zig");

pub const Region = struct {
    start: usize,
    pg_cnt: usize,
    bitmap: ?[*]pmm.bitmapUnit,
};

pub const CoreInfo = struct {
    regions: [16]Region,
    region_cnt: usize,
    size: usize,
};

pub var info: *allowzero CoreInfo = @ptrFromInt(0);

fn init() modules.ModuleInitError!void {
    info.size = @sizeOf(CoreInfo);
}

pub var mod: modules.Module = .{
    .name = "global",
    .init = init,
};
