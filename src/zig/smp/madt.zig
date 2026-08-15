const std = @import("std");
const sdt = @import("../acpi/sdt.zig");
const acpi = @import("../acpi/acpi.zig");
const modules = @import("../modules.zig");

pub const MADTFlags = packed struct(u32) {
    pic_installed: bool,
    rsvd: u31,
};

pub const MADT = extern struct {
    header: sdt.DefBlockHeader,
    lapic_addr: u32,
    flags: MADTFlags,
    first_entry: MADTEntry,
};

pub const MADTEntryType = enum(u8) {
    lapic,
    ioapic,
    ioapic_int_override,
    ioapic_nmi,
    lapic_nmi,
    lapic_64,
    lapic_x2,
};

pub const MADTEntry = extern struct {
    type: MADTEntryType,
    length: u8,
};

pub const MADTEntryLAPIC = extern struct {
    type: MADTEntryType = .lapic,
    length: u8,
    acpi_id: u8,
    apic_id: u8,
    flags: u32,
};

pub const MADTEntryLAPICx2 = extern struct {
    type: MADTEntryType = .lapic_x2,
    length: u8,
    // 2 bytes padding
    apic_id: u32,
    flags: u32,
    acpi_id: u32,
};

pub const MADTEntryLAPIC64 = extern struct {
    type: MADTEntryType = .lapic_64,
    length: u8,
    // 2 bytes padding
    lapic_addr: u64 align(4),
};

pub const MADTEntryIOAPIC = extern struct {
    type: MADTEntryType = .ioapic,
    length: u8,
    id: u8,
    rsvd: u8,
    addr: u32,
    base: u32,
};

pub const MADTEntryInterruptOverride = extern struct {
    type: MADTEntryType = .ioapic_int_override,
    length: u8,
    bus_src: u8,
    src_legacy: u8,
    dest_apic: u32,
    flags: u16,
};

pub const MADTError = error{
    NoMADT,
} || std.mem.Allocator.Error;

var madt_global: ?*MADT = null;

var overrides: ?std.ArrayList(*align(1) const MADTEntryInterruptOverride) = null;

pub fn findMADTEntries(
    res_type: type,
    gpa: std.mem.Allocator,
) MADTError!std.ArrayList(*align(1) const res_type) {
    const struct_info = @typeInfo(res_type).@"struct";
    const field_type = struct_info.field_types[0];
    if (field_type != MADTEntryType) {
        @compileError("invalid MADT entry struct: first field should be a MADTEntryType");
    }
    const entry_type = struct_info.field_attrs[0].defaultValue(field_type);
    const madt: *MADT = madt_global orelse return error.NoMADT;
    var entry: *const MADTEntry = &madt.first_entry;
    var entries = std.ArrayList(*align(1) const res_type).empty;
    while (@intFromPtr(entry) < @intFromPtr(madt) + madt.header.length) {
        if (entry.type == entry_type) {
            try entries.append(gpa, @ptrCast(entry));
        }
        entry = @ptrFromInt(@intFromPtr(entry) + entry.length);
    }
    return entries;
}

fn init() modules.ModuleInitError!void {
    madt_global = @ptrCast(
        acpi.findSDT("APIC") orelse return error.ModuleUnsupported,
    );
}

pub fn redirectionInfo(legacy_irq: u8, gpa: std.mem.Allocator) MADTError!?u32 {
    const entries = overrides orelse new: {
        overrides = try findMADTEntries(MADTEntryInterruptOverride, gpa);
        break :new overrides;
    };
    for (entries.items) |entry| {
        if (entry.src_legacy == legacy_irq) {
            return entry.dest_apic;
        }
    }
    return null;
}

pub var mod: modules.Module = .{
    .name = "madt",
    .init = init,
    .deps = &.{&acpi.mod},
};
