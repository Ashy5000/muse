const console = @import("console.zig");

pub fn crashed(msg: []const u8, _: ?usize) noreturn {
    console.print("KERNEL PANIC\npanic: {s}\n", .{msg});

    asm volatile ("cli");
    while (true) {}
}
