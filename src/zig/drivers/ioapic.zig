const std = @import("std");
const madt = @import("../smp/madt.zig");
const heap = @import("../alloc/heap.zig");
const console = @import("../console.zig");
const virtual = @import("../virtual.zig");
const cpu = @import("../smp/cpu.zig");
const bitmaps = @import("../utils/bitmaps.zig");
const interrupts = @import("../interrupts.zig");
const modules = @import("../modules.zig");

pub const IOAPIC = struct {
    registers: *volatile extern struct {
        reg_select: u32,
        padding: [3]u32,
        val: u32,
    },
    id: u4,
    version: u8,
    base: u5,
    entry_cnt: u8,
    input_map: u32,

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
        self.input_map = 0;
        const bits: std.math.Log2Int(u32) = @intCast(self.entry_cnt);
        for (0..bits) |i| {
            // Set = available
            self.input_map |= @as(u32, 1) << @intCast(i);
        }
    }

    fn alloc(self: *IOAPIC, mask: u32) ?std.math.Log2Int(u32) {
        for (self.base..self.base + self.entry_cnt) |i_usize| {
            const i: std.math.Log2Int(u32) = @intCast(i_usize);
            if ((self.input_map >> i) & 1 == 1 and (mask >> i) & 1 == 1) {
                self.input_map |= @as(u32, 1) << i;
                return i;
            }
        }
        return null;
    }

    const Polarity = enum(u1) { active_high, active_low };
    pub const TriggerMode = enum(u1) { edge_sensitive, level_sensitive };

    fn map(
        self: *const IOAPIC,
        irq: u8,
        vec: interrupts.Vec,
        polarity: Polarity,
        trigger_mode: TriggerMode,
        target: cpu.CPU,
    ) void {
        console.print("Mapping {}->{}.\n", .{ irq, vec });
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

        const reg_low: u32 = 0x10 + (irq * 2) - self.base;
        const reg_high: u32 = 0x11 + (irq * 2) - self.base;

        var entry_low: RedirectionEntryLow = @bitCast(self.readReg(reg_low));
        var entry_high: RedirectionEntryHigh = @bitCast(self.readReg(reg_high));

        // Mask out the interrupt while we are changing routing details so that
        // triggered interrupts don't get a broken I/O APIC state.
        entry_low.mask_interrupt = true;
        self.writeReg(reg_low, @bitCast(entry_low));

        entry_low.vec = vec;
        entry_low.delivery_mode = .normal;
        entry_low.destination_mode = .physical;
        entry_low.polarity = polarity;
        entry_low.trigger_mode = trigger_mode;
        entry_low.mask_interrupt = false;

        entry_high.destination = target.lapic_id;

        // Write high before low so that interrupt unmasking happens at the
        // very end.
        self.writeReg(reg_high, @bitCast(entry_high));
        self.writeReg(reg_low, @bitCast(entry_low));
    }
};

pub fn alloc(
    mask: u32,
    vec: interrupts.Vec,
    polarity: IOAPIC.Polarity,
    trigger_mode: IOAPIC.TriggerMode,
    target: cpu.CPU,
) ?std.math.Log2Int(u32) {
    for (0..ioapics.items.len) |i| {
        if (ioapics.items[i].alloc(mask)) |irq| {
            ioapics.items[i].map(irq, vec, polarity, trigger_mode, target);
            return irq;
        }
    }
    return null;
}

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
        ioapic.base = @intCast(entry.base);
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
