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

fn idle() void {
    while (true) {
        scheduler.preempt() catch |err|
            console.print("Idle task failed to context switch: {}.\n", .{err});
    }
}

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    @import("arch.zig").paging.init() catch unreachable;
    interrupts.mod.load() catch unreachable;
    console.mod.load() catch unreachable;
    timer.mod.load() catch unreachable;

    var idle_task = contextSwitch.createKernelTask(idle) catch unreachable;
    scheduler.push(&idle_task) catch unreachable;

    pci.init() catch unreachable;

    console.print("Initialization complete.\n", .{});

    while (true) {
        asm volatile ("hlt");
    }
}
