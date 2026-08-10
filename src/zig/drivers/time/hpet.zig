const sdt = @import("../../acpi/sdt.zig");
const acpi = @import("../../acpi/acpi.zig");
const virtual = @import("../../virtual.zig");
const lapic = @import("../../smp/lapic.zig");
const ioapic = @import("../ioapic.zig");
const console = @import("../../console.zig");
const timer = @import("../../subsystems/timer.zig");
const interrupts = @import("../../interrupts.zig");
const cpu = @import("../../smp/cpu.zig");
const tick = @import("../../tick.zig");
const idt = @import("../../arch.zig").idt;

const CounterSize = enum(u1) { @"32", @"64" };

const BlockID = packed struct(u32) {
    revision_id: u8,
    comparator_count: u5,
    counter_size: CounterSize,
    rsvd0: u1,
    legacy_replacement_supported: bool,
    pci_vendor_id: u16,
};

const HPET = extern struct {
    header: sdt.DefBlockHeader,
    event_timer_id: BlockID,
    address_space: enum(u8) { mem, io },
    register_width: u8,
    register_offset: u8,
    rsvd1: u8,
    addr: u64 align(4),
    hpet_number: u8,
    min_tick: u16,
    page_protection: u8,
};

const Registers = extern struct {
    general: extern struct {
        id: BlockID,
        period: u32,
    } align(16),
    config: packed struct(u64) {
        enable: bool,
        legacy_replacement_enable: bool,
        rsvd: u62,
    } align(16),
    int_status: packed struct(u64) {
        level_triggered_status: bool,
        rsvd: u63,
    } align(16),
    padding: [0xc8]u8,
    counter: u64 align(16),
    timers: [24]Timer align(16),

    const Timer = extern struct {
        info: packed struct(u64) {
            rsvd0: u1,
            int_type: ioapic.IOAPIC.TriggerMode,
            int_enable: bool,
            periodic_enable: bool,
            periodic_supported: bool,
            counter_size: CounterSize,
            periodic_accumulator_write: bool,
            rsvd1: u1,
            force_32_bit: bool,
            ioapic_irq: u5,
            fsb_enable: bool,
            fsb_supported: bool,
            rsvd: u16,
            ioapic_routing_map: u32,
        },
        comparator: u64,
        fsb_mapping: u64,
        padding: u64,
    };
};

var registers: *volatile Registers = undefined;
var sys_timer: ?*volatile Registers.Timer = null;

var target: u64 = 0;

fn hpet_tick() callconv(idt.int_callconv) void {
    switch (sys_timer.?.info.counter_size) {
        .@"32" => {
            sys_timer.?.comparator = @as(u32, @intCast(target));
            registers.config.enable = false;
            registers.counter = 0;
            registers.config.enable = true;
        },
        .@"64" => {
            target += driver_timer.period * 1000 / registers.general.period;
            sys_timer.?.comparator = target;
        },
    }
    tick.tick();
    lapic.eoi();
    asm volatile ("sti");
}

fn init() timer.Driver.InitError!bool {
    // Disable the PIT (this method is a little sketchy though)
    const io = @import("../../utils/io.zig");
    io.out(8, 0x43, 0b00110000);
    io.out(8, 0x40, 0x00);
    io.out(8, 0x40, 0x00);

    const table: *HPET = @ptrCast(acpi.findSDT("HPET") orelse return false);
    const regs_phys: [*]u8 = @ptrFromInt(@as(usize, @intCast(table.addr)));
    const regs_virt = try virtual.mapPhysObj(
        regs_phys[0..@sizeOf(Registers)],
        .{ .cache_mode = .Uncacheable },
    );
    registers = @ptrCast(@alignCast(regs_virt.ptr));
    registers.config.enable = false;
    for (0..registers.general.id.comparator_count + 1) |i| {
        registers.timers[i].info.int_enable = false;
        if (sys_timer) |_| {} else if (registers.timers[i].info.counter_size == .@"64") {
            sys_timer = &registers.timers[i];
        }
    }
    if (sys_timer) |_| {} else {
        sys_timer = &registers.timers[0];
    }
    console.print("Found HPET with period 0x{x}.\n", .{driver_timer.period});
    const vec = interrupts.alloc(@ptrCast(&hpet_tick)) orelse return error.IDTFull;
    console.print("{}", .{sys_timer.?.info.ioapic_routing_map});
    const irq = ioapic.alloc(
        sys_timer.?.info.ioapic_routing_map,
        vec,
        .active_high,
        .edge_sensitive,
        cpu.getActiveCPU().*,
    ) orelse return error.NoAvailableIRQs;
    sys_timer.?.info.ioapic_irq = irq;
    return true;
}

fn enable() void {
    registers.counter = 0;
    sys_timer.?.info.int_type = .edge_sensitive;
    sys_timer.?.info.int_enable = true;
    sys_timer.?.info.periodic_enable = false;
    sys_timer.?.info.force_32_bit = false;
    target = driver_timer.period * 1000 / registers.general.period;
    sys_timer.?.comparator = target;
    registers.config.enable = true;
}

pub var driver_timer: timer.Driver = .{
    .init = init,
    .enable = enable,
    .period = undefined,
};
