const multiboot = @import("multiboot.zig");
const global = @import("global.zig");
const paging = @import("arch.zig").paging;
const modules = @import("modules.zig");

pub fn init() modules.InitError!void {
    const tag = multiboot.multibootFindTag(
        multiboot.MultibootTagMmap,
    ) catch return error.Unsupported;
    var entry: *align(1) multiboot.MultibootMmapEntry = &tag.first_entry;
    var idx: usize = 0;
    const info = try global.mod.data(*allowzero global.CoreInfo);
    while (@intFromPtr(entry) < @intFromPtr(tag) + tag.size and idx < info.regions.len) {
        if (entry.type != .available) {
            entry = @ptrFromInt(@intFromPtr(entry) + tag.entry_size);
            continue;
        }
        info.regions[idx] = .{
            .start = @intCast(entry.addr),
            .pg_cnt = @intCast(entry.len / paging.page_size),
            .bitmaps = null,
        };
        entry = @ptrFromInt(@intFromPtr(entry) + tag.entry_size);
        idx += 1;
    }
    info.region_cnt = idx;
    mod.payload = info;
}

pub var mod: modules.Module = .{
    .name = "mmap",
    .init = init,
};
