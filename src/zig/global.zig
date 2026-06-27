const pmm = @import("alloc/pmm.zig");
const modules = @import("modules.zig");
const bitmaps = @import("utils/bitmaps.zig");

pub const Region = struct {
    start: usize,
    pg_cnt: usize,
    bitmap: ?[]bitmaps.BitmapUnit,
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
