const std = @import("std");
const sdt = @import("sdt.zig");
const aml = @import("aml.zig");

const FADT = extern struct {
    header: sdt.DefBlockHeader,
    firmware_ctrl: u32,
    dsdt: u32,
    rsvd0: u8,
    preferred_power_management_profile: u8,
    sci_interrupt: u16,
    smi_command_port: u32,
    acpi_enable: u8,
    acpi_disable: u8,
    s4bios_req: u8,
    pstate_control: u8,
    pm1a_event_block: u32,
    pm1b_event_block: u32,
    pm1a_control_block: u32,
    pm1b_control_block: u32,
    pm2_control_block: u32,
    pm_timer_block: u32,
    gpe0_block: u32,
    gpe1_block: u32,
    pm1_event_length: u8,
    pm1_control_length: u8,
    pm2_control_length: u8,
    pm_timer_length: u8,
    gpe0_length: u8,
    gpe1_length: u8,
    gpe1_base: u8,
    c_state_control: u8,
    worst_c2_latency: u16,
    worst_c3_latency: u16,
    flush_size: u16,
    flush_stride: u16,
    duty_offset: u8,
    duty_width: u8,
    day_alarm: u8,
    month_alarm: u8,
    century: u8,
    boot_architecture_flags: u16, // v2.0+ only
    rsvd1: u8,
    flags: u32,
    reset_reg: GenericAddressStructure,
    reset_value: u8,
    rsvd2: [3]u8,
    x_firmware_control: u64, // v2.0+ only
    x_dsdt: u64, // v2.0+ only
    x_pm1a_event_block: GenericAddressStructure,
    x_pm1b_event_block: GenericAddressStructure,
    x_pm1a_control_block: GenericAddressStructure,
    x_pm1b_control_block: GenericAddressStructure,
    x_pm2_control_block: GenericAddressStructure,
    x_pm_timer_block: GenericAddressStructure,
    x_gpe0_block: GenericAddressStructure,
    x_gpe1_block: GenericAddressStructure,
};

pub const GenericAddressStructure = extern struct {
    address_space: u8,
    bit_width: u8,
    bit_offset: u8,
    access_size: u8,
    address: u64,
};

const FADTError = sdt.SDTBackError || aml.AMLParseError;

pub fn initFADT(start: *sdt.DefBlockHeader, allocator: std.mem.Allocator) FADTError!void {
    const fadt: *FADT = @alignCast(@ptrCast(start));
    const dsdt: *sdt.DefBlockHeader = if (fadt.header.rev == 2)
        try sdt.backSDT(@ptrFromInt(@as(usize, @intCast(fadt.dsdt))))
    else
        try sdt.backSDT(@ptrFromInt(fadt.dsdt));
    try aml.parseAML(@as([*]u8, @ptrCast(dsdt))[0..dsdt.length], allocator);
}
