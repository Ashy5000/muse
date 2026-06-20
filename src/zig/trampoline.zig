const multiboot = @import("multiboot.zig");
const console = @import("console.zig");

export fn trampoline_main(multiboot_info: *multiboot.MultibootInfo, multiboot_magic: u32) callconv(.c) void {
    multiboot.config(multiboot_info, multiboot_magic);

    console.init() catch asm volatile ("hlt");

    console.print("meow.\n", .{});

    while (true) {
        asm volatile ("hlt");
    }
}
