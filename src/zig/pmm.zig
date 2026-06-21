const global = @import("global.zig");
const paging = @import("arch/x86/paging.zig");
const mmap = @import("mmap.zig");
const modules = @import("modules.zig");

pub const bitmapUnit = u8;

pub const PMMError = error{
    PMMNoMem,
    PMMUninit,
};

pub fn pmm_alloc() PMMError!usize {
    for (0..global.info.region_cnt) |i| {
        var addr: usize = global.info.regions[i].start;
        const bitmap_cnt: usize = global.info.regions[i].pg_cnt / @bitSizeOf(bitmapUnit);
        var bitmap: [*]bitmapUnit = global.info.regions[i].bitmap orelse return error.PMMUninit;

        for (0..bitmap_cnt) |j| {
            for (0..@bitSizeOf(bitmapUnit)) |k| {
                if (((bitmap[j] >> @intCast(k)) & 1) == 0) {
                    bitmap[j] |= @as(bitmapUnit, 1) << @intCast(k);
                    return addr;
                }
                addr += paging.page_size;
            }
        }
    }
    return error.PMMNoMem;
}

pub fn pmm_set_status(addr: usize, free: bool) PMMError!void {
    for (0..global.info.region_cnt) |i| {
        const start: usize = global.info.regions[i].start;
        const end: usize = start + global.info.regions[i].pg_cnt * paging.page_size;
        if (addr >= start and addr < end) {
            const offset: usize = addr - start;
            const bitmap_idx: usize = offset / @bitSizeOf(bitmapUnit);
            const bit_idx: usize = offset % @bitSizeOf(bitmapUnit);
            var bitmap: [*]bitmapUnit = global.info.regions[i].bitmap orelse return error.PMMUninit;
            if (free) {
                bitmap[bitmap_idx] &= ~(@as(bitmapUnit, 1) << @intCast(bit_idx));
            } else {
                bitmap[bitmap_idx] |= @as(bitmapUnit, 1) << @intCast(bit_idx);
            }
            return;
        }
    }
    return error.PMMNoMem;
}

// "Resource deallocation must succeed."
// - zig zen
pub fn pmm_free(addr: usize) void {
    pmm_set_status(addr, true) catch return;
}

fn init() modules.ModuleInitError!void {
    for (0..global.info.region_cnt) |i| {
        const bitmap_cnt: usize = global.info.regions[i].pg_cnt / @bitSizeOf(bitmapUnit);
        const bitmap: [*]bitmapUnit = @ptrFromInt(@intFromPtr(global.info) + global.info.size);
        global.info.size += bitmap_cnt * @sizeOf(bitmapUnit);
        @memset(bitmap[0..bitmap_cnt], 0);
        global.info.regions[i].bitmap = bitmap;
    }
    var addr: usize = @intFromPtr(global.info);
    while (addr < @intFromPtr(global.info) + global.info.size) : (addr += paging.page_size) {
        try pmm_set_status(addr, false);
    }
}

pub var mod: modules.Module = .{
    .name = "pmm",
    .init = init,
    .deps = @as([2]*modules.Module, .{ &mmap.mod, &global.mod })[0..],
};
