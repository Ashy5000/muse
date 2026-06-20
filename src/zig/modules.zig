const console = @import("console.zig");

pub const ModuleInitError = error{} || @import("console.zig").ConsoleInitError || @import("psf.zig").PSFInitError || @import("display.zig").DisplayInitError || @import("multiboot.zig").MultibootInitError || @import("mmap.zig").MmapInitError;

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
    console.print("-Loading module `{s}`.\n", .{m.name});
    if (m.inited) {
        return;
    }
    for (m.deps) |module| {
        depth += 1;
        loadModule(module) catch |err| {
            for (0..depth) |_| {
                console.print(" |", .{});
            }
            console.print("ERROR: {s}\n", .{@errorName(err)});
        };
        depth -= 1;
    }
    try m.init();
    m.inited = true;
}

pub var mod: Module = .{
    .name = "modules",
    .deps = @as([1]*Module, .{&console.mod})[0..],
};
