const std = @import("std");

const Subsystem = struct {
    root_name: []const u8,
    suffix: []const u8,
    files: []const []const u8,
};

const subsystems = [_]Subsystem{
    .{
        .root_name = "display.zig",
        .suffix = "display",
        .files = &.{
            "drivers/video/vga.zig",
        },
    },
    .{
        .root_name = "timer.zig",
        .suffix = "timer",
        .files = &.{
            "drivers/time/hpet.zig",
        },
    },
    .{
        .root_name = "pci.zig",
        .suffix = "pci",
        .files = &.{
            "drivers/massStorage/ata.zig",
        },
    },
};

const Args = struct {
    named: struct {
        out_path: []const u8,
    },
    positional: []const []const u8,
};

const Io = std.Io;

const CLIError = error{TooFewArguments};

pub fn main(init: std.process.Init) !void {
    var args_iter = init.minimal.args.iterate();
    _ = args_iter.skip();
    const out_path = args_iter.next() orelse {
        std.debug.print("No arguments!\n", .{});
        return error.TooFewArguments;
    };
    const io = init.io;
    const out = try Io.Dir.cwd().createFile(io, out_path, .{});
    defer out.close(io);
    const buf = try init.arena.allocator().alloc(u8, 1024);
    var file_writer = out.writer(io, buf);
    var writer = &file_writer.interface;
    for (subsystems) |sys| {
        _ = try writer.print("pub const drivers_{s} = [_]*@import(\"subsystems/{s}\").Driver{{\n", .{
            sys.suffix,
            sys.root_name,
        });
        for (sys.files) |path| {
            _ = try writer.print("    &@import(\"{s}\").driver_{s},\n", .{
                path,
                sys.suffix,
            });
        }
        _ = try writer.write("};\n");
    }
    try writer.flush();
}
