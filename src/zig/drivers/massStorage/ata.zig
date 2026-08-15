const std = @import("std");
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
const pic = @import("../../drivers/pic.zig");

const RegisterMain = enum(io.Port) {
    data,
    err_features,
    sector_count,
    lba_lo,
    lba_mid,
    lba_hi,
    drive_select,
    status_cmd,
};

const RegisterControl = enum(io.Port) {
    status_cmd,
    addr,
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
    dma_28_read = 0xc8,
    dma_28_write = 0xca,
    dma_48_read = 0x25,
    dma_48_write = 0x35,
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

fn identify(main: io.Port, control: io.Port, select: DriveSel) ?IdentifyInfo {
    io.out(8, main + @intFromEnum(RegisterMain.drive_select), @intFromEnum(select));
    io.out(8, main + @intFromEnum(RegisterMain.sector_count), 0);
    io.out(8, main + @intFromEnum(RegisterMain.lba_lo), 0);
    io.out(8, main + @intFromEnum(RegisterMain.lba_mid), 0);
    io.out(8, main + @intFromEnum(RegisterMain.lba_hi), 0);
    io.out(8, main + @intFromEnum(RegisterMain.status_cmd), @intFromEnum(Command.identify));
    if (io.in(8, control) == 0) {
        return null;
    }
    while (@as(RegStatus, @bitCast(
        io.in(8, control),
    )).bsy) {
        scheduler.preempt();
    }
    if (io.in(8, main + @intFromEnum(RegisterMain.lba_mid)) != 0) {
        return null;
    }
    if (io.in(8, main + @intFromEnum(RegisterMain.lba_hi)) != 0) {
        return null;
    }
    while (poll: {
        const status: RegStatus = @bitCast(io.in(8, control));
        if (status.err) {
            return null;
        }
        break :poll !status.drq;
    }) {
        scheduler.preempt();
    }
    var res: [256]u16 = undefined;
    for (&res) |*field| {
        field.* = io.in(16, main + @intFromEnum(RegisterMain.data));
    }
    return @bitCast(res);
}

const PRD = packed struct {
    buf_paddr: u32,
    transfer_size: u16,
    rsvd: u15 = 0,
    last_entry: bool,
};

const prdt_size: usize = paging.page_size;

const Drive = struct { info: IdentifyInfo };

const Channel = struct {
    main: io.Port,
    control: io.Port,
    busmaster_reg: pci.PCIDev.BAR,
    prdt: []PRD,
    prdt_phys: u32,
    master: ?Drive,
    slave: ?Drive,
    int_flag: bool,
};

fn init_channel(
    main: io.Port,
    control: io.Port,
    busmaster_reg: pci.PCIDev.BAR,
    which: enum {
        primary,
        secondary,
    },
) virtual.MapError!void {
    const prdt_phys: [*]align(paging.page_size) u8 = try pmm.pmmAllocLow(prdt_size);
    const prdt_virt: []PRD = @ptrCast(@alignCast(try virtual.mapPhysObj(
        prdt_phys[0..prdt_size],
        .{},
    )));
    @memset(prdt_virt, .{
        .buf_paddr = 0,
        .transfer_size = 0,
        .last_entry = false,
    });
    var channel: Channel = .{
        .main = main,
        .control = control,
        .busmaster_reg = busmaster_reg,
        .master = null,
        .slave = null,
        .prdt_phys = prdt_phys,
        .prdt = prdt_virt,
        .int_flag = false,
    };
    channel.busmaster_reg.write(32, 0x4, prdt_phys);
    const master_info = identify(main, control, .master);
    if (master_info) |info| {
        channel.master = .{ .info = info };
        var meow: [512]u8 = @splat(0);
        try do_dma(&channel, .master, &meow, .read, 123);
    }
    const slave_info = identify(main, control, .slave);
    if (slave_info) |info| {
        channel.slave = .{ .info = info };
    }
    switch (which) {
        .primary => channel_primary = channel,
        .secondary => channel_secondary = channel,
    }
}

const TransferDir = enum(u1) { write, read };

const BusmasterCommandByte = packed struct(u8) {
    enable: bool,
    dir: TransferDir,
    rsvd: u6 = 0,
};

fn fill_prdt(channel: *Channel, batch_p: []u8) virtual.MapError!void {
    const frames = @import("../../alloc/frames.zig");

    var batch = batch_p;

    var i: usize = 0;
    while (batch.len > 0) : (i += 1) {
        const seg_len: u16 = @truncate(@min(batch.len, 0x10000));
        const seg = batch[0..seg_len];
        batch = batch[seg_len..];
        if (channel.prdt[i].buf_paddr == 0) {
            channel.prdt[i].buf_paddr = try pmm.pmmAllocLow(
                std.mem.Alignment.forward(paging.page_align, seg_len),
            );
        }
        const pg_cnt = (seg_len + paging.page_size - 1) / paging.page_size;
        const bfr = try frames.frameAllocContig(pg_cnt);
        const vr: virtual.Vregion = .{
            .paddr = channel.prdt[i].buf_paddr,
            .vaddr = @intFromPtr(bfr.ptr),
            .pg_cnt = pg_cnt,
            .flags = .{},
            .next = null,
        };
        try paging.mapRegion(&vr);
        @memcpy(bfr[0..seg_len], seg);
        virtual.freeMappedObj(bfr);
        channel.prdt[i].transfer_size = seg_len;
        channel.prdt[i].last_entry = batch.len == 0;
    }
}

const sector_size: usize = 512;

fn send_dma_cmds(
    channel: *Channel,
    sel: DriveSel,
    sector_count_p: u16,
    dir: TransferDir,
    lba: u48,
) void {
    channel.busmaster_reg.write(8, 0x0, @bitCast(@as(
        BusmasterCommandByte,
        .{ .enable = false, .dir = dir },
    )));

    console.print("Set DMA transfer direction.\n", .{});

    var sector_count = sector_count_p;
    while (sector_count > 0) {
        const seg_sector_count = @min(sector_count, 0x10000 / sector_size);
        sector_count -= seg_sector_count;
        if (lba < 0xfffffff and seg_sector_count < 0xff) {
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.drive_select),
                @intFromEnum(sel) + @as(u8, @intCast(lba >> 24)),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.sector_count),
                @intCast(seg_sector_count),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.lba_lo),
                @truncate(lba),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.lba_mid),
                @truncate(lba >> 8),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.lba_hi),
                @truncate(lba >> 16),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.status_cmd),
                @intFromEnum(
                    if (dir == .read)
                        Command.dma_28_read
                    else
                        Command.dma_28_write,
                ),
            );
        } else {
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.drive_select),
                @intFromEnum(sel),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.sector_count),
                @intCast(@as(u16, seg_sector_count) >> @as(u4, 8)),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.lba_lo),
                @truncate(lba >> 24),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.lba_mid),
                @truncate(lba >> 32),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.lba_hi),
                @truncate(lba >> 40),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.sector_count),
                @truncate(seg_sector_count),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.lba_lo),
                @truncate(lba),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.lba_mid),
                @truncate(lba >> 8),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.lba_hi),
                @truncate(lba >> 16),
            );
            io.out(
                8,
                channel.main + @intFromEnum(RegisterMain.status_cmd),
                @intFromEnum(
                    if (dir == .read)
                        Command.dma_48_read
                    else
                        Command.dma_48_write,
                ),
            );
        }

        channel.busmaster_reg.write(8, 0x0, @bitCast(@as(
            BusmasterCommandByte,
            .{ .enable = true, .dir = dir },
        )));

        console.print("DMA command sent.\n", .{});

        while (!@atomicLoad(bool, &channel.int_flag, .unordered)) {
            // TODO: take this task off of the queue and store it
            // somewhere else. The ISR will push it back.
            scheduler.preempt();
        }
    }
    console.print("DMA finished.\n", .{});

    var command_byte: BusmasterCommandByte = @bitCast(
        channel.busmaster_reg.read(8, 0x0),
    );
    command_byte.enable = false;
    channel.busmaster_reg.write(8, 0x0, @bitCast(command_byte));
}

