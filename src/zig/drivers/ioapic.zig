const std = @import("std");
const madt = @import("smp/madt.zig");
const heap = @import("../alloc/heap.zig");
const console = @import("../console.zig");
const virtual = @import("../virtual.zig");
const cpu = @import("smp/cpu.zig");
const modules = @import("../modules.zig");

pub const IOAPIC = struct {
    registers: *volatile extern struct {
        reg_select: u32,
        padding: [3]u32,
        val: u32,
    },
    id: u4,
    version: u8,
    entry_cnt: u8,

    fn readReg(self: *const IOAPIC, reg: u32) u32 {
        self.registers.reg_select = reg;
        return self.registers.val;
    }

    fn writeReg(self: *const IOAPIC, reg: u32, val: u32) void {
        self.registers.reg_select = reg;
        self.registers.val = val;
    }

    fn detect(self: *IOAPIC) void {
        const IDReg = packed struct {
            rsvd0: u24,
            id: u4,
            rsvd1: u4,
        };
        const id_reg: IDReg = @bitCast(self.readReg(0x00));
        self.id = id_reg.id;

        const InfoReg = packed struct {
            version: u8,
            rsvd0: u8,
            entry_cnt: u8,
            rsvd1: u8,
        };
        const info_reg: InfoReg = @bitCast(self.readReg(0x01));
        self.version = info_reg.version;
        self.entry_cnt = info_reg.entry_cnt;
    }

    const Polarity = enum(u1) { active_high, active_low };
    const TriggerMode = enum(u1) { edge_sensitive, level_sensitive };

    fn map(
        self: *const IOAPIC,
        irq: u8,
        vec: u8,
        polarity: Polarity,
        trigger_mode: TriggerMode,
        target: cpu.CPU,
    ) void {
        const RedirectionEntryLow = packed struct {
            vec: u8,
            delivery_mode: enum(u3) {
                normal = 0,
                low_priority = 1,
                system_management_interrupt = 2,
                non_maskable_interrupt = 4,
                init = 5,
                external = 7,
            },
            destination_mode: enum(u1) { physical, logical },
            apic_busy: bool,
            polarity: Polarity,
            level_triggered_status: bool,
            trigger_mode: TriggerMode,
            mask_interrupt: bool,
            rsvd: u15,
        };
        const RedirectionEntryHigh = packed struct {
            rsvd: u24,
            destination: u8,
        };

        const reg_low: u32 = 0x10 + irq;
        const reg_high: u32 = 0x11 + irq;

        var entry_low: RedirectionEntryLow = @bitCast(self.readReg(reg_low));
        var entry_high: RedirectionEntryHigh = @bitCast(self.readReg(reg_high));

        entry_low.vec = vec;
        entry_low.delivery_mode = .normal;
        entry_low.destination_mode = .physical;
        entry_low.polarity = polarity;
        entry_low.trigger_mode = trigger_mode;
        entry_low.mask_interrupt = false;

        entry_high.destination = target.lapic_id;
    }
};

var ioapics = std.ArrayList(IOAPIC).empty;

fn init() modules.ModuleInitError!void {
    const gpa = heap.allocator() catch return error.ModuleInitFailure;
    var ioapic_entries = madt.findMADTEntries(
        madt.MADTEntryIOAPIC,
        gpa,
    ) catch return error.ModuleInitFailure;
    defer ioapic_entries.deinit(gpa);
    for (ioapic_entries.items) |entry| {
        const regs_phys: []u8 = @as([*]u8, @ptrFromInt(entry.addr))[0 .. 5 * @sizeOf(u32)];
        const regs_virt: []u8 = virtual.mapPhysObj(
            regs_phys,
            .{ .cache_mode = .Uncacheable },
        ) catch return error.ModuleInitFailure;
        var ioapic: IOAPIC = undefined;
        ioapic.registers = @alignCast(@ptrCast(regs_virt.ptr));
        ioapic.detect();
        console.print("I/O APIC: {}.\n", .{ioapic});
        ioapics.append(gpa, ioapic) catch return error.ModuleInitFailure;
    }
}

pub var mod: modules.Module = .{
    .name = "ioapic",
    .init = init,
    .deps = &.{ &madt.mod, &heap.mod, &virtual.mod },
};
