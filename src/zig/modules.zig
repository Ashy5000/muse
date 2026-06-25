const console = @import("console.zig");

pub const ModuleInitError = error{} || @import("console.zig").ConsoleInitError || @import("psf.zig").PSFInitError || @import("display.zig").DisplayInitError || @import("multiboot.zig").MultibootInitError || @import("mmap.zig").MmapInitError || @import("pmm.zig").PMMError || @import("elf.zig").ELFInitError;

fn initEmpty() !void {}

pub const Module = struct {
    name: []const u8,
    inited: bool = false,
    deps: []const *Module = @as([0]*Module, .{})[0..],
    init: *const fn () ModuleInitError!void = initEmpty,
};

var depth: u32 = 0;

pub fn loadModule(m: *Module) ModuleInitError!void {
    for (0..depth) |_| {
        console.print(" |", .{});
    }
    if (m.inited) {
        console.print("-`{s}` (skipped)\n", .{m.name});
        return;
    }
    console.print("-`{s}`\n", .{m.name});
    for (m.deps) |module| {
        depth += 1;
        try loadModule(module);
        depth -= 1;
    }
    m.init() catch |err| {
        for (0..depth) |_| {
            console.print(" |", .{});
        }
        console.print("ERROR: {s}\n", .{@errorName(err)});
        return err;
    };
    m.inited = true;
}

pub var mod: Module = .{
    .name = "modules",
    .deps = @as([1]*Module, .{&console.mod})[0..],
};