fn do_dma(
    channel: *Channel,
    sel: DriveSel,
    data_p: []u8,
    dir: TransferDir,
    lba_p: u48,
) virtual.MapError!void {
    channel.int_flag = false;
    var data = data_p;
    var lba = lba_p;
    while (data.len > 0) {
        const batch_len: u16 = @intCast(
            @min(data.len, prdt_size / @sizeOf(PRD) * 0x10000),
        );
        const batch = data[0..batch_len];
        data = data[batch_len..];

        try fill_prdt(channel, batch);
        send_dma_cmds(
            channel,
            sel,
            @intCast(batch_len / sector_size),
            dir,
            lba,
        );

        lba += @intCast(batch_len / sector_size);
    }
}

var channel_primary: ?Channel = null;
var channel_secondary: ?Channel = null;

const idt = @import("../../arch.zig").idt;

fn isr_primary() callconv(idt.int_callconv) void {
    @atomicStore(
        bool,
        &(channel_primary orelse return).int_flag,
        true,
        .unordered,
    );
}

fn isr_secondary() callconv(idt.int_callconv) void {
    @atomicStore(
        bool,
        &(channel_secondary orelse return).int_flag,
        true,
        .unordered,
    );
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
    const busmaster_reg: pci.PCIDev.BARRaw = reg: for (dev.bars.?) |bar| {
        if (bar.idx == 4) {
            break :reg bar;
        }
    } else {
        return; // todo: support PIO mode
    };
    const prog_if: ATAProgIf = @bitCast(dev.class.prog_if);
    _ = init: {
        if (!prog_if.primary_pci) {
            const vec_primary = (interrupts.alloc(isr_primary) catch |err| break :init err) orelse return; // TODO: fall back to polling
            if (ioapic.alloc(
                1 << 14,
                vec_primary,
                .active_high,
                .edge_sensitive,
                cpu.getActiveCPU().*,
            )) |_| {
                _ = init_channel(
                    0x1f0,
                    0x3f6,
                    busmaster_reg.enhance(8) catch |err| break :init err,
                    .primary,
                ) catch |err| break :init err;
            }
        }
        if (!prog_if.secondary_pci) {
            const vec_secondary = (interrupts.alloc(isr_primary) catch |err| break :init err) orelse return; // TODO: fall back to polling
            if (ioapic.alloc(
                1 << 15,
                vec_secondary,
                .active_high,
                .edge_sensitive,
                cpu.getActiveCPU().*,
            )) |_| {
                const busmaster_secondary: pci.PCIDev.BARRaw = .{
                    .idx = busmaster_reg.idx,
                    .data = switch (busmaster_reg.data) {
                        .io => |data_io| .{
                            .io = .{ .port = data_io.port + 0x8 },
                        },
                        .mem => |data_mem| .{ .mem = .{
                            .paddr = data_mem.paddr + 0x8,
                            .prefetchable = data_mem.prefetchable,
                            .mem_type = data_mem.mem_type,
                        } },
                    },
                };
                _ = init_channel(
                    0x170,
                    0x376,
                    busmaster_secondary.enhance(8) catch |err| break :init err,
                    .primary,
                ) catch |err| break :init err;
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
