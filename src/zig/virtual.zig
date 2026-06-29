const std = @import("std");
const paging = @import("arch.zig").paging;
const pmm = @import("alloc/pmm.zig");
const frames = @import("alloc/frames.zig");
const modules = @import("modules.zig");

pub const PageCacheMode = enum {
    Uncacheable,
    WriteCombining,
    Writethrough,
    WriteProtect,
    Writeback,
    Uncached,
};

pub const Vflags = struct {
    cache_mode: PageCacheMode = .Writeback,
    user: bool = false,
    writeable: bool = true,
};

pub const Vregion = struct {
    vaddr: usize,
    paddr: usize,
    pg_cnt: usize,
    next: ?*Vregion = null,
    flags: Vflags = .{},
};

pub fn backSlice(s: []u8, flags: Vflags) pmm.PMMError!void {
    const start: usize = @intFromPtr(s.ptr);
    var addr: usize = std.mem.Alignment.backward(paging.page_align, start);
    while (addr < start + s.len) : (addr += paging.page_size) {
        const info: *paging.pageTableEntry = paging.getPageInfo(addr) orelse {
            try paging.mapPage(addr, try pmm.pmmAlloc(), flags);
            continue;
        };
        if (!info.flags.present) {
            try paging.mapPage(addr, try pmm.pmmAlloc(), flags);
        }
    }
}

const MapError = pmm.PMMError || frames.FrameAllocError;

pub fn mapPhysObj(s: []u8) MapError![]u8 {
    const phys_start: usize = std.mem.Alignment.backward(paging.page_align, @intFromPtr(s.ptr));
    const phys_end: usize = std.mem.Alignment.forward(paging.page_align, @intFromPtr(s.ptr) + s.len);
    const pg_cnt: usize = (phys_end - phys_start) / paging.page_size;
    const virt_start: usize = @intFromPtr(try frames.frameAllocContig(pg_cnt));
    const region: Vregion = .{
        .vaddr = virt_start,
        .paddr = phys_start,
        .pg_cnt = pg_cnt,
    };
    try paging.mapRegion(&region);
    const offset = @intFromPtr(s.ptr) - phys_start;
    const res_ptr: [*]u8 = @ptrFromInt(virt_start + offset);
    return res_ptr[0..s.len];
}

pub fn unmapPhysObj(s: []u8) void {
    const virt_start: usize = std.mem.Alignment.backward(paging.page_align, @intFromPtr(s.ptr));
    const virt_end: usize = std.mem.Alignment.forward(paging.page_align, @intFromPtr(s.ptr) + s.len);
    var addr: usize = virt_start;
    while (addr < virt_end) : (addr += paging.page_size) {
        frames.frameFree(@ptrFromInt(addr));
    }
}

pub var mod: modules.Module = .{
    .name = "virtual",
    .deps = &.{ &pmm.mod, &paging.mod, &frames.mod },
};
