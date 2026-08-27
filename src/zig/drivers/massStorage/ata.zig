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
    active: ?DriveSel,
    status: enum { Pending, Done, Failed },
};

const vfs = @import("../../vfs.zig");
const heap = @import("../../alloc/heap.zig");

const Payload = struct { channel: *Channel, sel: DriveSel };

fn transfer(
    vnode: *vfs.Vnode,
    data: []u8,
    dir_p: vfs.Vnode.Direction,
    offset: usize,
) vfs.Vnode.TransferError!void {
    const dir: TransferDir = switch (dir_p) {
        .read => .read,
        .write => .write,
    };
    const payload: *Payload = @ptrCast(@alignCast(vnode.data.file.payload));
    const sector_align = std.mem.Alignment.fromByteUnits(sector_size);
    const lba_start = offset / sector_size;

    const data_start = sector_align.forward(offset);
    const data_end = sector_align.backward(offset + data.len);
    const use_data = data_end > data_start;

    const pre_start = sector_align.backward(offset);
    const pre_end = data_start;
    const use_pre = pre_end > pre_start;

    const post_start = data_end;
    const post_end = sector_align.forward(offset + data.len);
    const use_post = post_end > post_start and post_start != pre_start;

    const gpa = heap.mod.data() catch return error.CriticalSystemFailure;
    const extra_bfrs: [][sector_size]u8 = gpa.alloc(
        [sector_size]u8,
        @as(
            usize,
            if (use_pre) 1 else 0,
        ) + @as(
            usize,
            if (use_post) 1 else 0,
        ),
    ) catch return error.CriticalSystemFailure;
    defer gpa.free(extra_bfrs);

    switch (dir) {
        .read => {
            var bfrs: [][]u8 = gpa.alloc(
                []u8,
                extra_bfrs.len + @as(usize, if (use_data) 1 else 0),
            ) catch return error.CriticalSystemFailure;
            defer gpa.free(bfrs);

            var idx: usize = 0;
            if (use_pre) {
                bfrs[idx] = &extra_bfrs[0];
                idx += 1;
            }
            if (use_data) {
                bfrs[idx] = data[data_start - offset .. data_end - offset];
                idx += 1;
            }
            if (use_post) {
                bfrs[idx] = &extra_bfrs[extra_bfrs.len - 1];
                idx += 1;
            }

            doDMA(
                payload.channel,
                payload.sel,
                bfrs,
                .read,
                @intCast(lba_start),
            ) catch |err| switch (err) {
                error.IOFailed => return error.IOFailed,
                else => return error.CriticalSystemFailure,
            };

            if (use_pre) {
                const len = @min(data.len, data_start - offset);
                @memcpy(
                    data[0..len],
                    extra_bfrs[0][offset - pre_start ..][0..len],
                );
            }
            if (use_post) {
                @memcpy(
                    data[data_end - offset ..],
                    extra_bfrs[extra_bfrs.len - 1][0 .. (offset + data.len) % sector_size],
                );
            }
        },
        .write => std.debug.panic("ATA write unsupported", .{}),
    }
}

const InitError = CommandError || std.mem.Allocator.Error;

fn initDrive(
    channel: *Channel,
    sel: DriveSel,
    prdt_phys: [*]align(paging.page_size) u8,
    root: *vfs.Vnode,
) InitError!void {
    const identify_info = try identify(channel.main, channel.control, sel);
    const info = identify_info orelse return;
    channel.master = .{ .info = info };
    channel.busmaster_reg.write(
        8,
        getBusmasterOffset(sel, .prdt_lowest),
        @truncate(@intFromPtr(prdt_phys)),
    );
    channel.busmaster_reg.write(
        8,
        getBusmasterOffset(sel, .prdt_low),
        @truncate(@intFromPtr(prdt_phys) >> 8),
    );
    channel.busmaster_reg.write(
        8,
        getBusmasterOffset(sel, .prdt_high),
        @truncate(@intFromPtr(prdt_phys) >> 16),
    );
    channel.busmaster_reg.write(
        8,
        getBusmasterOffset(sel, .prdt_highest),
        @truncate(@intFromPtr(prdt_phys) >> 24),
    );

    const payload = try (try heap.mod.data()).create(Payload);
    payload.* = .{
        .channel = channel,
        .sel = sel,
    };

    const node: vfs.Vnode = .{
        .data = .{
            .file = .{
                .payload = @ptrCast(payload),
                .transfer = transfer,
            },
        },
    };
    try root.data.directory.children.put("sda0", node);
}

