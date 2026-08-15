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
    vaddr: [*]allowzero align(paging.page_size) u8,
    paddr: [*]allowzero align(paging.page_size) u8,
    pg_cnt: usize,
    next: ?*Vregion = null,
    flags: Vflags = .{},
};

pub const BackError = pmm.AllocError || paging.MapError;

pub fn backSlice(s: []u8) BackError!void {
    const start: usize = @intFromPtr(s.ptr);
    var addr: usize = std.mem.Alignment.backward(paging.page_align, start);
    while (addr < start + s.len) : (addr += paging.page_size) {
        if (!paging.getPageStatus(@ptrFromInt(addr))) {
            try paging.mapPage(
                @ptrFromInt(addr),
                try pmm.pmmAlloc(paging.page_size),
                .@"4k",
                .{},
            );
        }
    }
}

pub const MapError = frames.FrameAllocError || paging.MapError;

pub fn mapPhysObj(s: []u8, flags: Vflags) MapError![]u8 {
    const phys_start: usize = std.mem.Alignment.backward(
        paging.page_align,
        @intFromPtr(s.ptr),
    );
    const phys_end: usize = std.mem.Alignment.forward(
        paging.page_align,
        @intFromPtr(s.ptr) + s.len,
    );
    const pg_cnt: usize = (phys_end - phys_start) / paging.page_size;
    const virt_start = (try frames.frameAllocContig(pg_cnt)).ptr;
    const region: Vregion = .{
        .vaddr = virt_start,
        .paddr = @ptrFromInt(phys_start),
        .pg_cnt = pg_cnt,
        .flags = flags,
    };
    try paging.mapRegion(&region);
    const offset = @intFromPtr(s.ptr) - phys_start;
    const res_ptr: [*]u8 = virt_start + offset;
    return res_ptr[0..s.len];
}

pub fn freeMappedObj(s: []u8) void {
    const virt_start: usize = std.mem.Alignment.backward(paging.page_align, @intFromPtr(s.ptr));
    const virt_end: usize = std.mem.Alignment.forward(paging.page_align, @intFromPtr(s.ptr) + s.len);
    var addr: usize = virt_start;
    while (addr < virt_end) : (addr += paging.page_size) {
        frames.frameFree(addr);
    }
}

pub var mod: modules.Module = .{
    .name = "virtual",
    .deps = &.{ &pmm.mod, &paging.mod, &frames.mod },
};
