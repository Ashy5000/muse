const std = @import("std");
const io = @import("../utils/io.zig");
const console = @import("../console.zig");
const heap = @import("../alloc/heap.zig");
const modules = @import("../modules.zig");

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

const PCIType = enum(u7) {
    general,
    bridge_pci_pci,
    bridge_pci_cardbus,
};

const PCITypeByte = packed struct(u8) {
    type: PCIType,
    multi_function: bool,
};

const PCIClass = struct {
    major: PCIClassMajor,
    minor: u8,
    prog_if: u8,
};

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

const BARMemResolved = struct {
    bar_mem_type: BARMemType,
    prefetchable: bool,
    addr: usize,
};

const BARIOResolved = struct {
    addr: usize,
};

const BAR = union(enum) {
    mem: BARMemResolved,
    io: BARIOResolved,
};

const PCIDev = struct {
    bus: u8,
    slot: u5,
    func: u3,
    class: PCIClass,
    vendor: PCIVendor,
    type_byte: PCITypeByte,
    bars: ?[]BAR,

    fn readConfigSpace(dev: *const PCIDev, offset: u8) u32 {
        const addr: PCIAddr = .{
            .bus = dev.bus,
            .slot = dev.slot,
            .func = dev.func,
            .reg_offset = offset,
        };
        io.out32(pci_config_addr, @bitCast(addr));
        return io.in32(pci_config_data);
    }

    fn getVendor(dev: *PCIDev) void {
        dev.vendor = @enumFromInt(dev.readConfigSpace(0x0) & 0xffff);
    }

    fn getTypeByte(dev: *PCIDev) void {
        dev.type_byte = @bitCast(@as(u8, @truncate(dev.readConfigSpace(0xc) >> 16)));
    }

    fn getSecondaryBus(dev: *const PCIDev) ?u8 {
        if (dev.type_byte.type != .bridge_pci_pci) {
            @branchHint(.cold);
            return null;
        }
        return @truncate(dev.readConfigSpace(0x18) >> 8);
    }

    fn getClassMajor(dev: *PCIDev) void {
        dev.class.major = @enumFromInt(dev.readConfigSpace(0x8) >> 24);
    }

    fn getClassMinor(dev: *PCIDev) void {
        dev.class.minor = @intCast((dev.readConfigSpace(0x8) >> 16) & 0xff);
    }

    fn getProgIF(dev: *PCIDev) void {
        dev.class.prog_if = @intCast((dev.readConfigSpace(0x8) >> 8) & 0xff);
    }

    fn getBAR(dev: *const PCIDev, idx: u8) ?BAR {
        const bar_int: u32 = dev.readConfigSpace(0x10 + 0x4 * idx);
        if (bar_int == 0) {
            return null;
        }
        if ((bar_int & 0x1) == 0) {
            // Mem
            const bar_mem: BARMem = @bitCast(bar_int);
            var res: BAR = .{ .mem = .{
                .bar_mem_type = bar_mem.bar_mem_type,
                .prefetchable = bar_mem.prefetchable,
                .addr = @as(usize, bar_mem.addr_hi) << 4,
            } };
            if (res.mem.bar_mem_type == .@"64") {
                const addr_ext: usize = dev.readConfigSpace(0x10 + 0x4 * (idx + 1));
                // We can be sure we won't get a 64-bit memory address on a 32-bit machine,
                // so we can safely store addresses with type `usize`, and only parse 64-bit
                // BARs when `usize` is at least 64 bits wide.
                if (@bitSizeOf(usize) >= 64) {
                    res.mem.addr |= addr_ext << 32;
                }
            }
        }
        // IO
        const bar_io: BARIO = @bitCast(bar_int);
        return .{ .io = .{
            .addr = @as(u32, bar_io.addr_hi) << 2,
        } };
    }
};

fn scanPCIFunc(bus: u8, slot: u5, func: u3, allocator: std.mem.Allocator) std.mem.Allocator.Error!?PCIDev {
    var dev: PCIDev = undefined;
    dev.bus = bus;
    dev.slot = slot;
    dev.func = func;
    dev.getVendor();
    if (@intFromEnum(dev.vendor) == 0xffff) {
        return null;
    }
    dev.getClassMajor();
    dev.getClassMinor();
    dev.getProgIF();
    dev.getTypeByte();
    if (dev.type_byte.type == .bridge_pci_pci) {
        try scanPCIBus(dev.getSecondaryBus().?, allocator);
    }
    console.print("Found PCI {s} on bus {}, slot {}, func {}. Vendor: {x}\n", .{ @tagName(dev.class.major), bus, slot, func, dev.vendor });

    dev.bars = null;
    if (dev.type_byte.type != .bridge_pci_cardbus) {
        const bar_cnt: usize = if (dev.type_byte.type == .general) 6 else 2;
        var bars: [6]BAR = @splat(undefined);
        var present_bar_cnt: u8 = 0;
        var i: usize = 0;
        while (i < bar_cnt) : (i += 1) {
            const bar = dev.getBAR(@intCast(i));
            if (bar) |b| {
                switch (b) {
                    .mem => |m| {
                        console.print("Found memory BAR with base 0x{x}.\n", .{m.addr});
                        // 64-bit memory BARs take up 2 entries, so skip an extra 1.
                        if (m.bar_mem_type == .@"64") {
                            i += 1;
                        }
                    },
                    .io => |m| console.print("Found I/O BAR with base 0x{x}.\n", .{m.addr}),
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
    if (!first_func.type_byte.multi_function) {
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
