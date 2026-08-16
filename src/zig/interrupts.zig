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

pub fn alloc(isr: ISR) modules.InitError!?Vec {
    const bitmap = try mod.data(*u256);
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
    return null;
}

const heap = @import("alloc/heap.zig");

fn init() modules.InitError!void {
    for (exceptions) |info| {
        idt.loadISR(info.*);
    }
    asm volatile ("sti");

    const Static = struct {
        // 32 for exceptions, 32 for PIC
        var payload: u256 = 0xffffffffffffffff;
    };
    mod.payload = &Static.payload;
}

pub var mod: modules.Module = .{
    .name = "interrupts",
    .init = init,
};
