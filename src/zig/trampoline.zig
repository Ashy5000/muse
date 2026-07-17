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
const lapic = @import("drivers/smp/lapic.zig");
const ioapic = @import("drivers/ioapic.zig");
const contextSwitch = @import("arch.zig").contextSwitch;

pub const panic = std.debug.FullPanic(panic_mod.crashed);

var task: scheduler.Task = undefined;

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    modules.loadModule(&interrupts.mod) catch unreachable;
    modules.loadModule(&console.mod) catch unreachable;
    modules.loadModule(&pci.mod) catch unreachable;
    modules.loadModule(&lapic.mod) catch unreachable;
    modules.loadModule(&ioapic.mod) catch unreachable;

    console.print("Initialization complete.\n", .{});

    while (true) {
        asm volatile ("hlt");
    }
}
