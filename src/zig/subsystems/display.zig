const std = @import("std");
const vga = @import("../drivers/video/vga.zig");
const modules = @import("../modules.zig");
const drivers = @import("../drivers.zig");

/// A type capable of representing a color.
pub const Color = u24;

/// An error produced while drawing to a display.
pub const DrawError = vga.VGADrawError;

/// A display output that supports initialization, pixel plotting, and
/// scrolling.
pub const Driver = struct {
    width: usize,
    height: usize,
    init: *const fn () bool,
    putPixel: *const fn (x: usize, y: usize, c: Color) DrawError!void,
    scrollGrid: *const fn (inc: usize) DrawError!void,
};

fn init() modules.InitError!void {
    const Static = struct {
        var display_primary: *Driver = undefined;
    };
    for (drivers.drivers_display) |d| {
        if (d.init()) {
            Static.display_primary = d;
            mod.payload = Static.display_primary;
            return;
        }
    }
    return error.Unsupported;
}

/// The display module, which initializes the subsystem for video output.
pub var mod: modules.Module = .{
    .name = "display",
    .init = init,
};
