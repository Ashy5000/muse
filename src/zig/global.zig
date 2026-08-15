const pmm = @import("alloc/pmm.zig");
const modules = @import("modules.zig");
const bitmaps = @import("utils/bitmaps.zig");
const paging = @import("arch.zig").paging;

pub const Region = struct {
    start: usize,
    pg_cnt: usize,
    bitmaps: ?[pmm.tier_cnt][]bitmaps.BitmapUnit,
};

pub const CoreInfo = struct {
    regions: [16]Region,
    region_cnt: usize,
    size: usize,
};

pub var info: *allowzero align(paging.page_size) CoreInfo = @ptrFromInt(0);

fn init() modules.ModuleInitError!void {
    info.size = @sizeOf(CoreInfo);
}

pub var mod: modules.Module = .{
    .name = "global",
    .init = init,
};