fn initChannel(
    main: io.Port,
    control: io.Port,
    busmaster_reg: pci.PCIDev.BAR,
    which: enum {
        primary,
        secondary,
    },
) InitError!void {
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
    const channel: Channel = .{
        .main = main,
        .control = control,
        .busmaster_reg = busmaster_reg,
        .master = null,
        .slave = null,
        .prdt = prdt_virt,
        .active = null,
        .status = undefined,
    };
    switch (which) {
        .primary => channel_primary = channel,
        .secondary => channel_secondary = channel,
    }

    // Software reset
    io.out(8, channel.control + @intFromEnum(RegisterControl.status_cmd), 0x4);
    for (0..100) |_| _ = io.in(8, channel.control + @intFromEnum(
        RegisterControl.status_cmd,
    ));
    io.out(8, channel.control + @intFromEnum(RegisterControl.status_cmd), 0x0);

    const root = try vfs.mod.data_ref();
    try initDrive(&(switch (which) {
        .primary => channel_primary,
        .secondary => channel_secondary,
    }.?), .master, prdt_phys, root);
    try initDrive(&(switch (which) {
        .primary => channel_primary,
        .secondary => channel_secondary,
    }.?), .slave, prdt_phys, root);
}

const TransferDir = enum(u1) { write, read };

const RegisterBusmaster = enum(u16) {
    command = 0,
    status = 2,
    prdt_lowest = 4,
    prdt_low = 5,
    prdt_high = 6,
    prdt_highest = 7,
};

inline fn getBusmasterOffset(select: DriveSel, reg: RegisterBusmaster) u16 {
    return @intFromEnum(reg) + @as(u16, switch (select) {
        .master => 0,
        .slave => 8,
    });
}

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

const frames = @import("../../alloc/frames.zig");

fn fillPRDT(
    channel: *const Channel,
    bfrs: []const []u8,
    dir: TransferDir,
) virtual.MapError!struct {
    bfrs: usize,
    offset: usize,
    bytes: usize,
} {
    var i: usize = 0;
    var bfrs_consumed: usize = 0;
    var offset: usize = 0;
    var bytes: usize = 0;

    for (bfrs) |data| multi: {
        var last_paddr: ?u32 = null;
        var remaining_len: usize = data.len;

        var remap_len: usize = 0;
        const max_remap_len: usize = 0x10000;

        var seg_len: u16 = @intCast(@min(remaining_len, paging.page_size));

        const max_entry_cnt = paging.page_size / @sizeOf(PRD);

        while (remaining_len > 0) : ({
            offset += seg_len;
            remaining_len -= seg_len;
            if (i == max_entry_cnt) break :multi;
            if (i > max_entry_cnt) unreachable;
            seg_len = @min(remaining_len, paging.page_size);
        }) {
            const paddr: usize = @intFromPtr(paging.getPageMapping(
                @ptrFromInt(paging.page_align.backward(
                    @intFromPtr(data.ptr + offset),
                )),
            ) orelse std.debug.panic(
                "unmapped buffer passed to ATA DMA operation",
                .{},
            )) + (@intFromPtr(data.ptr + offset) % paging.page_size);
            const paddr_low: u32 = @truncate(paddr);

            const paddr_valid = paddr_low == paddr;

            if (remap_len > max_remap_len) unreachable;

            if (remap_len > 0 and (paddr_valid or remap_len == max_remap_len or i == max_entry_cnt - 1)) {
                const pg_cnt = (remap_len + paging.page_size - 1) / remap_len;
                const phys = try pmm.pmmAllocLow(pg_cnt * paging.page_size);
                errdefer pmm.pmmFree(
                    @intFromPtr(phys),
                    pg_cnt * paging.page_size,
                );
                if (dir == .write) {
                    const virt = try frames.frameAllocContig(pg_cnt);
                    defer for (0..pg_cnt) |j| frames.frameFree(
                        @intFromPtr(virt.ptr) + j * paging.page_size,
                    );
                    const vr: virtual.Vregion = .{
                        .paddr = phys,
                        .vaddr = virt.ptr,
                        .pg_cnt = pg_cnt,
                        .flags = .{},
                    };
                    try paging.mapRegion(&vr);
                    @memcpy(
                        virt[0..remap_len],
                        virt[offset - remap_len .. offset],
                    );
                }
                channel.prdt[i] = .{
                    .buf_paddr = @intCast(@intFromPtr(phys)),
                    .transfer_size = @truncate(remap_len),
                    .last_entry = false,
                };
                i += 1;
                last_paddr = null;
            }
            if (!paddr_valid) {
                remap_len += seg_len;
                continue;
            }
            if (last_paddr) |last| {
                // 0 = 0x10000
                if (last + paging.page_size == paddr and channel.prdt[i - 1].transfer_size > 0) {
                    channel.prdt[i - 1].transfer_size += seg_len;
                    continue;
                }
            }
            channel.prdt[i] = .{
                .transfer_size = seg_len,
                .buf_paddr = paddr_low,
                .last_entry = false,
            };
            last_paddr = paddr_low;
            i += 1;
        }
        bfrs_consumed += 1;
        bytes += offset;
        offset = 0;
    }
    bytes += offset;
    channel.prdt[i - 1].last_entry = true;
    return .{
        .bfrs = bfrs_consumed,
        .offset = offset,
        .bytes = bytes,
    };
}

