const std = @import("std");
const hpet = @import("../drivers/time/hpet.zig");
const acpi = @import("../acpi/acpi.zig");
const lapic = @import("../smp/lapic.zig");
const ioapic = @import("../drivers/ioapic.zig");
const virtual = @import("../virtual.zig");
const modules = @import("../modules.zig");
const drivers = @import("../drivers.zig");

pub const IntervalPico = u64;

pub const Driver = struct {
    pub const InitError = error{
        IDTFull,
        NoAvailableIRQs,
    } || virtual.MapError || modules.InitError;

    period: IntervalPico,
    init: *const fn () InitError!bool,
    enable: *const fn () void,
};

pub const tick_period: IntervalPico = 200_000_000_000; // 200 microseconds

fn init() modules.InitError!void {
    for (drivers.drivers_timer) |t| {
        if (t.init() catch return error.InitializationFailure) {
            mod.payload = @ptrCast(t);
            t.period = tick_period;
            t.enable();
            return;
        }
    }
    return error.Unsupported;
}

/// The display module, which initializes the subsystem for video output.
pub var mod: modules.Module = .{
    .name = "timer",
    .init = init,
};
