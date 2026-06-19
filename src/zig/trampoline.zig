const multiboot = @import("multiboot.zig");
const vga = @import("vga.zig");

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    vga.init() catch |err| switch (err) {
        else => {
            asm volatile ("hlt");
        },
    };

    while (true) {
        asm volatile ("hlt");
    }
}
