const std = @import("std");
const vga = @import("video/vga.zig");
const modules = @import("modules.zig");

pub const Color = u24;

const displays: [1]*Display = .{&vga.display_vga};

pub const DisplayDrawError = vga.VGADrawError;

pub const Display = struct {
    width: usize,
    height: usize,
    init: *const fn () bool,
    putPixel: *const fn (x: usize, y: usize, c: Color) DisplayDrawError!void,
    scrollGrid: *const fn (inc: usize) DisplayDrawError!void,
};

pub var display_primary: ?*Display = null;

fn init() modules.ModuleInitError!void {
    for (displays) |d| {
        if (d.init()) {
            display_primary = d;
            return;
        }
    }
    return error.ModuleInitFailure;
}

pub var mod: modules.Module = .{
    .name = "display",
    .init = init,
    .deps = &@as([1]*modules.Module, .{&@import("multiboot.zig").mod}),
};
