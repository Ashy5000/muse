//! Handles muse's paging system when building for x86_64. Some of the functions contained in this file are only used when paging is first initialized, and others are used throughout the lifetime of the OS.
const std = @import("std");
const virtual = @import("../../virtual.zig");
const pmm = @import("../../alloc/pmm.zig");
const msr = @import("../../utils/msr.zig");
const cpuid = @import("../../cpuid.zig");
const modules = @import("../../modules.zig");

const maxInt = std.math.maxInt;

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
    unresolved: bool = false,
    avl_0: u2 = 0,
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
        ext: u16,
    },
};

const page_entries: usize = page_size / @sizeOf(PageEntry);

const PageSize = enum(usize) {
    @"4k" = 4 * 1024,
    @"2m" = 2 * 1024 * 1024,
    @"1g" = 1024 * 1024 * 1024,
};

pub const MapError = pmm.AllocError || modules.InitError;

// The following functions are used multiple times throughout the lifetime of the OS.

fn resolveTable(
    parent: *[page_entries]PageEntry,
    idx: u9,
    clear_addr: ?*[page_entries]PageEntry,
) pmm.AllocError!void {
    if (parent[idx].fields.present) {
        return;
    }
    var entry: PageEntry = .{ .fields = .{
        .present = true,
        .write = true,
        .user = true,
        .pat_2 = false,
        .pat_1 = false,
        .size = false,
        .addr_hi = 0,
        .exec_disable = false,
    } };
    const table_paddr = try pmm.pmmAlloc(page_size);
    const table: *[page_entries]PageEntry = clear_addr orelse @ptrCast(table_paddr);
    entry.int |= @intFromPtr(table_paddr);
    parent[idx] = entry;
    @memset(table, .{ .int = 0 });
}

fn applyPATIndex(entry: *PageEntry, vflags: virtual.Vflags, size: PageSize) MapError!void {
    var res = entry;
    const pat_idx: u3 = if ((try cpuid.mod.data()).features.msr)
        @intFromEnum(vflags.cache_mode)
    else
        0;
    res.fields.pat_1 = (pat_idx & 0x2) > 0;
    res.fields.pat_2 = (pat_idx & 0x4) > 0;
    switch (size) {
        .@"4k" => res.fields.size = pat_idx & 0x1 == 1,
        else => res.fields.addr_hi |= (pat_idx & 0x1),
    }
}

fn registerFrame(
    table: *align(page_size) [page_entries]PageEntry,
    idx: u9,
    paddr: [*]allowzero align(page_size) u8,
    size: PageSize,
    vflags: virtual.Vflags,
) MapError!void {
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
        .addr_hi = @intCast(@intFromPtr(paddr) >> 12),
        .exec_disable = false,
    } };
    try applyPATIndex(&entry, vflags, size);
    table[idx] = entry;
}

// The following functions are to be called only when the entirety of memory is identity paged.
// They are designed to be used when paging is first initialized.

fn mapPageInit(
    pml4: *align(page_size) [page_entries]PageEntry,
    vaddr: Vaddr,
    paddr: [*]allowzero align(page_size) u8,
    size: PageSize,
    vflags: virtual.Vflags,
) MapError!void {
    try resolveTable(pml4, vaddr.components.pml4, null);
    const ptr_table: *align(page_size) [page_entries]PageEntry = @ptrFromInt(
        pml4[vaddr.components.pml4].fields.addr_hi << 12,
    );
    if (size == .@"1g") {
        try registerFrame(
            ptr_table,
            vaddr.components.directory_ptr,
            paddr,
            size,
            vflags,
        );
        return;
    }
    try resolveTable(ptr_table, vaddr.components.directory_ptr, null);
    const directory: *align(page_size) [page_entries]PageEntry = @ptrFromInt(
        ptr_table[vaddr.components.directory_ptr].fields.addr_hi << 12,
    );
    if (size == .@"2m") {
        try registerFrame(
            directory,
            vaddr.components.directory,
            paddr,
            size,
            vflags,
        );
        return;
    }
    try resolveTable(directory, vaddr.components.directory, null);
    const table: *align(page_size) [page_entries]PageEntry = @ptrFromInt(
        directory[vaddr.components.directory].fields.addr_hi << 12,
    );
    try registerFrame(table, vaddr.components.table, paddr, size, vflags);
}

