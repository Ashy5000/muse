const console = @import("console.zig");
const dump = @import("arch.zig").dump;

pub fn crashed(msg: []const u8, _: ?usize) noreturn {
    kpanic("{s}", .{msg});
}

// Using the builtin panic function doesn't work in interrupt handlers for some reason (probably the stack trace stuff is the issue), so we provide a lower-level API.
pub fn kpanic(comptime fmt: []const u8, args: anytype) noreturn {
    console.print("KERNEL PANIC\npanic: ", .{});
    console.print(fmt, args);
    dump.dumpRegs();

    asm volatile ("cli; hlt");
    // We have to convince the Zig compiler the function doesn't return:
    while (true) {}
}
