const virtual = @import("../virtual.zig");

/// The header at the start of every ACPI SDT.
pub const DefBlockHeader = extern struct {
    signature: [4]u8,
    length: u32,
    rev: u8,
    checksum: u8,
    oem_id: [6]u8,
    oem_table_id: [8]u8,
    oem_rev: u32,
    creator_id: u32,
    creator_rev: u32,
};

pub const SDTBackError = virtual.MapError;

/// Maps a SDT in physical memory to one in virtual memory.
pub fn backSDT(ptr: *DefBlockHeader) SDTBackError!*DefBlockHeader {
    const ptr_multi: [*]u8 = @as([*]u8, @ptrCast(ptr));
    const ptr_slice: []u8 = ptr_multi[0..@sizeOf(DefBlockHeader)];
    const header_slice: []u8 = try virtual.mapPhysObj(ptr_slice);
    const header: *DefBlockHeader = @alignCast(@ptrCast(header_slice.ptr));
    const sdt_slice: []u8 = try virtual.mapPhysObj(ptr_multi[0..header.length]);
    virtual.unmapPhysObj(header_slice);
    const table: *DefBlockHeader = @alignCast(@ptrCast(sdt_slice.ptr));
    return table;
}