fn mapRegionInit(
    pml4: *align(page_size) [page_entries]PageEntry,
    vr: virtual.Vregion,
) MapError!void {
    const vflags: virtual.Vflags = vr.flags;
    var vaddr: [*]allowzero align(page_size) u8 = vr.vaddr;
    var paddr: [*]allowzero align(page_size) u8 = vr.paddr;
    var remaining: usize = vr.pg_cnt * page_size;
    var inc: usize = 0;
    while (true) {
        vaddr = @ptrFromInt(@intFromPtr(vaddr) + inc);
        paddr = @ptrFromInt(@intFromPtr(paddr) + inc);
        remaining -= inc;
        if (remaining == 0) {
            break;
        }
        const vaddr_int = @intFromPtr(vaddr);
        const paddr_int = @intFromPtr(paddr);
        const addr_union: Vaddr = .{ .addr = vaddr_int };
        if (remaining >= @intFromEnum(PageSize.@"2m") and (paddr_int % @intFromEnum(PageSize.@"2m")) == 0 and (vaddr_int % @intFromEnum(PageSize.@"2m")) == 0) {
            try mapPageInit(pml4, addr_union, paddr, PageSize.@"2m", vflags);
            inc = @intFromEnum(PageSize.@"2m");
            continue;
        }
        try mapPageInit(pml4, addr_union, paddr, PageSize.@"4k", vflags);
        inc = @intFromEnum(PageSize.@"4k");
    }
}

var first_region: ?*virtual.Vregion = null;

pub fn mapPage(
    addr: [*]allowzero align(page_size) u8,
    paddr: [*]allowzero align(page_size) u8,
    size: PageSize,
    vflags: virtual.Vflags,
) MapError!void {
    asm volatile ("invlpg (%[addr])"
        :
        : [addr] "r" (addr),
        : .{
          .memory = true,
        });
    const vaddr: Vaddr = .{ .addr = @intFromPtr(addr) };
    const pml4: *align(page_size) [page_entries]PageEntry = @ptrFromInt(@as(Vaddr, .{
        .components = .{
            .ext = maxInt(u16),
            .pml4 = maxInt(u9),
            .directory_ptr = maxInt(u9),
            .directory = maxInt(u9),
            .table = maxInt(u9),
            .offset = 0,
        },
    }).addr);
    const ptr_table: *align(page_size) [page_entries]PageEntry = @ptrFromInt(@as(Vaddr, .{
        .components = .{
            .ext = maxInt(u16),
            .pml4 = maxInt(u9),
            .directory_ptr = maxInt(u9),
            .directory = maxInt(u9),
            .table = vaddr.components.pml4,
            .offset = 0,
        },
    }).addr);
    try resolveTable(pml4, vaddr.components.pml4, ptr_table);
    if (size == .@"1g") {
        try registerFrame(ptr_table, vaddr.components.directory_ptr, paddr, size, vflags);
        return;
    }
    const directory: *align(page_size) [page_entries]PageEntry = @ptrFromInt(@as(Vaddr, .{
        .components = .{
            .ext = maxInt(u16),
            .pml4 = maxInt(u9),
            .directory_ptr = maxInt(u9),
            .directory = vaddr.components.pml4,
            .table = vaddr.components.directory_ptr,
            .offset = 0,
        },
    }).addr);
    try resolveTable(ptr_table, vaddr.components.directory_ptr, directory);
    if (size == .@"2m") {
        try registerFrame(directory, vaddr.components.directory, paddr, size, vflags);
        return;
    }
    const table: *align(page_size) [page_entries]PageEntry = @ptrFromInt(@as(Vaddr, .{
        .components = .{
            .ext = maxInt(u16),
            .pml4 = maxInt(u9),
            .directory_ptr = vaddr.components.pml4,
            .directory = vaddr.components.directory_ptr,
            .table = vaddr.components.directory,
            .offset = 0,
        },
    }).addr);
    try resolveTable(directory, vaddr.components.directory, table);
    try registerFrame(table, vaddr.components.table, paddr, size, vflags);
}

