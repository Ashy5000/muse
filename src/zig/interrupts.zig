const std = @import("std");
const idt = @import("arch.zig").idt;
const console = @import("console.zig");
const modules = @import("modules.zig");

pub const ISR = *const fn (*anyopaque, usize) callconv(idt.int_callconv) void;

pub const ISRInfo = struct {
    vec: Vec,
    isr: ISR,
};

const exceptions: [2]*const ISRInfo = .{
    &@import("exceptions/gpf.zig").isr_info,
    &@import("exceptions/page_fault.zig").isr_info,
};

var idt_bitmap: u256 = 0xffffffff; // Reserve exception vectors

pub const Vec = std.math.Log2Int(@TypeOf(idt_bitmap));

pub fn alloc(isr: ISR) ?Vec {
    for (0..@bitSizeOf(@TypeOf(idt_bitmap))) |i| {
        const vec: Vec = @intCast(i);
        if ((idt_bitmap >> vec) & 0x1 == 0) {
            idt_bitmap |= @as(@TypeOf(idt_bitmap), 1) << vec;
            idt.loadISR(.{
                .vec = vec,
                .isr = isr,
            });
            return vec;
        }
    }
    return null;
}

fn init() modules.ModuleInitError!void {
    for (exceptions) |info| {
        idt.loadISR(info.*);
    }
    asm volatile ("sti");
}

pub var mod: modules.Module = .{
    .name = "interrupts",
    .init = init,
    .deps = &.{&idt.mod},
};
