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

fn init() modules.InitError!*allowzero align(paging.page_size) CoreInfo {
    const info: *allowzero align(paging.page_size) CoreInfo = @ptrFromInt(0);
    info.size = @sizeOf(CoreInfo);
    return info;
}

pub var mod: modules.Module(*allowzero align(paging.page_size) CoreInfo) = .{
    .name = "global",
    .init = init,
};
