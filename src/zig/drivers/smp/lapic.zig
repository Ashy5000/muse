const heap = @import("../../alloc/heap.zig");
const madt = @import("madt.zig");
const console = @import("../../console.zig");
const virtual = @import("../../virtual.zig");
const msr = @import("../../utils/msr.zig");
const modules = @import("../../modules.zig");

const SpuriousInterruptReg = packed struct(u32) {
    vec: u8,
    apic_software_enable: bool,
    focus_processor_checking: bool,
    rsvd0: u2,
    eoi_brodcast_suppression: bool,
    rsvd1: u19,
};

const LAPICRegisters = extern struct {
    rsvd0: [16]u8 align(8),
    lapic_id: u32 align(8),
    version: u32 align(8),
    rsvd1: [32]u8 align(8),
    task_priority: u32 align(8),
    arbitration_priority: u32 align(8),
    processor_priority: u32 align(8),
    eoi: u32 align(8),
    remote_read: u32 align(8),
    logical_dest: u32 align(8),
    dest_format: u32 align(8),
    spurious_int: SpuriousInterruptReg align(8),
    // There are some more fields, but we don't need them right now, and
    // the weird alignments mean that they will just clutter up the struct.
};

var lapic_regs: ?*volatile LAPICRegisters = null;

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
    const base_ptr: [*]u8 = @ptrFromInt(base.addr << 12);
    console.print("APIC register space at {*}.\n", .{base_ptr});
    const vbase_slice: []u8 = try virtual.mapPhysObj(base_ptr[0..@sizeOf(LAPICRegisters)], .{ .cache_mode = .Uncacheable });
    lapic_regs = @alignCast(@ptrCast(vbase_slice.ptr));
}

pub const LAPICInitError = error{LAPICUninit};

/// This function must be called once from each CPU. It initializes the local
/// APIC for that core specifically, as compared to the boot module which sets
/// up and enables APIC functionality globally.
pub fn initLocalAPIC() LAPICInitError!void {
    const regs = lapic_regs orelse return error.LAPICUninit;
    regs.spurious_int.apic_software_enable = true;
}

fn init() modules.ModuleInitError!void {
    const gpa = heap.allocator() catch return error.ModuleInitFailure;
    const lapic_entries = madt.findMADTEntries(madt.MADTEntryLAPIC, gpa) catch return error.ModuleInitFailure;
    for (lapic_entries) |entry| {
        console.print("LAPIC: {}\n", .{entry.*});
    }

    enableAPIC() catch return error.ModuleInitFailure;

    initLocalAPIC() catch unreachable;
}

pub var mod: modules.Module = .{
    .name = "lapic",
    .init = init,
    .deps = &.{ &madt.mod, &msr.mod },
};
