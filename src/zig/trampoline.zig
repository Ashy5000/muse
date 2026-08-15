const std = @import("std");
const builtin = @import("builtin");
const multiboot = @import("multiboot.zig");
const modules = @import("modules.zig");
const console = @import("console.zig");
const panic_mod = @import("panic.zig");
const interrupts = @import("interrupts.zig");
const pci = @import("subsystems/pci.zig");
const cpuid = @import("cpuid.zig");
const scheduler = @import("scheduler.zig");
const timer = @import("subsystems/timer.zig");
const tick = @import("tick.zig");
const contextSwitch = @import("arch.zig").contextSwitch;

pub const panic = std.debug.FullPanic(panic_mod.crashed);

var task: scheduler.Task = undefined;

fn meow() void {
    const allocator = (@import("alloc/heap.zig").mod.data(std.mem.Allocator) catch unreachable).*;
    tick.sleep(allocator, 3e12) catch unreachable;
    console.print("meow", .{});
    while (true) {
        asm volatile ("hlt");
    }
}

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    @import("arch.zig").paging.init() catch unreachable;
    interrupts.mod.init() catch unreachable;
    console.mod.init() catch unreachable;
    timer.mod.init() catch unreachable;
    pci.init() catch unreachable;

    var ctx = contextSwitch.createKernelTask(meow) catch unreachable;
    scheduler.push(&ctx);

    console.print("Initialization complete.\n", .{});

    while (true) {
        asm volatile ("hlt");
    }
}
