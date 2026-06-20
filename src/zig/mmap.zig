const multiboot = @import("multiboot.zig");
const console = @import("console.zig");
const global = @import("global.zig");
const paging = @import("paging.zig");
const modules = @import("modules.zig");

pub const MmapInitError = console.ConsoleInitError || multiboot.MultibootInitError || multiboot.MultibootTagError;

pub fn init() modules.ModuleInitError!void {
    const tag = try multiboot.multibootFindTag(multiboot.MultibootTagMmap);
    var entry: *align(1) multiboot.MultibootMmapEntry = &tag.first_entry;
    var idx: usize = 0;
    while (@intFromPtr(entry) < @intFromPtr(tag) + tag.size and idx < global.info.regions.len) {
        if (entry.type == .available) {
            console.print("Found available mmap chunk from 0x{x}->0x{x}.\n", .{ entry.addr, entry.addr + entry.len });
        }
        global.info.regions[idx] = .{
            .start = @intCast(entry.addr),
            .pg_cnt = @intCast(entry.len / paging.page_size),
        };
        entry = @ptrFromInt(@intFromPtr(entry) + tag.entry_size);
        idx += 1;
    }
    global.info.region_cnt = idx;
}

pub var mod: modules.Module = .{
    .name = "mmap",
    .deps = @as([2]*modules.Module, .{ &console.mod, &multiboot.mod })[0..],
    .init = init,
};
