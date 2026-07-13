const std = @import("std");
const builtin = @import("builtin");
const multiboot = @import("multiboot.zig");
const modules = @import("modules.zig");
const console = @import("console.zig");
const panic_mod = @import("panic.zig");
const interrupts = @import("interrupts.zig");
const pci = @import("pci.zig");
const cpuid = @import("cpuid.zig");
const scheduler = @import("scheduler.zig");
const contextSwitch = @import("arch.zig").contextSwitch;

pub const panic = std.debug.FullPanic(panic_mod.crashed);

var task: scheduler.Task = undefined;

fn taskTest() void {
    console.print("meow\n", .{});
    contextSwitch.contextSwitch(&task, &scheduler.root_task);
}

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    modules.loadModule(&interrupts.mod) catch unreachable;
    modules.loadModule(&console.mod) catch unreachable;

    contextSwitch.createKernelTask(taskTest) catch unreachable;
    contextSwitch.contextSwitch(&scheduler.root_task, &task);
    console.print("woof\n", .{});

    console.print("Initialization complete.\n", .{});

    while (true) {
        asm volatile ("hlt");
    }
}
