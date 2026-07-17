const std = @import("std");

pub const CPU = struct {
    lapic_id: u8,
};

pub var cpus = std.ArrayList(CPU).empty;
