const std = @import("std");
const heap = @import("../alloc/heap.zig");
const madt = @import("madt.zig");
const console = @import("../console.zig");
const virtual = @import("../virtual.zig");
const msr = @import("../utils/msr.zig");
const pic = @import("../drivers/pic.zig");
const cpuid = @import("../cpuid.zig");
const cpu = @import("cpu.zig");
const scheduler = @import("../scheduler.zig");
const modules = @import("../modules.zig");

pub const SpuriousInterruptReg = packed struct(u32) {
    vec: u8,
    apic_software_enable: bool,
    focus_processor_checking: bool,
    rsvd0: u2,
    eoi_brodcast_suppression: bool,
    rsvd1: u19,
};

pub const LAPICRegisters = extern struct {
    rsvd0: [32]u8 align(16),
    lapic_id: u32 align(16),
    version: u32 align(16),
    rsvd1: [64]u8 align(16),
    task_priority: u32 align(16),
    arbitration_priority: u32 align(16),
    processor_priority: u32 align(16),
    eoi: u32 align(16),
    remote_read: u32 align(16),
    logical_dest: u32 align(16),
    dest_format: u32 align(16),
    spurious_int: SpuriousInterruptReg align(16),
    isr0: u32 align(16),
    isr1: u32 align(16),
    isr2: u32 align(16),
    isr3: u32 align(16),
    isr4: u32 align(16),
    isr5: u32 align(16),
    isr6: u32 align(16),
    isr7: u32 align(16),
    tmr0: u32 align(16),
    tmr1: u32 align(16),
    tmr2: u32 align(16),
    tmr3: u32 align(16),
    tmr4: u32 align(16),
    tmr5: u32 align(16),
    tmr6: u32 align(16),
    tmr7: u32 align(16),
    irr0: u32 align(16),
    irr1: u32 align(16),
    irr2: u32 align(16),
    irr3: u32 align(16),
    irr4: u32 align(16),
    irr5: u32 align(16),
    irr6: u32 align(16),
    irr7: u32 align(16),
    err_status: u32 align(16),
    rsvd2: [0x60]u8 align(16),
    cmci: u32 align(16),
    icr0: u32 align(16),
    icr1: u32 align(16),
    lvt: extern struct {
        timer: u32 align(16),
        thermal: u32 align(16),
        perf: u32 align(16),
        lint0: u32 align(16),
        lint1: u32 align(16),
        err: u32 align(16),
    } align(16),
    timer_initial_count: u32 align(16),
    timer_current_count: u32 align(16),
    rsvd3: [0xc0]u8 align(16),
    timer_divide_conf: u32 align(16),
    rsvd4: u32 align(16),
};

pub var lapic_regs: ?*volatile LAPICRegisters = null;

fn enableAPIC() virtual.MapError!void {
    const APICBaseMSR = packed struct {
        rsvd0: u8,
        bsp: bool,
        rsvd1: u2,
        enable: bool,
        addr: u52,
    };

    const apic_base_msr: msr.MSR = 0x1b;

    var base: APICBaseMSR = @bitCast(msr.getMSR(apic_base_msr));
    base.enable = true;
    msr.setMSR(apic_base_msr, @bitCast(base));
    const base_ptr: [*]u8 = @ptrFromInt(@as(usize, @intCast(base.addr << 12)));
    console.print("APIC register space at {*}.\n", .{base_ptr});
    const vbase_slice: []u8 = try virtual.mapPhysObj(
        base_ptr[0..@sizeOf(LAPICRegisters)],
        .{ .cache_mode = .Uncacheable },
    );
    lapic_regs = @ptrCast(@alignCast(vbase_slice.ptr));
}

pub const LAPICInitError = error{LAPICUninit} || std.mem.Allocator.Error;

/// This function must be called once from each CPU. It initializes the local
/// APIC for that core specifically, as compared to the boot module which sets
/// up and enables APIC functionality globally.
pub fn initLocalAPIC(gpa: std.mem.Allocator) LAPICInitError!void {
    _ = lapic_regs orelse return error.LAPICUninit;
    lapic_regs.?.spurious_int.apic_software_enable = true;
    try cpu.cpus.append(gpa, .{
        .lapic_id = @intCast(lapic_regs.?.lapic_id),
        .queue = .{
            .sync_status = .available,
            .active = try gpa.create(scheduler.Task),
            .list = null,
        },
    });
    lapic_regs.?.timer_initial_count = 0;
    lapic_regs.?.task_priority = 0;
}

pub fn eoi() void {
    lapic_regs.?.eoi = 0;
}

fn init() modules.ModuleInitError!void {
    if (!cpuid.cpu_features.?.apic) return error.ModuleUnsupported;

    const gpa = heap.allocator() catch return error.ModuleInitFailure;
    var lapic_entries = madt.findMADTEntries(
        madt.MADTEntryLAPIC,
        gpa,
    ) catch return error.ModuleInitFailure;
    defer lapic_entries.deinit(gpa);
    for (lapic_entries.items) |entry| {
        console.print("LAPIC: {}\n", .{entry.*});
    }

    enableAPIC() catch return error.ModuleInitFailure;

    initLocalAPIC(gpa) catch unreachable;
}

pub var mod: modules.Module = .{
    .name = "lapic",
    .init = init,
    .deps = &.{ &madt.mod, &msr.mod, &pic.mod, &cpuid.mod },
};
