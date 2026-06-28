const std = @import("std");
const console = @import("../../console.zig");
const modules = @import("../../modules.zig");

const IDTEntry = packed struct {
    isr_lo: u16,
    segment: u16,
    rsvd0: u8 = 0,
    gate_type: u4,
    rsvd1: u1 = 0,
    dpl: u2,
    present: bool,
    isr_hi: u16,
};

const kernel_cs: u16 = 0x8;

const null_entry: IDTEntry = .{
    .isr_lo = 0,
    .segment = kernel_cs,
    .gate_type = 0xE,
    .dpl = 0,
    .present = false,
    .isr_hi = 0,
};

const IDTR = packed struct {
    size_dec: u16,
    base: u32,
};

var idt: [256]IDTEntry = @splat(null_entry);

var idtr: IDTR = .{ .size_dec = @sizeOf(@TypeOf(idt)) - 1, .base = 0 };

pub fn loadISR(irq: usize, isr: *const fn () void) void {
    idt[irq].present = true;
    idt[irq].isr_lo = @truncate(@intFromPtr(isr));
    idt[irq].isr_hi = @truncate(@intFromPtr(isr) >> 16);
}

fn divZero() void {
    std.debug.panic("division by zero", .{});
}

fn init() modules.ModuleInitError!void {
    idtr.base = @intFromPtr(&idt);
    loadISR(0, divZero);
    console.print("idtr: {} | {*}.\n", .{ idtr, &idtr });
    asm volatile ("lidt %[idtr]"
        :
        : [idtr] "m" (idtr),
    );
    asm volatile ("mov $0, %eax; mov $0, %ebx; divl %ebx, %eax");
}

pub var mod: modules.Module = .{
    .name = "idt",
    .init = init,
};
