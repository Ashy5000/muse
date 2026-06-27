const multiboot = @import("multiboot.zig");
const global = @import("global.zig");
const paging = @import("arch/x86/paging.zig");
const modules = @import("modules.zig");

pub fn init() modules.ModuleInitError!void {
    const tag = multiboot.multibootFindTag(multiboot.MultibootTagMmap) catch return error.ModuleUnsupported;
    var entry: *align(1) multiboot.MultibootMmapEntry = &tag.first_entry;
    var idx: usize = 0;
    while (@intFromPtr(entry) < @intFromPtr(tag) + tag.size and idx < global.info.regions.len) {
        if (entry.type != .available) {
            entry = @ptrFromInt(@intFromPtr(entry) + tag.entry_size);
            continue;
        }
        global.info.regions[idx] = .{
            .start = @intCast(entry.addr),
            .pg_cnt = @intCast(entry.len / paging.page_size),
            .bitmap = null,
        };
        entry = @ptrFromInt(@intFromPtr(entry) + tag.entry_size);
        idx += 1;
    }
    global.info.region_cnt = idx;
}

pub var mod: modules.Module = .{
    .name = "mmap",
    .deps = &@as([1]*modules.Module, .{&multiboot.mod}),
    .init = init,
};
