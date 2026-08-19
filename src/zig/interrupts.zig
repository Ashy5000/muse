const std = @import("std");
const idt = @import("arch.zig").idt;
const modules = @import("modules.zig");

pub const ISR = *const fn () callconv(idt.int_callconv) void;

pub const ISRInfo = struct {
    vec: Vec,
    isr: ISR,
};

const exceptions: [2]*const ISRInfo = .{
    &@import("exceptions/gpf.zig").isr_info,
    &@import("exceptions/page_fault.zig").isr_info,
};

pub const Vec = std.math.Log2Int(u256);

pub const AllocError = error{NoAvailableIRQs} || modules.InitError;

pub fn alloc(isr: ISR) AllocError!Vec {
    const bitmap = try mod.data_ref();
    for (0..256) |i| {
        const vec: Vec = @intCast(i);
        if ((bitmap.* >> vec) & 0x1 == 0) {
            bitmap.* |= @as(u256, 1) << vec;
            idt.loadISR(.{
                .vec = vec,
                .isr = isr,
            });
            return vec;
        }
    }
    return error.NoAvailableIRQs;
}

const heap = @import("alloc/heap.zig");

fn init() modules.InitError!u256 {
    for (exceptions) |info| {
        idt.loadISR(info.*);
    }
    asm volatile ("sti");

    // 32 for exceptions, 16 for PIC
    return 0xffffffffffff;
}

pub var mod: modules.Module(u256) = .{
    .name = "interrupts",
    .init = init,
};
