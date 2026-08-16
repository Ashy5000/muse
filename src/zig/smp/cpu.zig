const std = @import("std");
const scheduler = @import("../scheduler.zig");
const lapic = @import("lapic.zig");
const modules = @import("../modules.zig");

pub const CPU = struct {
    lapic_id: u8,
    queue: scheduler.Queue,
};

pub var cpus = std.ArrayList(CPU).empty;

pub fn getActiveCPU() modules.InitError!*CPU {
    const regs = try lapic.mod.data(*volatile lapic.LAPICRegisters);
    const id = regs.lapic_id;
    for (cpus.items) |*cpu| {
        if (id == cpu.lapic_id) {
            return cpu;
        }
    }
    unreachable;
}
