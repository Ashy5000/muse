const std = @import("std");
const builtin = @import("builtin");
const multiboot = @import("multiboot.zig");
const mmap = @import("mmap.zig");
const modules = @import("modules.zig");
const console = @import("console.zig");

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    modules.loadModule(&modules.mod) catch asm volatile ("hlt");
    modules.loadModule(&mmap.mod) catch asm volatile ("hlt");

    console.hexdump(@as([*]const u8, @ptrFromInt(0x200000))[0..50]);

    while (true) {
        asm volatile ("hlt");
    }
}
