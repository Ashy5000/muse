const std = @import("std");
const vga = @import("drivers/video/vga.zig");
const modules = @import("modules.zig");

/// A type capable of representing a color.
pub const Color = u24;

const displays: [1]*Display = .{&vga.display_vga};

/// An error produced while drawing to a display.
pub const DisplayDrawError = vga.VGADrawError;

/// A display output that supports initialization, pixel plotting, and
/// scrolling.
pub const Display = struct {
    width: usize,
    height: usize,
    init: *const fn () bool,
    putPixel: *const fn (x: usize, y: usize, c: Color) DisplayDrawError!void,
    scrollGrid: *const fn (inc: usize) DisplayDrawError!void,
};

/// The primary system display, used for the system console.
pub var display_primary: ?*Display = null;

fn init() modules.ModuleInitError!void {
    for (displays) |d| {
        if (d.init()) {
            display_primary = d;
            return;
        }
    }
    return error.ModuleUnsupported;
}

/// The display module, which initializes the subsystem for video output.
pub var mod: modules.Module = .{
    .name = "display",
    .init = init,
    .deps = &.{ &@import("multiboot.zig").mod, &@import("virtual.zig").mod },
};