pub fn mapRegion(vr: *const virtual.Vregion) MapError!void {
    const vflags: virtual.Vflags = vr.flags;
    var vaddr: [*]allowzero align(page_size) u8 = vr.vaddr;
    var paddr: [*]allowzero align(page_size) u8 = vr.paddr;
    var remaining: usize = vr.pg_cnt * page_size;
    var inc: usize = 0;
    while (true) {
        vaddr = @ptrFromInt(@intFromPtr(vaddr) + inc);
        paddr = @ptrFromInt(@intFromPtr(paddr) + inc);
        remaining -= inc;
        if (remaining == 0) {
            break;
        }
        const paddr_int: usize = @intFromPtr(paddr);
        const vaddr_int: usize = @intFromPtr(vaddr);
        const cpuid_info = try cpuid.mod.data();
        if (cpuid_info.extended_info.pdpe1gb and remaining >= @intFromEnum(PageSize.@"1g") and (paddr_int % @intFromEnum(PageSize.@"1g")) == 0 and (vaddr_int % @intFromEnum(PageSize.@"1g")) == 0) {
            try mapPage(vaddr, paddr, PageSize.@"1g", vflags);
            inc = @intFromEnum(PageSize.@"1g");
            continue;
        }
        if (remaining >= @intFromEnum(PageSize.@"2m") and (paddr_int % @intFromEnum(PageSize.@"2m")) == 0 and (vaddr_int % @intFromEnum(PageSize.@"2m")) == 0) {
            try mapPage(vaddr, paddr, PageSize.@"2m", vflags);
            inc = @intFromEnum(PageSize.@"2m");
            continue;
        }
        try mapPage(vaddr, paddr, PageSize.@"4k", vflags);
        inc = @intFromEnum(PageSize.@"4k");
    }
}

pub fn getPageStatus(addr: [*]align(page_size) u8) bool {
    const vaddr: Vaddr = .{ .addr = @intFromPtr(addr) };
    const ptr_table: *[page_entries]PageEntry = @ptrFromInt(@as(Vaddr, .{
        .components = .{
            .ext = maxInt(u16),
            .pml4 = maxInt(u9),
            .directory_ptr = maxInt(u9),
            .directory = maxInt(u9),
            .table = vaddr.components.pml4,
            .offset = 0,
        },
    }).addr);
    const ptr_table_entry: PageEntry = ptr_table[vaddr.components.directory_ptr];
    if (!ptr_table_entry.fields.present) {
        return false;
    }
    if (ptr_table_entry.fields.size) {
        return true;
    }
    const directory: *[page_entries]PageEntry = @ptrFromInt(@as(Vaddr, .{
        .components = .{
            .ext = maxInt(u16),
            .pml4 = maxInt(u9),
            .directory_ptr = maxInt(u9),
            .directory = vaddr.components.pml4,
            .table = vaddr.components.directory_ptr,
            .offset = 0,
        },
    }).addr);
    const directory_entry: PageEntry = directory[vaddr.components.directory];
    if (!directory_entry.fields.present) {
        return false;
    }
    if (directory_entry.fields.size) {
        return true;
    }
    const table: *[page_entries]PageEntry = @ptrFromInt(@as(Vaddr, .{
        .components = .{
            .ext = maxInt(u16),
            .pml4 = maxInt(u9),
            .directory_ptr = vaddr.components.pml4,
            .directory = vaddr.components.directory_ptr,
            .table = vaddr.components.directory,
            .offset = 0,
        },
    }).addr);
    const table_entry: PageEntry = table[vaddr.components.table];
    return table_entry.fields.present;
}

pub fn init() MapError!void {
    const pml2: *align(page_size) [page_entries]PageEntry = @ptrCast(try pmm.pmmAlloc(page_size));
    @memset(pml2, .{ .int = 0 });

    const multiboot = @import("../../multiboot.zig");
    const regions = [_]virtual.Vregion{
        (try pmm.mod.data()).global_vr,
        (try @import("../../elf.zig").mod.data()),
        (try multiboot.mod.data()).multiboot_vr,
    };
    for (regions) |vr| {
        try mapRegionInit(pml2, vr);
    }

    pml2[pml2.len - 1] = .{
        .fields = .{
            .present = true,
            .write = true,
            .user = false,
            .pat_2 = false,
            .pat_1 = false,
            .size = false,
            .addr_hi = @intCast(@intFromPtr(pml2) >> 12),
            .exec_disable = false,
        },
    };
    const pat_msr: msr.MSR = 0x277;
    // This value is set such that virtual.PageCacheMode becomes an index into the PAT:
    // 0x00 = Uncacheable
    // 0x01 = WriteCombining
    // 0x04 = Writethrough
    // 0x05 = WriteProtect
    // 0x06 = Writeback
    // 0x07 = Uncached
    msr.setMSR(pat_msr, 0x00_01_04_05_06_07_00_00) catch {};
    asm volatile ("mov %[pml2], %%cr3"
        :
        : [pml2] "r" (pml2),
        : .{ .memory = true });
}
