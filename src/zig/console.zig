const std = @import("std");
const psf = @import("psf.zig");
const display = @import("display.zig");
const modules = @import("modules.zig");

const Console = struct {
    d: *display.Display,
    x: usize,
    y: usize,
};

var console: ?Console = null;

pub const ConsoleInitError = psf.PSFInitError || display.DisplayInitError;

const ConsolePrintError = error{
    ConsoleUninit,
} || psf.PSFDrawError;

fn maybeScroll(cons: *Console) ConsolePrintError!void {
    if (cons.y < cons.d.height / psf.font.?.glyph_height) {
        return;
    }
    cons.y -= 1;
    try cons.d.scrollGrid(psf.font.?.glyph_height);
}

pub fn printChar(char: u8) ConsolePrintError!void {
    var cons = &(console orelse return error.ConsoleUninit);
    if (char == '\n') {
        cons.x = 0;
        cons.y += 1;
        try maybeScroll(cons);
        return;
    }
    try psf.put_char(cons.d, char, cons.x, cons.y, 0xFFFFFF);
    cons.x += 1;
    if (cons.x == cons.d.width / psf.font.?.glyph_width) {
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

pub fn print(comptime fmt: []const u8, args: anytype) void {
    console_writer.print(fmt, args) catch return;
}

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

fn init() ConsoleInitError!void {
    console = .{
        .d = display.display_primary.?,
        .x = 0,
        .y = 0,
    };
}

pub var mod: modules.Module = .{
    .name = "console",
    .deps = &@as([2]*modules.Module, .{ &psf.mod, &display.mod }),
    .init = init,
};
