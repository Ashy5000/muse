const std = @import("std");
const builtin = @import("builtin");
const multiboot = @import("multiboot.zig");
const modules = @import("modules.zig");
const console = @import("console.zig");
const pmm = @import("pmm.zig");
const elf = @import("elf.zig");
const panic_mod = @import("panic.zig");

pub const panic = std.debug.FullPanic(panic_mod.crashed);

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    modules.loadModule(&modules.mod) catch asm volatile ("hlt");
    modules.loadModule(&pmm.mod) catch {
        console.print("Loading PMM failed!.\n", .{});
        asm volatile ("hlt");
    };
    modules.loadModule(&elf.mod) catch {
        console.print("Loading ELF failed!.\n", .{});
        asm volatile ("hlt");
    };

    console.print("Initialization complete.\n", .{});

    while (true) {
        asm volatile ("hlt");
    }
}
