const std = @import("std");
const builtin = @import("builtin");
const multiboot = @import("multiboot.zig");
const modules = @import("modules.zig");
const console = @import("console.zig");
const panic_mod = @import("panic.zig");
const interrupts = @import("interrupts.zig");
const pmm = @import("alloc/pmm.zig");
const pci = @import("pci.zig");
const paging = @import("arch.zig").paging;

pub const panic = std.debug.FullPanic(panic_mod.crashed);

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    modules.loadModule(&paging.mod) catch asm volatile ("hlt");

    while (true) {
        asm volatile ("hlt");
    }
}
