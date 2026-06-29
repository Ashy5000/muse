const std = @import("std");
const io = @import("utils/io.zig");
const console = @import("console.zig");
const heap = @import("alloc/heap.zig");
const modules = @import("modules.zig");

const pci_config_addr: io.Port = 0xcf8;
const pci_config_data: io.Port = 0xcfc;

const PCIAddr = packed struct(u32) {
    reg_offset: u8 = 0,
    func: u3,
    slot: u5,
    bus: u8,
    rsvd: u7 = 0,
    enable: bool = true,
};

fn PCIConfigRead(dev: PCIDev, offset: u8) u32 {
    const addr: PCIAddr = .{
        .bus = dev.bus,
        .slot = dev.slot,
        .func = dev.func,
        .reg_offset = offset,
    };
    io.out32(pci_config_addr, @bitCast(addr));
    return io.in32(pci_config_data);
}

const PCIClassMajor = enum(u8) {
    unclassified,
    mass_storage_controller,
    network_controller,
    display_controller,
    multimedia_controller,
    memory_controller,
    bridge,
    simple_communication_controller,
    base_system_peripheral,
    input_device_controller,
    docking_station,
    processor,
    serial_bus_controller,
    wireless_controller,
    intelligent_controller,
    satellite_communication_controller,
    encryption_controller,
    signal_processing_controller,
    processing_accelerator,
    non_essential_instrumentation,
    coprocessor = 0x40,
    unassigned = 0xFF,
};

const PCIVendor = enum(u16) { intel = 0x8086, _ };

fn getVendor(dev: PCIDev) PCIVendor {
    return @enumFromInt(PCIConfigRead(dev, 0x0) & 0xffff);
}

const PCIType = enum(u7) {
    general,
    bridge_pci_pci,
    bridge_pci_cardbus,
};

const PCITypeByte = packed struct(u8) {
    type: PCIType,
    multi_function: bool,
};

fn getTypeByte(dev: PCIDev) PCITypeByte {
    return @bitCast(@as(u8, @truncate(PCIConfigRead(dev, 0xc) >> 16)));
}

fn getSecondaryBus(dev: PCIDev) u8 {
    return @truncate(PCIConfigRead(dev, 0x18) >> 8);
}

const PCIClass = struct {
    major: PCIClassMajor,
    minor: u8,
    prog_if: u8,
};

fn getClassMajor(dev: PCIDev) PCIClassMajor {
    return @enumFromInt(PCIConfigRead(dev, 0x8) >> 24);
}

fn getClassMinor(dev: PCIDev) u8 {
    return @intCast((PCIConfigRead(dev, 0x8) >> 16) & 0xff);
}

fn getProgIF(dev: PCIDev) u8 {
    return @intCast((PCIConfigRead(dev, 0x8) >> 8) & 0xff);
}

const BARMemType = enum(u2) {
    @"32" = 0,
    @"64" = 2,
};

const BARMem = packed struct {
    bar_type: bool = false,
    bar_mem_type: BARMemType,
    prefetchable: bool,
    addr_hi: u28,
};

const BARIO = packed struct {
    bar_type: bool = true,
    rsvd: bool,
    addr_hi: u30,
};

const BAR = union(enum) {
    mem: BARMem,
    io: BARIO,
};

fn getBAR(dev: PCIDev, idx: u8) ?BAR {
    const bar_int: u32 = PCIConfigRead(dev, 0x10 + 0x4 * idx);
    if (bar_int == 0) {
        return null;
    }
    if ((bar_int & 0x1) == 0) {
        // Mem
        return .{ .mem = @bitCast(bar_int) };
    } else {
        // IO
        return .{ .io = @bitCast(bar_int) };
    }
}

const PCIDev = struct {
    bus: u8,
    slot: u5,
    func: u3,
    class: PCIClass,
    vendor: PCIVendor,
    multi_function: bool,
    bars: ?[]BAR,
};

fn scanPCIFunc(bus: u8, slot: u5, func: u3, allocator: std.mem.Allocator) std.mem.Allocator.Error!?PCIDev {
    var dev: PCIDev = undefined;
    dev.bus = bus;
    dev.slot = slot;
    dev.func = func;
    dev.vendor = getVendor(dev);
    if (@intFromEnum(dev.vendor) == 0xffff) {
        return null;
    }
    dev.class.major = getClassMajor(dev);
    dev.class.minor = getClassMinor(dev);
    dev.class.prog_if = getProgIF(dev);
    const type_byte: PCITypeByte = getTypeByte(dev);
    dev.multi_function = type_byte.multi_function;
    if (type_byte.type == .bridge_pci_pci) {
        try scanPCIBus(getSecondaryBus(dev), allocator);
    }
    console.print("Found PCI {s} on bus {}, slot {}, func {}. Vendor: {x}\n", .{ @tagName(dev.class.major), bus, slot, func, dev.vendor });

    dev.bars = null;
    if (type_byte.type != .bridge_pci_cardbus) {
        const bar_cnt: usize = if (type_byte.type == .general) 6 else 2;
        var bars: [6]BAR = @splat(undefined);
        var present_bar_cnt: usize = 0;
        for (0..bar_cnt) |i| {
            const bar = getBAR(dev, @intCast(i));
            if (bar) |b| {
                switch (b) {
                    .mem => |m| console.print("Found memory BAR with base 0x{x}.\n", .{@as(usize, m.addr_hi) << 4}),
                    .io => |m| console.print("Found I/O BAR with base 0x{x}.\n", .{@as(usize, m.addr_hi) << 2}),
                }
                bars[present_bar_cnt] = b;
                present_bar_cnt += 1;
            }
        }
        const present_bars = try allocator.alloc(BAR, present_bar_cnt);
        @memcpy(present_bars, bars[0..present_bar_cnt]);
        dev.bars = present_bars;
    }
    return dev;
}

fn scanPCIDev(bus: u8, slot: u5, allocator: std.mem.Allocator) std.mem.Allocator.Error!u3 {
    const first_func = try scanPCIFunc(bus, slot, 0, allocator) orelse return 0;
    var fn_cnt: u3 = 1;
    if (!first_func.multi_function) {
        return fn_cnt;
    }
    for (1..8) |func| {
        if (try scanPCIFunc(bus, slot, @intCast(func), allocator)) |_| {
            fn_cnt += 1;
        }
    }
    return fn_cnt;
}

fn scanPCIBus(bus: u8, allocator: std.mem.Allocator) std.mem.Allocator.Error!void {
    // The host bridge is already scanned by init().
    const start: usize = if (bus == 0) 1 else 0;
    for (start..32) |slot| {
        _ = try scanPCIDev(@intCast(bus), @intCast(slot), allocator);
    }
}

fn init() modules.ModuleInitError!void {
    const allocator = heap.allocator() catch return error.ModuleInitFailure;
    const bus_cnt: u8 = scanPCIDev(0, 0, allocator) catch return error.ModuleInitFailure;
    for (0..bus_cnt) |i| {
        scanPCIBus(@intCast(i), allocator) catch return error.ModuleInitFailure;
    }
}

pub var mod: modules.Module = .{
    .name = "pci",
    .init = init,
    .deps = &.{ &console.mod, &heap.mod },
};