fn cleanupPRDT(
    channel: *Channel,
    bfrs: []const []u8,
    dir: TransferDir,
) virtual.MapError!void {
    var i: usize = 0;
    var prd: PRD = channel.prdt[i];
    for (bfrs) |data| multi: {
        var offset: usize = 0;
        while (offset < data.len) : ({
            i += 1;
            offset += prd.transfer_size;
            if (prd.last_entry) break :multi;
            prd = channel.prdt[i];
        }) {
            const paddr: usize = @intFromPtr(paging.getPageMapping(
                @ptrFromInt(paging.page_align.backward(
                    @intFromPtr(data.ptr + offset),
                )),
            ) orelse std.debug.panic(
                "unmapped buffer passed to ATA DMA operation",
                .{},
            ));
            if (i == paging.page_size / @sizeOf(PRD)) unreachable;
            if (paddr <= std.math.maxInt(u32)) continue;
            if (dir == .read) {
                const pg_cnt = (prd.transfer_size + paging.page_size - 1) / paging.page_size;
                const virt = try frames.frameAllocContig(pg_cnt);
                defer for (0..pg_cnt) |j| frames.frameFree(
                    @intFromPtr(virt.ptr) + j * paging.page_size,
                );
                const vr: virtual.Vregion = .{
                    .paddr = @ptrFromInt(paddr),
                    .vaddr = virt.ptr,
                    .pg_cnt = pg_cnt,
                    .flags = .{},
                };
                try paging.mapRegion(&vr);
                @memcpy(
                    data[offset..][0..prd.transfer_size],
                    virt[0..prd.transfer_size],
                );
            }
            pmm.pmmFree(
                prd.buf_paddr,
                paging.page_align.forward(prd.transfer_size),
            );
        }
    }
}

const sector_size: usize = 512;

const IOError = error{IOFailed};

