const multiboot = @import("multiboot.zig");

const VGAFramebuffer = struct {
    bfr: [*]u8,
    width: usize,
    height: usize,
    pitch: usize,
};

const VGAInitError = multiboot.MultibootTagError || multiboot.MultibootInitError;

var framebuffer: ?VGAFramebuffer = null;

pub fn init() VGAInitError!void {
    try multiboot.init();

    const vga_tag = try multiboot.multibootFindTag(multiboot.MultibootTagFramebuffer);
    const fb: VGAFramebuffer = .{
        .bfr = @ptrFromInt(@as(usize, @intCast(vga_tag.addr))),
        .width = vga_tag.width,
        .height = vga_tag.height,
        .pitch = vga_tag.pitch,
    };
    var offset: usize = 0;
    while (offset < fb.pitch * fb.height) : (offset += 1) {
        fb.bfr[offset] = 0xFF;
    }
    framebuffer = fb;
}
