const GUID = [16]u8;

const Header = extern struct {
    signature: [8]u8, // EFI PART
    revision: u32,
    size: u32,
    header_checksum: u32,
    rsvd: u32,
    lba_this: u64,
    lba_alt: u64,
    first_usable: u64,
    last_usable: u64,
    disk_guid: GUID,
    table_lba: u64,
    partition_count: u32,
    entry_size: u32,
    table_checksum: u32,
};

const Entry = extern struct {
    type_guid: GUID,
    part_guid: GUID,
    lba_start: u64,
    lba_end: u64,
    attrs: packed struct(u64) {
        required_by_firmware: bool,
        unused0: u1,
        used_by_os: bool,
        unused1: u61,
    },
    name: [72]u8,
};

const std = @import("std");
const vfs = @import("../../vfs.zig");

const sector_size: usize = 512;

const BlockDevice = @import("../../BlockDevice.zig");

fn init(dev: *BlockDevice) vfs.Vnode.TransferError!bool {
    const console = @import("../../console.zig");
    const heap = @import("../../alloc/heap.zig");
    var header_bfr: [@sizeOf(Header)]u8 = undefined;
    _ = try dev.transferQueued(
        &header_bfr,
        .read,
        sector_size,
        .standard,
        .immediate,
        null,
    );
    var header: Header = @bitCast(header_bfr);
    console.print("sig: {s}\n", .{header.signature});
    if (!std.mem.eql(u8, &header.signature, "EFI PART")) return false;

    const use_crc32 = false; // TianoCore seems to set the checksum to zero.
    if (use_crc32) {
        const crc32 = @import("../../utils/crc32.zig");
        const actual = header.table_checksum;
        header.table_checksum = 0;
        header_bfr = @bitCast(header);
        const calculated = crc32.calc(&header_bfr);
        console.print("Calculated: {x}. Actual: {x}.\n", .{ calculated, actual });
        if (calculated != actual) return false;
    }

    console.print(
        "Found GPT partition table with {} partitions.\n",
        .{header.partition_count},
    );

    const gpa = heap.mod.data() catch return error.CriticalSystemFailure;
    const table_size: usize = @intCast(
        header.partition_count * header.entry_size,
    );
    const table: []u8 = gpa.alloc(
        u8,
        table_size,
    ) catch return error.CriticalSystemFailure;
    _ = try dev.transferQueued(
        table,
        .read,
        header.table_lba * sector_size,
        .standard,
        .immediate,
        null,
    );
    for (0..header.partition_count) |i| {
        const offset = i * header.entry_size;
        const entry: *align(1) Entry = @ptrCast(table.ptr + offset);
        if (std.mem.eql(
            u8,
            &entry.part_guid,
            &@as([16]u8, @splat(0)),
        )) continue;
        console.hexdump(table[offset..][0..16]);
        console.print("offset: {x}.", .{offset + header.table_lba * sector_size});
        console.print("Type GUID: {x}.\n", .{entry.type_guid});
    }

    return true;
}

const partition = @import("../../subsystems/partition.zig");

pub var driver_partition: partition.Driver = .{
    .init = init,
};
