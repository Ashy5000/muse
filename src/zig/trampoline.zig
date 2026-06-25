const std = @import("std");
const builtin = @import("builtin");
const multiboot = @import("multiboot.zig");
const modules = @import("modules.zig");
const console = @import("console.zig");
const virtual = @import("virtual.zig");
const panic_mod = @import("panic.zig");
const alloc = @import("alloc.zig");

pub const panic = std.debug.FullPanic(panic_mod.crashed);

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    modules.loadModule(&modules.mod) catch asm volatile ("hlt");
    modules.loadModule(&alloc.mod) catch asm volatile ("hlt");
    console.print("Initialization complete.\n", .{});

    while (true) {
        asm volatile ("hlt");
    }
}