fn sendDMACommands(
    channel: *Channel,
    sel: DriveSel,
    dir: TransferDir,
    sector_num: usize,
    sector_count: usize,
) (modules.InitError || IOError)!void {
    channel.busmaster_reg.write(8, 0x0, @bitCast(@as(
        BusmasterCommandByte,
        .{ .enable = false, .dir = dir },
    )));

    // Set clear bits
    channel.busmaster_reg.write(8, 0x2, @bitCast(@as(BusmasterStatusByte, .{
        .err = true,
        .irq = true,
    })));

    const lba_bits: u8 = 0xe0;

    if (sector_num < 0xfffffff and sector_count < 0xff) {
        // LBA28 transfer
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.drive_select),
            @intFromEnum(sel) + @as(
                u8,
                @intCast(sector_num >> 24),
            ) | lba_bits,
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.sector_count),
            @intCast(sector_count),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.lba_lo),
            @truncate(sector_num),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.lba_mid),
            @truncate(sector_num >> 8),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.lba_hi),
            @truncate(sector_num >> 16),
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
        // LBA48 transfer
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.drive_select),
            @intFromEnum(sel) | lba_bits,
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.sector_count),
            @intCast(@as(u16, @intCast(sector_count)) >> @as(u4, 8)),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.lba_lo),
            @truncate(sector_num >> 24),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.lba_mid),
            @truncate(sector_num >> 32),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.lba_hi),
            @truncate(sector_num >> 40),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.sector_count),
            @truncate(sector_count),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.lba_lo),
            @truncate(sector_num),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.lba_mid),
            @truncate(sector_num >> 8),
        );
        io.out(
            8,
            channel.main + @intFromEnum(RegisterMain.lba_hi),
            @truncate(sector_num >> 16),
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

    channel.status = .Pending;
    channel.busmaster_reg.write(8, 0x0, @bitCast(@as(
        BusmasterCommandByte,
        .{ .enable = true, .dir = dir },
    )));
    while (channel.status == .Pending) {}

    if (channel.status == .Failed) {
        return error.IOFailed;
    }

    channel.busmaster_reg.write(
        8,
        0x0,
        @bitCast(@as(BusmasterCommandByte, .{
            .enable = false,
            .dir = undefined,
        })),
    );
}

const DMAError = CommandError || IOError;

fn doDMA(
    channel: *Channel,
    sel: DriveSel,
    bfrs_p: [][]u8,
    dir: TransferDir,
    lba_p: u48,
) DMAError!void {
    channel.active = sel;
    var lba = lba_p;
    var bfrs = bfrs_p;
    while (bfrs.len > 0) {
        const increment = try fillPRDT(channel, bfrs, dir);

        try sendDMACommands(
            channel,
            sel,
            dir,
            lba,
            increment.bytes / sector_size,
        );

        lba += @intCast(increment.bytes / sector_size);

        try cleanupPRDT(channel, bfrs, dir);
        bfrs = bfrs[increment.bfrs..];
        if (bfrs.len > 0) {
            bfrs[0] = bfrs[0][increment.offset..];
        }
    }
    channel.active = null;
}

var channel_primary: ?Channel = null;
var channel_secondary: ?Channel = null;

const idt = @import("../../arch.zig").idt;
const lapic = @import("../../smp/lapic.zig");

fn handleISR(channel: *Channel) void {
    const active = channel.active orelse return;
    const status: BusmasterStatusByte = @bitCast(
        channel.busmaster_reg.read(
            8,
            getBusmasterOffset(active, .status),
        ),
    );
    if (!status.irq) {
        return;
    }
    const command: BusmasterCommandByte = @bitCast(channel.busmaster_reg.read(
        8,
        getBusmasterOffset(active, .command),
    ));
    console.print("command: {}\n", .{command});
    channel.status = if (status.err) .Failed else .Done;
}

fn isrPrimary() callconv(idt.int_callconv) void {
    defer lapic.eoi();
    handleISR(&(channel_primary orelse return));
}

fn isrSecondary() callconv(idt.int_callconv) void {
    defer lapic.eoi();
    handleISR(&(channel_secondary orelse return));
}

fn init(dev: *pci.PCIDev) void {
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
        }
        if (!prog_if.secondary_pci) {
            const vec = interrupts.alloc(isrSecondary) catch |err| break :init err;
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
    } catch |err| std.debug.panic(
        "critical system error while initializing ATA: {s}",
        .{@errorName(err)},
    );
}

pub var driver_pci: pci.Driver = .{
    .init = init,
    .class_major = .mass_storage_controller,
    .class_minor = 0x1,
};
