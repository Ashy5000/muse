const std = @import("std");
const builtin = @import("builtin");
const multiboot = @import("multiboot.zig");
const modules = @import("modules.zig");
const console = @import("console.zig");
const panic_mod = @import("panic.zig");
const interrupts = @import("interrupts.zig");
const pci = @import("drivers/pci.zig");
const cpuid = @import("cpuid.zig");
const scheduler = @import("scheduler.zig");
const timer = @import("timer.zig");
const contextSwitch = @import("arch.zig").contextSwitch;

pub const panic = std.debug.FullPanic(panic_mod.crashed);

var task: scheduler.Task = undefined;

fn meow() void {
    while (true) {
        console.print("b", .{});
        asm volatile ("hlt");
    }
}

fn woof() void {
    while (true) {
        console.print("c", .{});
        asm volatile ("hlt");
    }
}

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    modules.loadModule(&interrupts.mod) catch unreachable;
    modules.loadModule(&console.mod) catch unreachable;
    modules.loadModule(&pci.mod) catch unreachable;
    modules.loadModule(&timer.mod) catch unreachable;

    var ctx = contextSwitch.createKernelTask(meow) catch unreachable;
    var ctx2 = contextSwitch.createKernelTask(woof) catch unreachable;
    scheduler.push(&ctx);
    scheduler.push(&ctx2);

    console.print("Initialization complete.\n", .{});

    while (true) {
        console.print("a", .{});
        asm volatile ("hlt");
    }
}
