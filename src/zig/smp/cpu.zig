const std = @import("std");
const scheduler = @import("../scheduler.zig");
const lapic = @import("lapic.zig");

pub const CPU = struct {
    lapic_id: u8,
    queue: scheduler.Queue,
};

pub var cpus = std.ArrayList(CPU).empty;

pub fn getActiveCPU() *CPU {
    const id = lapic.lapic_regs.?.lapic_id;
    for (cpus.items) |*cpu| {
        if (id == cpu.lapic_id) {
            return cpu;
        }
    }
    unreachable;
}
