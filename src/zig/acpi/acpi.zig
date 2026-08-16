const std = @import("std");
const multiboot = @import("../multiboot.zig");
const virtual = @import("../virtual.zig");
const heap = @import("../alloc/heap.zig");
const console = @import("../console.zig");
const sdt = @import("sdt.zig");
const fadt = @import("fadt.zig");
const scoping = @import("scoping.zig");
const modules = @import("../modules.zig");

/// Version 1 of the RSDP.
pub const RSDPv1 = extern struct {
    signature: [8]u8,
    checksum: u8,
    oem_id: [6]u8,
    rev: u8,
    rsdt_addr: u32,
};

/// Version 2 of the RSDP. The first 5 fields are identical, but there are four
/// additional fields following them. rsdt_addr still exists but is deprecated
/// in favor of xsdt_addr.
pub const RSDPv2 = extern struct {
    signature: [8]u8,
    checksum: u8,
    oem_id: [6]u8,
    rev: u8,
    rsdt_addr: u32,

    len: u32,
    xsdt_addr: u64,
    checksum_ext: u8,
    rsvd: [3]u8,
};

/// The RSDT, a table pointed to by the RSDPv1 that contains pointers to each
/// other SDT.
const RSDT = extern struct {
    header: sdt.DefBlockHeader,
    first_sdt_ptr: u32,
};

/// The XSDT, a table pointed to by the RSDPv2 that contains pointers to each
/// other SDT.
const XSDT = extern struct {
    header: sdt.DefBlockHeader,
    first_sdt_ptr: u64 align(4),
};

/// Verifies a SDT given a pointer to its header and its length. Returns true
/// upon success, and false upon an invalid table.
fn verifySDT(ptr: *const sdt.DefBlockHeader, len: usize) bool {
    const data: []const u8 = @as([*]const u8, @ptrCast(ptr))[0..len];
    var checksum: u8 = 0;
    for (data) |b| {
        checksum +%= b;
    }
    return checksum == 0;
}

pub fn findSDT(sig: *const [4]u8) modules.InitError!?*sdt.DefBlockHeader {
    for ((try mod.data())) |ptr| {
        if (std.mem.eql(u8, ptr.signature[0..4], sig)) {
            return ptr;
        }
    }
    return null;
}

/// Initializes ACPI.
fn init() modules.InitError![]*sdt.DefBlockHeader {
    const allocator = try heap.mod.data();
    const ptrs = rsdp: {
        const tag_new = multiboot.multibootFindTag(
            multiboot.MultibootTagAcpiNew,
        ) catch {
            const tag_old = multiboot.multibootFindTag(
                multiboot.MultibootTagAcpiOld,
            ) catch return error.Unsupported;

            const rsdp: RSDPv1 = tag_old.rsdp;
            if (!verifySDT(@ptrCast(&rsdp), @sizeOf(RSDPv1))) {
                return error.InitializationFailure;
            }

            const rsdt: *RSDT = @ptrCast(sdt.backSDT(
                @ptrFromInt(rsdp.rsdt_addr),
            ) catch return error.InitializationFailure);
            if (!verifySDT(@ptrCast(rsdt), rsdt.header.length)) {
                return error.InitializationFailure;
            }

            const entry_count: usize = (rsdt.header.length - @sizeOf(sdt.DefBlockHeader)) / @sizeOf(u32);
            const entries: []const u32 = (@as(
                [*]const u32,
                @ptrCast(&rsdt.first_sdt_ptr),
            ))[0..entry_count];
            const ptrs: []*sdt.DefBlockHeader = allocator.alloc(
                *sdt.DefBlockHeader,
                entry_count,
            ) catch return error.InitializationFailure;
            for (0..entry_count) |i| {
                ptrs[i] = @ptrFromInt(entries[i]);
            }
            break :rsdp ptrs;
        };

        const rsdp: RSDPv2 = tag_new.rsdp;
        if (!verifySDT(@ptrCast(&rsdp), rsdp.len)) {
            return error.InitializationFailure;
        }

        const xsdt: *XSDT = @ptrCast(@alignCast(sdt.backSDT(
            @ptrFromInt(@as(usize, @intCast(rsdp.xsdt_addr))),
        ) catch return error.InitializationFailure));
        if (!verifySDT(@ptrCast(xsdt), xsdt.header.length)) {
            return error.InitializationFailure;
        }
        const entry_count: usize = (xsdt.header.length - @sizeOf(sdt.DefBlockHeader)) / @sizeOf(u64);
        const entries: []align(4) const u64 = (@as(
            [*]align(4) const u64,
            @ptrCast(&xsdt.first_sdt_ptr),
        ))[0..entry_count];
        const ptrs: []*sdt.DefBlockHeader = allocator.alloc(
            *sdt.DefBlockHeader,
            entry_count,
        ) catch return error.InitializationFailure;
        for (0..entry_count) |i| {
            ptrs[i] = @ptrFromInt(@as(usize, @intCast(entries[i])));
        }
        break :rsdp ptrs;
    };

    scoping.init(allocator) catch return error.InitializationFailure;
    for (0..ptrs.len) |i| {
        ptrs[i] = sdt.backSDT(ptrs[i]) catch return error.InitializationFailure;
        console.print("Found {s}.\n", .{ptrs[i].signature});
        if (!verifySDT(ptrs[i], ptrs[i].length)) {
            return error.InitializationFailure;
        }
        if (std.mem.eql(u8, &ptrs[i].signature, "FACP")) {
            fadt.initFADT(ptrs[i], allocator) catch return error.InitializationFailure;
        }
    }

    return ptrs;
}

/// The ACPI module, which maps and parses ACPI tables during the boot sequence.
pub var mod: modules.Module([]*sdt.DefBlockHeader) = .{
    .name = "acpi",
    .init = init,
};
