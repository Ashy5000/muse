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

const modules = @import("../../modules.zig");

const CommandError = virtual.MapError || modules.InitError;

fn identify(
    main: io.Port,
    control: io.Port,
    select: DriveSel,
) CommandError!?IdentifyInfo {
    io.out(
        8,
        main + @intFromEnum(RegisterMain.drive_select),
        @intFromEnum(select),
    );
    io.out(8, main + @intFromEnum(RegisterMain.sector_count), 0);
    io.out(8, main + @intFromEnum(RegisterMain.lba_lo), 0);
    io.out(8, main + @intFromEnum(RegisterMain.lba_mid), 0);
    io.out(8, main + @intFromEnum(RegisterMain.lba_hi), 0);
    io.out(
        8,
        main + @intFromEnum(RegisterMain.status_cmd),
        @intFromEnum(Command.identify),
    );
    if (io.in(8, control + @intFromEnum(RegisterControl.status_cmd)) == 0) {
        return null;
    }
    while (@as(RegStatus, @bitCast(
        io.in(8, control + @intFromEnum(RegisterControl.status_cmd)),
    )).bsy) {
        try scheduler.preempt();
    }
    if (io.in(8, main + @intFromEnum(RegisterMain.lba_mid)) != 0) {
        return null;
    }
    if (io.in(8, main + @intFromEnum(RegisterMain.lba_hi)) != 0) {
        return null;
    }
    while (poll: {
        const status: RegStatus = @bitCast(io.in(
            8,
            control + @intFromEnum(RegisterControl.status_cmd),
        ));
        if (status.err) {
            return null;
        }
        break :poll !status.drq;
    }) {
        try scheduler.preempt();
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
    master: ?Drive,
    slave: ?Drive,
    int_flag: bool,
};

fn initChannel(
    main: io.Port,
    control: io.Port,
    busmaster_reg: pci.PCIDev.BAR,
    which: enum {
        primary,
        secondary,
    },
) CommandError!void {
    const prdt_phys: [*]align(paging.page_size) u8 = try pmm.pmmAllocLow(
        prdt_size,
    );
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
        .prdt = prdt_virt,
        .int_flag = false,
    };

    // Software reset
    io.out(8, channel.control + @intFromEnum(RegisterControl.status_cmd), 0x4);
    for (0..100) |_| _ = io.in(8, channel.control + @intFromEnum(RegisterControl.status_cmd));
    io.out(8, channel.control + @intFromEnum(RegisterControl.status_cmd), 0x0);

    const master_info = try identify(main, control, .master);
    if (master_info) |info| {
        console.print("Info: {}.\n", .{info});
        channel.master = .{ .info = info };
        channel.busmaster_reg.write(8, 0x4, @truncate(@intFromPtr(prdt_phys)));
        channel.busmaster_reg.write(8, 0x5, @truncate(@intFromPtr(prdt_phys) >> 4));
        channel.busmaster_reg.write(8, 0x6, @truncate(@intFromPtr(prdt_phys) >> 8));
        channel.busmaster_reg.write(8, 0x7, @truncate(@intFromPtr(prdt_phys) >> 12));
    }
    const slave_info = try identify(main, control, .slave);
    if (slave_info) |info| {
        channel.slave = .{ .info = info };
        channel.busmaster_reg.write(8, 0xc, @truncate(@intFromPtr(prdt_phys)));
        channel.busmaster_reg.write(8, 0xd, @truncate(@intFromPtr(prdt_phys) >> 4));
        channel.busmaster_reg.write(8, 0xe, @truncate(@intFromPtr(prdt_phys) >> 8));
        channel.busmaster_reg.write(8, 0xf, @truncate(@intFromPtr(prdt_phys) >> 12));
    }
    switch (which) {
        .primary => channel_primary = channel,
        .secondary => channel_secondary = channel,
    }
}

const TransferDir = enum(u1) { write, read };

const BusmasterCommandByte = packed struct(u8) {
    enable: bool,
    rsvd0: u2 = 0,
    dir: TransferDir,
    rsvd1: u4 = 0,
};

const BusmasterStatusByte = packed struct(u8) {
    enable: bool = false,
    err: bool = false,
    irq: bool = false,
    rsvd: u2 = 0,
    master_dma_supported: bool = false,
    slave_dma_supported: bool = false,
    simplex_only: bool = false,
};

