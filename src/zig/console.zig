const std = @import("std");
const psf = @import("psf.zig");
const display = @import("subsystems/display.zig");
const io = @import("utils/io.zig");
const modules = @import("modules.zig");

const Console = struct {
    d: *display.Driver,
    x: usize,
    y: usize,
};

fn maybeScroll(cons: *Console) display.DrawError!void {
    if (cons.y < cons.d.height / psf.font.glyph_height) {
        return;
    }
    cons.y -= 1;
    try cons.d.scrollGrid(psf.font.glyph_height);
}

const PrintError = display.DrawError || modules.InitError;

fn printChar(char: u8) PrintError!void {
    const cons = try mod.data_ref();
    io.out(8, 0xe9, char);
    if (char == '\n') {
        cons.x = 0;
        cons.y += 1;
        try maybeScroll(cons);
        return;
    }
    try psf.put_char(cons.d, char, cons.x, cons.y, 0xFFFFFF);
    cons.x += 1;
    if (cons.x == cons.d.width / psf.font.glyph_width) {
        cons.x = 0;
        cons.y += 1;
        try maybeScroll(cons);
    }
}

fn printString(str: []const u8) std.Io.Writer.Error!void {
    for (str) |char| {
        printChar(char) catch return error.WriteFailed;
    }
}

fn drain(w: *std.Io.Writer, data: []const []const u8, splat: usize) std.Io.Writer.Error!usize {
    // Flush the buffer
    if (w.end != 0) {
        try printString(w.buffered());
        w.end = 0;
    }

    var consumed: usize = 0;

    for (data[0 .. data.len - 1]) |str| {
        try printString(str);
        consumed += str.len;
    }

    const last_elem: []const u8 = data[data.len - 1];
    if (last_elem.len > 0) {
        for (0..splat) |_| {
            try printString(last_elem);
            consumed += last_elem.len;
        }
    }

    return consumed;
}

/// Creates a std.Io.Writer attached to the system console.
pub fn writer(buf: []u8) std.Io.Writer {
    return .{
        .buffer = buf,
        .end = 0,
        .vtable = &.{
            .drain = drain,
        },
    };
}

var console_writer = writer(&.{});

/// Formats and prints a string to the system console.
pub fn print(comptime fmt: []const u8, args: anytype) void {
    console_writer.print(fmt, args) catch return;
}

/// Generates a hexadecimal dump of a data slice.
pub fn hexdump(data: []const u8) void {
    var offset: usize = 0;
    while (offset < data.len) : (offset += 16) {
        print("{x:0>8} ", .{@intFromPtr(@as([*]const u8, @ptrCast(data))) + offset});
        for (0..@min(16, data.len - offset)) |i| {
            print("{x:0>2} ", .{data[offset + i]});
        }
        print("\n", .{});
    }
}

const heap = @import("alloc/heap.zig");

fn init() modules.InitError!Console {
    return .{
        .d = try display.mod.data(),
        .x = 0,
        .y = 0,
    };
}

/// The console module, which initializes the system console.
pub var mod: modules.Module(Console) = .{
    .name = "console",
    .init = init,
};
