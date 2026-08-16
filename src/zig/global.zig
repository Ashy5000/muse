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

const info: *allowzero align(paging.page_size) CoreInfo = @ptrFromInt(0);

fn init() modules.InitError!void {
    info.size = @sizeOf(CoreInfo);
    mod.payload = info;
}

pub var mod: modules.Module = .{
    .name = "global",
    .init = init,
};
