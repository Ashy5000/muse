const console = @import("console.zig");

pub fn crashed(msg: []const u8, _: ?usize) noreturn {
    console.print("KERNEL PANIC\npanic: {s}\n", .{msg});

    asm volatile ("cli; hlt");
    // We have to convince the Zig compiler the function doesn't return:
    while (true) {}
}
