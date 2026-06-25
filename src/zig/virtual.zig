const paging = @import("arch.zig").paging;
const pmm = @import("pmm.zig");
const modules = @import("modules.zig");

pub const Vregion = struct {
    vaddr: usize,
    paddr: usize,
    pg_cnt: usize,
    next: ?*Vregion = null,
    flags: paging.pageTableEntryFlags = .{},
};

pub fn backSlice(s: []u8) pmm.PMMError!void {
    const start: usize = @intFromPtr(s.ptr);
    var addr: usize = start - (start % paging.page_size);
    while (addr < start + s.len) : (addr += paging.page_size) {
        const info: *paging.pageTableEntry = paging.getPageInfo(addr);
        if (!info.flags.present) {
            try paging.mapPage(addr, try pmm.pmmAlloc(), .{});
        }
    }
}

const mod: modules.Module = .{
    .name = "virtual",
    .deps = &@as([2]modules.Module, .{ &pmm.mod, &paging.mod }),
};
