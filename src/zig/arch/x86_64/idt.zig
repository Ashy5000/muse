const std = @import("std");
const interrupts = @import("../../interrupts.zig");

const IDTEntry = packed struct {
    isr_lo: u16,
    segment: u16,
    ist: u3,
    rsvd0: u5 = 0,
    gate_type: u4,
    rsvd1: u1 = 0,
    dpl: u2,
    present: bool,
    isr_hi: u48,
    rsvd2: u32 = 0,
};

const kernel_cs: u16 = 0x8;

const null_entry: IDTEntry = .{
    .isr_lo = 0,
    .segment = kernel_cs,
    .ist = 0,
    .gate_type = 0xe,
    .dpl = 0,
    .present = false,
    .isr_hi = 0,
};

const IDTR = packed struct(u128) {
    size_dec: u16,
    base: u64,
    padding: u48 = 0,
};

var idt: [256]IDTEntry = @splat(null_entry);

export var idtr: IDTR = .{ .size_dec = @sizeOf(@TypeOf(idt)) - 1, .base = 0 };

pub const int_callconv = std.lang.CallingConvention{ .x86_64_interrupt = .{} };

var loaded = false;
pub fn loadISR(info: interrupts.ISRInfo) void {
    if (!loaded) {
        idtr.base = @intFromPtr(&idt);
        asm volatile ("lidt [idtr]");
        loaded = true;
    }
    idt[info.vec].present = true;
    idt[info.vec].isr_lo = @truncate(@intFromPtr(info.isr));
    idt[info.vec].isr_hi = @truncate(@intFromPtr(info.isr) >> 16);
}
