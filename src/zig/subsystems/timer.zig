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
    } || virtual.MapError;

    period: IntervalPico,
    init: *const fn () InitError!bool,
    enable: *const fn () void,
};

pub var system_timer: ?*Driver = null;

pub const tick_period: IntervalPico = 200_000_000; // 200 microseconds

fn init() modules.ModuleInitError!void {
    for (drivers.drivers_timer) |t| {
        if (t.init() catch return error.ModuleInitFailure) {
            system_timer = t;
            t.period = tick_period;
            t.enable();
            return;
        }
    }
    return error.ModuleUnsupported;
}

/// The display module, which initializes the subsystem for video output.
pub var mod: modules.Module = .{
    .name = "timer",
    .init = init,
    // TODO: Do this dynamically
    .deps = &.{ &acpi.mod, &virtual.mod, &lapic.mod, &ioapic.mod },
};
