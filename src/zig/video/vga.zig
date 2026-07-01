const multiboot = @import("../multiboot.zig");
const display = @import("../display.zig");
const virtual = @import("../virtual.zig");
const paging = @import("../arch/x86/paging.zig");

const Framebuffer = struct {
    bfr: [*]u8,
    width: usize,
    height: usize,
    pitch: usize,
};

var framebuffer: ?Framebuffer = null;

fn init() bool {
    const vga_tag = multiboot.multibootFindTag(multiboot.MultibootTagFramebuffer) catch return false;
    var fb: Framebuffer = .{
        .bfr = @ptrFromInt(@as(usize, @intCast(vga_tag.addr))),
        .width = vga_tag.width,
        .height = vga_tag.height,
        .pitch = vga_tag.pitch,
    };
    display_vga.width = fb.width;
    display_vga.height = fb.height;
    //todo
    fb.bfr = (virtual.mapPhysObj(fb.bfr[0 .. fb.pitch * fb.height]) catch return false).ptr;
    framebuffer = fb;
    fillRect(0, 0, fb.width, fb.height, 0x000000) catch return false;
    return true;
}

pub const VGADrawError = error{
    VGAUninit,
};

pub fn putPixel(x: usize, y: usize, c: display.Color) display.DisplayDrawError!void {
    const fb: Framebuffer = framebuffer orelse return error.VGAUninit;
    const offset = y * fb.pitch + x * @bitSizeOf(display.Color) / 8;
    fb.bfr[offset] = @truncate(c);
    fb.bfr[offset + 1] = @truncate(c >> 8);
    fb.bfr[offset + 2] = @truncate(c >> 16);
}

fn fillRect(x: usize, y: usize, w: usize, h: usize, c: display.Color) VGADrawError!void {
    const fb: Framebuffer = framebuffer orelse return error.VGAUninit;
    const pixel_size = @bitSizeOf(display.Color) / 8;
    var y_p = y;
    var line_offset = y * fb.pitch;
    while (y_p < y + h) : (y_p += 1) {
        var x_p = x;
        var px_offset = x_p * pixel_size;
        while (x_p < x + w) : (x_p += 1) {
            fb.bfr[line_offset + px_offset] = @truncate(c);
            fb.bfr[line_offset + px_offset + 1] = @truncate(c >> 8);
            fb.bfr[line_offset + px_offset + 2] = @truncate(c >> 16);
            px_offset += pixel_size;
        }
        line_offset += fb.pitch;
    }
}

pub fn scrollGrid(inc: usize) display.DisplayDrawError!void {
    const fb: Framebuffer = framebuffer orelse return error.VGAUninit;
    const top_aligned: usize = fb.height - (fb.height % inc);
    @memmove(fb.bfr, fb.bfr[fb.pitch * inc .. fb.pitch * top_aligned]);
    @memset(fb.bfr[fb.pitch * (top_aligned - inc) .. fb.pitch * fb.height], 0);
}

pub var display_vga: display.Display = .{
    .width = 0,
    .height = 0,
    .init = init,
    .putPixel = putPixel,
    .scrollGrid = scrollGrid,
};
