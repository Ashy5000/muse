const display = @import("subsystems/display.zig");
const modules = @import("modules.zig");

const psf_magic: u32 = 0x864AB572;

const Font = extern struct {
    magic: u32,
    version: u32,
    header_size: u32,
    flags: u32,
    glyph_count: u32,
    glyph_size: u32,
    glyph_height: u32,
    glyph_width: u32,
    data_start: u8,
};

pub const font: *align(1) const Font = @ptrCast(@embedFile("font"));

comptime {
    if (font.magic != psf_magic) {
        @compileError("PSF file has invalid magic value");
    }
    if (font.flags > 0) {
        @compileError("PSF file uses Unicode (unsupported)");
    }
}

pub fn put_char(d: *display.Driver, char: u8, grid_x: usize, grid_y: usize, color: display.Color) display.DrawError!void {
    const x_c = grid_x * font.glyph_width;
    const y_c = grid_y * font.glyph_height;
    const line_size = (font.glyph_width + 7) / 8;
    const data: [*]const u8 = @as(
        [*]align(1) const u8,
        @ptrCast(&font.data_start),
    ) + font.glyph_size * char;
    var offset: usize = 0;
    var y: usize = 0;
    while (y < font.glyph_height) : (y += 1) {
        var x: usize = 0;
        while (x < font.glyph_width) : (x += 1) {
            if (((data[offset + (x / 8)] >> @intCast(7 - (x % 8))) & 1) > 0) {
                try d.putPixel(x + x_c, y + y_c, color);
            } else {
                try d.putPixel(x + x_c, y + y_c, 0x000000);
            }
        }
        offset += line_size;
    }
}
