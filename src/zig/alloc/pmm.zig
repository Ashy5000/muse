const global = @import("../global.zig");
const console = @import("../console.zig");
const paging = @import("../arch/x86/paging.zig");
const mmap = @import("../mmap.zig");
const virtual = @import("../virtual.zig");
const modules = @import("../modules.zig");
const bitmaps = @import("../utils/bitmaps.zig");

pub const PMMError = error{
    PMMNoMem,
    PMMUninit,
};

pub fn pmmAlloc() PMMError!usize {
    for (0..global.info.region_cnt) |i| {
        const idx: usize = bitmaps.bitmapAlloc(global.info.regions[i].bitmap orelse return error.PMMUninit) orelse continue;
        return global.info.regions[i].start + paging.page_size * idx;
    }
    return error.PMMNoMem;
}

pub fn pmmSetStatus(addr: usize, status: bool) PMMError!void {
    for (0..global.info.region_cnt) |i| {
        const start: usize = global.info.regions[i].start;
        const end: usize = start + global.info.regions[i].pg_cnt * paging.page_size;
        if (addr >= start and addr < end) {
            const idx: usize = (addr - start) / paging.page_size;
            bitmaps.bitmapSet(global.info.regions[i].bitmap orelse return error.PMMUninit, idx, status);
            return;
        }
    }
    return error.PMMNoMem;
}

// "Resource deallocation must succeed."
// - zig zen
pub fn pmmFree(addr: usize) void {
    pmmSetStatus(addr, false) catch return;
}

var global_vr: ?virtual.Vregion = null;

fn init() modules.ModuleInitError!void {
    // TODO: Don't discard page count % 8 pages.
    for (0..global.info.region_cnt) |i| {
        const bitmap_cnt: usize = global.info.regions[i].pg_cnt / @bitSizeOf(bitmaps.BitmapUnit);
        const bitmap_ptr: [*]bitmaps.BitmapUnit = @ptrFromInt(@intFromPtr(global.info) + global.info.size);
        const bitmap: []bitmaps.BitmapUnit = bitmap_ptr[0..bitmap_cnt];
        global.info.size += bitmap.len * @sizeOf(bitmaps.BitmapUnit);
        @memset(bitmap, 0);
        global.info.regions[i].bitmap = bitmap;
    }
    global_vr = .{
        .vaddr = @intFromPtr(global.info),
        .paddr = @intFromPtr(global.info),
        .pg_cnt = (global.info.size + paging.page_size - 1) / paging.page_size,
    };
    paging.registerRegion(&global_vr.?);
    var addr: usize = @intFromPtr(global.info);
    while (addr < @intFromPtr(global.info) + global.info.size) : (addr += paging.page_size) {
        pmmSetStatus(addr, true) catch return error.ModuleInitFailure;
    }
}

pub var mod: modules.Module = .{
    .name = "pmm",
    .init = init,
    .deps = @as([2]*modules.Module, .{ &mmap.mod, &global.mod })[0..],
};