fn fillPRDT(channel: *const Channel, batch_p: []u8) virtual.MapError!void {
    const frames = @import("../../alloc/frames.zig");

    var batch = batch_p;

    var i: usize = 0;
    while (batch.len > 0) : (i += 1) {
        const seg_len: u16 = @truncate(@min(batch.len, 0x10000));
        const seg = batch[0..seg_len];
        batch = batch[seg_len..];
        if (channel.prdt[i].buf_paddr == 0) {
            channel.prdt[i].buf_paddr = @intCast(
                @intFromPtr(try pmm.pmmAllocLow(
                    std.mem.Alignment.forward(paging.page_align, seg_len),
                )),
            );
        }
        const pg_cnt = (seg_len + paging.page_size - 1) / paging.page_size;
        const bfr = try frames.frameAllocContig(pg_cnt);
        const vr: virtual.Vregion = .{
            .paddr = @ptrFromInt(channel.prdt[i].buf_paddr),
            .vaddr = bfr.ptr,
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

fn sendDMACommands(
    channel: *const Channel,
    sel: DriveSel,
    sector_count_p: u16,
    dir: TransferDir,
    lba: u48,
) modules.InitError!void {
    channel.busmaster_reg.write(8, 0x0, @bitCast(@as(
        BusmasterCommandByte,
        .{ .enable = false, .dir = dir },
    )));

    // Set clear bits
    channel.busmaster_reg.write(8, 0x2, @bitCast(@as(BusmasterStatusByte, .{
        .err = true,
        .irq = true,
    })));

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

        console.print("Enabling DMA...\n", .{});
        channel.busmaster_reg.write(8, 0x0, @bitCast(@as(
            BusmasterCommandByte,
            .{ .enable = true, .dir = dir },
        )));

        while (!@atomicLoad(bool, &channel.int_flag, .unordered)) {
            // TODO: take this task off of the queue and store it
            // somewhere else. The ISR will push it back.
            try scheduler.preempt();
        }
    }
    console.print("DMA finished.\n", .{});

    var command_byte: BusmasterCommandByte = @bitCast(
        channel.busmaster_reg.read(8, 0x0),
    );
    command_byte.enable = false;
    channel.busmaster_reg.write(8, 0x0, @bitCast(command_byte));
}

fn doDMA(
    channel: *Channel,
    sel: DriveSel,
    data_p: []u8,
    dir: TransferDir,
    lba_p: u48,
) CommandError!void {
    channel.int_flag = false;
    var data = data_p;
    var lba = lba_p;
    while (data.len > 0) {
        const batch_len: u16 = @intCast(
            @min(data.len, prdt_size / @sizeOf(PRD) * 0x10000),
        );
        const batch = data[0..batch_len];
        data = data[batch_len..];

        try fillPRDT(channel, batch);
        try sendDMACommands(
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
const lapic = @import("../../smp/lapic.zig");

fn isrPrimary() callconv(idt.int_callconv) void {
    defer lapic.eoi();
    const channel = &(channel_primary orelse return);
    const status: BusmasterStatusByte = @bitCast(
        channel.busmaster_reg.read(8, 0x2),
    );
    console.print("Status: {}.\n", .{status});
    // @atomicStore(
    //     bool,
    //     &channel.int_flag,
    //     true,
    //     .unordered,
    // );
}

fn isrSecondary() callconv(idt.int_callconv) void {
    defer lapic.eoi();
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

    var command: pci.PCIDev.Command = dev.getCommand();
    command.bus_master = true;
    command.io_space = true;
    command.mem_space = true;
    command.interrupt_disable = false;
    dev.setCommand(command);

    const prog_if: ATAProgIf = @bitCast(dev.class.prog_if);
    _ = init: {
        const active_cpu = cpu.getActiveCPU() catch |err| break :init err;
        if (!prog_if.primary_pci) {
            const vec = interrupts.alloc(isrPrimary) catch |err| break :init err;
            _ = ioapic.alloc(
                1 << 14,
                vec,
                .active_high,
                .edge_sensitive,
                active_cpu.*,
            ) catch |err| break :init err;
            initChannel(
                0x1f0,
                0x3f6,
                busmaster_reg.enhance(8) catch |err| break :init err,
                .primary,
            ) catch |err| break :init err;
            var meow: [512]u8 = @splat(0xda);
            doDMA(&channel_primary.?, .master, &meow, .read, 25) catch unreachable;
            console.hexdump(&meow);
        }
        if (!prog_if.secondary_pci) {
            const vec = interrupts.alloc(isrPrimary) catch |err| break :init err;
            _ = ioapic.alloc(
                1 << 14,
                vec,
                .active_high,
                .edge_sensitive,
                active_cpu.*,
            ) catch |err| break :init err;
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
            _ = initChannel(
                0x170,
                0x376,
                busmaster_secondary.enhance(8) catch |err| break :init err,
                .primary,
            ) catch |err| break :init err;
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
