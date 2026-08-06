const pci = @import("../../subsystems/pci.zig");
const console = @import("../../console.zig");
const io = @import("../../utils/io.zig");
const scheduler = @import("../../scheduler.zig");
const interrupts = @import("../../interrupts.zig");
const ioapic = @import("../ioapic.zig");
const cpu = @import("../../smp/cpu.zig");
const pmm = @import("../../alloc/pmm.zig");
const paging = @import("../../arch.zig").paging;
const virtual = @import("../../virtual.zig");

const Register = enum(io.Port) {
    data,
    err_features,
    sector_count,
    lba_lo,
    lba_mid,
    lba_hi,
    drive_select,
    status_cmd,
};

const RegStatus = packed struct {
    err: bool,
    idx: bool,
    corr: bool,
    drq: bool,
    srv: bool,
    df: bool,
    rdy: bool,
    bsy: bool,
};

const DriveSel = enum(u8) {
    master = 0xa0,
    slave = 0xb0,
};

const Command = enum(u8) {
    identify = 0xec,
};

const IdentifyInfo = extern struct {
    unused0: [60]u16,
    lba28_sectors: u32,
    unused1: [21]u16,
    lba48: packed struct(u16) {
        unused0: u10,
        supported: bool,
        unused1: u5,
    },
    unused2: [4]u16,
    udma: packed struct(u16) {
        supported: u8,
        active: u8,
    },
    unused3: [4]u16,
    conductor: packed struct(u16) {
        unused0: u11,
        @"80": bool,
        unused1: u4,
    },
    unused4: [6]u16,
    lba48_sectors: u64,
    unused5: [152]u16,
};

fn identify(main: io.Port, select: DriveSel) ?IdentifyInfo {
    io.out8(main + @intFromEnum(Register.drive_select), @intFromEnum(select));
    io.out8(main + @intFromEnum(Register.sector_count), 0);
    io.out8(main + @intFromEnum(Register.lba_lo), 0);
    io.out8(main + @intFromEnum(Register.lba_mid), 0);
    io.out8(main + @intFromEnum(Register.lba_hi), 0);
    io.out8(main + @intFromEnum(Register.status_cmd), @intFromEnum(Command.identify));
    if (io.in8(main + @intFromEnum(Register.status_cmd)) == 0) {
        return null;
    }
    while (@as(RegStatus, @bitCast(
        io.in8(main + @intFromEnum(Register.status_cmd)),
    )).bsy) {
        scheduler.preempt();
    }
    if (io.in8(main + @intFromEnum(Register.lba_mid)) != 0) {
        return null;
    }
    if (io.in8(main + @intFromEnum(Register.lba_hi)) != 0) {
        return null;
    }
    while (poll: {
        const status: RegStatus = @bitCast(io.in8(main + @intFromEnum(Register.status_cmd)));
        if (status.err) {
            return null;
        }
        break :poll !status.drq;
    }) {
        scheduler.preempt();
    }
    var res: [256]u16 = undefined;
    for (&res) |*field| {
        field.* = io.in16(main + @intFromEnum(Register.data));
    }
    return @bitCast(res);
}

const PRD = packed struct {
    buf_paddr: u32,
    transfer_size: u16,
    rsvd: u15,
    last_entry: bool,
};

const Drive = struct { info: IdentifyInfo };

const Channel = struct {
    main: io.Port,
    control: io.Port,
    busmaster_reg: pci.PCIDev.BAR,
    prdt: []PRD,
    prdt_phys: u32,
    master: ?Drive,
    slave: ?Drive,
};

fn init_channel(main: io.Port, control: io.Port, busmaster_reg: pci.PCIDev.BAR) virtual.MapError!Channel {
    const prdt_phys: u32 = @truncate(try pmm.pmmAlloc(paging.page_size));
    const prdt_virt: []PRD = @alignCast(@ptrCast(try virtual.mapPhysObj(
        @as([*]u8, @ptrFromInt(prdt_phys))[0..paging.page_size],
        .{ .cache_mode = .WriteCombining }, // TODO: Rethink cache mode
    )));
    var channel: Channel = .{
        .main = main,
        .control = control,
        .busmaster_reg = busmaster_reg,
        .master = null,
        .slave = null,
        .prdt_phys = prdt_phys,
        .prdt = prdt_virt,
    };
    const master_info = identify(main, .master);
    if (master_info) |info| {
        channel.master = .{ .info = info };
    }
    const slave_info = identify(main, .slave);
    if (slave_info) |info| {
        channel.slave = .{ .info = info };
    }
    return channel;
}

var int_primary: bool = false;
var int_secondary: bool = false;

fn isr_primary() void {
    int_primary = true;
}

fn isr_secondary() void {
    int_secondary = true;
}

fn init(dev: *pci.PCIDev) void {
    console.print("Dev: .{}.\n", .{dev.*});
    const ATAProgIf = packed struct {
        primary_pci: bool,
        primary_switch: bool,
        secondary_pci: bool,
        secondary_switch: bool,
        rsvd: u3,
        bus_master: bool,
    };
    const busmaster_reg: pci.PCIDev.BAR = reg: for (dev.bars.?) |bar| {
        if (bar.idx == 4) {
            break :reg bar;
        }
    } else {
        return; // todo: support PIO mode
    };
    const prog_if: ATAProgIf = @bitCast(dev.class.prog_if);
    _ = init: {
        if (!prog_if.primary_pci) {
            const vec_primary = interrupts.alloc(@ptrCast(&isr_primary)) orelse return; // TODO: fall back to polling
            if (ioapic.alloc(
                1 << 14,
                vec_primary,
                .active_high,
                .edge_sensitive,
                cpu.getActiveCPU().*,
            )) |_| {
                _ = init_channel(0x1f0, 0x3f6, busmaster_reg) catch |err| break :init err;
            }
        }
        if (!prog_if.secondary_pci) {
            const vec_secondary = interrupts.alloc(@ptrCast(&isr_secondary)) orelse return; // TODO: fall back to polling
            if (ioapic.alloc(
                1 << 15,
                vec_secondary,
                .active_high,
                .edge_sensitive,
                cpu.getActiveCPU().*,
            )) |_| {
                _ = init_channel(0x170, 0x376, busmaster_reg) catch |err| break :init err;
            }
        }
    } catch |err| @import("std").debug.panic(
        "critical system error while initializing ATA: {s}",
        .{@errorName(err)},
    );
}

pub var driver_pci: pci.Driver = .{
    .init = init,
    .class_major = .mass_storage_controller,
    .class_minor = 0x1,
};
