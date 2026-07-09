//! Handles muse's paging system when building for x86_64. Some of the functions contained in this file are only used when paging is first initialized, and others are used throughout the lifetime of the OS.
const std = @import("std");
const virtual = @import("../../virtual.zig");
const pmm = @import("../../alloc/pmm.zig");
const msr = @import("../../utils/msr.zig");
const elf = @import("../../elf.zig");
const modules = @import("../../modules.zig");

// We can support larger pages, but this is the minimum.
pub const page_size: usize = 4096;
pub const page_align = std.mem.Alignment.fromByteUnits(page_size);

const paddr_width: u16 = 52;

const PageEntryStruct = packed struct {
    present: bool,
    write: bool,
    user: bool,
    pat_2: bool,
    pat_1: bool,
    accessed: bool = false,
    dirty: bool = false,
    size: bool, // In page tables, this bit is used to represent pat_0.
    global: bool = false,
    avl_0: u3 = 0,
    addr_hi: u40, // In the page directory pointer table and page directories, the lowest
    // bit of addr_hi is used for pat_0.
    avl_1: u11 = 0,
    exec_disable: bool,
};

/// An entry into any one of the x86_64 paging-related tables.
const PageEntry = packed union(u64) {
    int: u64,
    fields: PageEntryStruct,
};

const Vaddr = packed union(usize) {
    addr: usize,
    components: packed struct {
        offset: u12,
        table: u9,
        directory: u9,
        directory_ptr: u9,
        pml4: u9,
        rsvd: u16,
    },
};

const page_entries: usize = page_size / @sizeOf(PageEntry);

const PageSize = enum(usize) {
    @"4k" = 4 * 1024,
    @"2m" = 2 * 1024 * 1024,
    @"1g" = 1024 * 1024 * 1024,
};

// The following functions are used multiple times throughout the lifetime of the OS.

fn resolveTable(parent: *[page_entries]PageEntry, idx: u9) pmm.PMMError!void {
    if (parent[idx].fields.present) {
        return;
    }
    var entry: PageEntry = .{ .fields = .{
        .present = true,
        .write = true,
        .user = true,
        .pat_2 = undefined,
        .pat_1 = undefined,
        .size = false,
        .addr_hi = 0,
        .exec_disable = false,
    } };
    const table: *[page_entries]PageEntry = @ptrFromInt(try pmm.pmmAlloc(page_size));
    @memset(table, .{ .int = 0 });
    entry.int |= @intFromPtr(table);
    parent[idx] = entry;
}

fn applyPATIndex(entry: *PageEntry, vflags: virtual.Vflags, size: PageSize) void {
    var res = entry;
    const pat_idx: u3 = @intFromEnum(vflags.cache_mode);
    res.fields.pat_1 = (pat_idx & 0x2) > 0;
    res.fields.pat_2 = (pat_idx & 0x4) > 0;
    switch (size) {
        .@"4k" => res.fields.size = pat_idx & 0x1 == 1,
        else => res.fields.addr_hi |= (pat_idx & 0x1),
    }
}

fn registerFrame(
    table: *[page_entries]PageEntry,
    idx: u9,
    paddr: usize,
    size: PageSize,
    vflags: virtual.Vflags,
) void {
    var entry: PageEntry = .{ .fields = .{
        .present = true,
        .write = vflags.writeable,
        .user = vflags.user,
        .pat_2 = undefined,
        .pat_1 = undefined,
        .size = switch (size) {
            .@"4k" => false,
            else => true,
        },
        .addr_hi = @intCast(paddr >> 12),
        .exec_disable = false,
    } };
    applyPATIndex(&entry, vflags, size);
    table[idx] = entry;
}

// The following functions are to be called only when the entirety of memory is identity paged.
// They are designed to be used when paging is first initialized.

fn mapPageInit(
    pml4: *[page_entries]PageEntry,
    vaddr: Vaddr,
    paddr: usize,
    size: PageSize,
    vflags: virtual.Vflags,
) pmm.PMMError!void {
    try resolveTable(pml4, vaddr.components.pml4);
    const ptr_table: *[page_entries]PageEntry = @ptrFromInt(pml4[vaddr.components.pml4].fields.addr_hi << 12);
    if (size == .@"1g") {
        registerFrame(ptr_table, vaddr.components.directory_ptr, paddr, size, vflags);
        return;
    }
    try resolveTable(ptr_table, vaddr.components.directory_ptr);
    const directory: *[page_entries]PageEntry = @ptrFromInt(ptr_table[vaddr.components.directory_ptr].fields.addr_hi << 12);
    if (size == .@"2m") {
        registerFrame(directory, vaddr.components.directory, paddr, size, vflags);
        return;
    }
    try resolveTable(directory, vaddr.components.directory);
    const table: *[page_entries]PageEntry = @ptrFromInt(directory[vaddr.components.directory].fields.addr_hi << 12);
    registerFrame(table, vaddr.components.table, paddr, size, vflags);
}

fn mapRegionInit(pml4: *[page_entries]PageEntry, vr: *virtual.Vregion) pmm.PMMError!void {
    const vflags: virtual.Vflags = vr.flags;
    var vaddr: usize = vr.vaddr;
    var paddr: usize = vr.paddr;
    var remaining: usize = vr.pg_cnt * page_size;
    var inc: usize = 0;
    while (remaining > 0) {
        vaddr += inc;
        paddr += inc;
        remaining -= inc;
        const addr_union: Vaddr = .{ .addr = vaddr };
        if (remaining >= @intFromEnum(PageSize.@"2m") and (paddr % @intFromEnum(PageSize.@"2m")) == 0) {
            try mapPageInit(pml4, addr_union, paddr, PageSize.@"2m", vflags);
            inc = @intFromEnum(PageSize.@"2m");
            continue;
        }
        try mapPageInit(pml4, addr_union, paddr, PageSize.@"4k", vflags);
        inc = @intFromEnum(PageSize.@"4k");
    }
}

var first_region: ?*virtual.Vregion = null;

/// Adds a region to a linked list of regions that will be mapped once paging is enabled. If paging is already enabled, calling this function will have no effect.
pub fn registerRegion(vr: *virtual.Vregion) void {
    vr.next = first_region;
    first_region = vr;
}

fn init() modules.ModuleInitError!void {
    const pml2: *[page_entries]PageEntry = @ptrFromInt(pmm.pmmAlloc(page_size) catch return error.ModuleInitFailure);
    @memset(pml2, .{ .int = 0 });
    var region = first_region;
    while (region) |vr| {
        mapRegionInit(pml2, vr) catch return error.ModuleInitFailure;
        region = vr.next;
    }
    const pat_msr: msr.MSR = 0x277;
    // This value is set such that virtual.PageCacheMode becomes an index into the PAT:
    // 0x00 = Uncacheable
    // 0x01 = WriteCombining
    // 0x04 = Writethrough
    // 0x05 = WriteProtect
    // 0x06 = Writeback
    // 0x07 = Uncached
    msr.setMSR(pat_msr, 0x00_01_04_05_06_07_00_00);
    asm volatile ("mov %[pml2], %%cr3"
        :: [pml2] "r" (pml2),
        : .{ .memory = true }
    );
}

/// The paging module, which initializes paging structures and enables paging
/// on an x86_64 system.
pub var mod: modules.Module = .{
    .name = "paging",
    .init = init,
    .deps = &@as([2]*modules.Module, .{ &pmm.mod, &elf.mod }),
};
