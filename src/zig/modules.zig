const std = @import("std");
const console = @import("console.zig");

pub const ModuleInitError = error{
    ModuleInitFailure,
    ModuleUnsupported,
    ModuleMissingConfig,
};

fn initEmpty() !void {}

pub const Module = struct {
    name: []const u8,
    inited: bool = false,
    visited: bool = false,
    deps: []const *Module = @as([0]*Module, .{})[0..],
    init: *const fn () ModuleInitError!void = initEmpty,
};

var depth: u32 = 0;

pub fn loadModule(m: *Module) ModuleInitError!void {
    if (m.inited) {
        return;
    }
    if (m.visited) {
        std.debug.panic("dependency loop", .{});
    }
    m.visited = true;
    for (0..depth) |_| {
        console.print(" |", .{});
    }
    console.print("\\`{s}`\n", .{m.name});
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
    if (m.deps.len > 0) {
        for (0..depth) |_| {
            console.print(" |", .{});
        }
        console.print("/\n", .{});
    }
}

pub var mod: Module = .{
    .name = "modules",
    .deps = @as([1]*Module, .{&console.mod})[0..],
};
