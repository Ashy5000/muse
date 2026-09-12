const multiboot = @import("multiboot.zig");
const global = @import("global.zig");
const paging = @import("arch.zig").paging;
const modules = @import("modules.zig");

pub fn init() modules.InitError!*allowzero align(paging.page_size) global.CoreInfo {
    const tag = multiboot.multibootFindTag(
        multiboot.MultibootTagMmap,
    ) catch return error.Unsupported;
    var entry: *align(1) multiboot.MultibootMmapEntry = &tag.first_entry;
    var idx: usize = 0;
    const info = try global.mod.data();
    while (@intFromPtr(entry) < @intFromPtr(tag) + tag.size and idx < info.regions.len) {
        if (entry.type != .available) {
            entry = @ptrFromInt(@intFromPtr(entry) + tag.entry_size);
            continue;
        }
        const start: usize = paging.page_align.forward(entry.addr);
        info.regions[idx] = .{
            .start = @ptrFromInt(start),
            .pg_cnt = @intCast(
                (entry.addr + entry.len - start) / paging.page_size,
            ),
            .bitmaps = null,
        };
        entry = @ptrFromInt(@intFromPtr(entry) + tag.entry_size);
        idx += 1;
    }
    info.region_cnt = idx;
    return info;
}

pub var mod: modules.Module(
    *allowzero align(paging.page_size) global.CoreInfo,
) = .{
    .name = "mmap",
    .init = init,
};
