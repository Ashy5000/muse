const idt = @import("arch.zig").idt;
const modules = @import("modules.zig");

pub const ISRInfo = struct {
    vec: u8,
    isr: *const fn (*anyopaque, usize) callconv(idt.int_callconv) void,
};

const isrs: [2]*ISRInfo = .{
    &@import("exceptions/gpf.zig").isr_info,
    &@import("exceptions/page_fault.zig").isr_info,
};

fn init() modules.ModuleInitError!void {
    for (isrs) |isr| {
        idt.loadISR(isr.vec, isr.isr);
    }
    asm volatile ("sti");
}

pub var mod: modules.Module = .{
    .name = "interrupts",
    .init = init,
    .deps = &.{&idt.mod},
};
