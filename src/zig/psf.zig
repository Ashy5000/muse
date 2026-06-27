const display = @import("display.zig");
const modules = @import("modules.zig");

const psf_magic: u32 = 0x864AB572;

const font_file = @embedFile("font.psf");

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

pub var font: ?*align(1) const Font = null;

pub const PSFDrawError = error{
    PSFUninit,
} || display.DisplayDrawError;

pub fn put_char(d: *display.Display, char: u8, grid_x: usize, grid_y: usize, color: display.Color) PSFDrawError!void {
    const f = font orelse return error.PSFUninit;
    const x_c = grid_x * f.glyph_width;
    const y_c = grid_y * f.glyph_height;
    const line_size = (f.glyph_width + 7) / 8;
    const data: [*]const u8 = @as([*]align(1) const u8, @ptrCast(&f.data_start)) + f.glyph_size * char;
    var offset: usize = 0;
    var y: usize = 0;
    while (y < f.glyph_height) : (y += 1) {
        var x: usize = 0;
        while (x < f.glyph_width) : (x += 1) {
            if (((data[offset + (x / 8)] >> @intCast(7 - (x % 8))) & 1) > 0) {
                try d.putPixel(x + x_c, y + y_c, color);
            } else {
                try d.putPixel(x + x_c, y + y_c, 0x000000);
            }
        }
        offset += line_size;
    }
}

pub fn init() modules.ModuleInitError!void {
    const f = @as(*align(1) const Font, @ptrCast(font_file));
    if (f.magic != psf_magic) {
        @compileError("PSF file has invalid magic value");
    }
    if (f.flags > 0) {
        @compileError("PSF file uses Unicode (unsupported)");
    }
    font = f;
}

pub var mod: modules.Module = .{
    .name = "psf",
    .init = init,
};
