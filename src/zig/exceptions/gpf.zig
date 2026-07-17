const std = @import("std");
const interrupts = @import("../interrupts.zig");
const idt = @import("../arch.zig").idt;

fn gpf(_: *anyopaque, _: usize) callconv(idt.int_callconv) void {
    std.debug.panic("#GP", .{});
}

/// A descriptor for an GPF handler ISR.
pub var isr_info: interrupts.ISRInfo = .{
    .vec = 0xD,
    .isr = gpf,
};
