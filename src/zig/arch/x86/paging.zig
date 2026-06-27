//! Handles muse's paging system when building for x86. Some of the functions contained in this file are only used when paging is first initialized, and others are used throughout the lifetime of the OS.

const std = @import("std");
const pmm = @import("../../alloc/pmm.zig");
const virtual = @import("../../virtual.zig");
const elf = @import("../../elf.zig");
const console = @import("../../console.zig");
const modules = @import("../../modules.zig");

pub const page_size: usize = 4096;
pub const page_align = std.mem.Alignment.fromByteUnits(page_size);

/// Flags for an entry into the root page directory.
const pageDirectoryEntryFlags = packed struct {
    present: bool = true,
    write: bool = true,
    user: bool = true,
    write_through: bool = false,
    cache_disable: bool = false,
    accessed: bool = false,
    unresolved: bool = false,
    size: bool = false,
};

/// An entry in the root page directory, corresonding to a page table (if present).
const pageDirectoryEntry = packed struct {
    flags: pageDirectoryEntryFlags,
    /// There are four extra unused bits in each entry.
    available: u4 = 0,
    addr_hi: u20 = 0,
};

/// Flags describing a single page.
pub const pageTableEntryFlags = packed struct {
    present: bool = true,
    write: bool = true,
    user: bool = false,
    write_through: bool = false,
    cache_disable: bool = false,
    accessed: bool = false,
    dirty: bool = false,
    pat: bool = false,
    global: bool = false,
};

/// An entry into a page table, corresonding to a single page (if present).
pub const pageTableEntry = packed struct {
    flags: pageTableEntryFlags,
    available: u3 = 0,
    addr_hi: u20 = 0,
};

const page_entries: usize = page_size / @sizeOf(pageDirectoryEntry);

// The following functions are used multiple times throughout the lifetime of the OS.

/// Initializes a page table and adds a corresponding entry to a page directory, if there is not one present already. If `virt` is set, it will assume paging is enabled, and use the loopback entry in the root page directory to write to the page table. Otherwise, it will write directly to the page table's physical address.
fn initTable(directory: *[page_entries]pageDirectoryEntry, vaddr: u32, flags: pageDirectoryEntryFlags, virt: bool) pmm.PMMError!*[page_entries]pageTableEntry {
    const idx = vaddr >> 22;
    if (directory[idx].flags.present) {
        return @ptrFromInt(directory[idx].addr_hi << 12);
    }
    const table_addr: u32 = @intCast(try pmm.pmmAlloc());
    var table: *[page_entries]pageTableEntry = undefined;
    directory[idx].flags = flags;
    directory[idx].addr_hi = @intCast(table_addr >> 12);
    if (virt) {
        table = @ptrFromInt(0xffc00000 + (idx << 12));
    } else {
        table = @ptrFromInt(table_addr);
    }
    @memset(table, .{
        .flags = .{ .present = false },
    });
    return table;
}

/// Given a virtual->physical mapping of a single page and a page table, `fillTableEntry()` will create an entry corresponding to the mapped page in the table, replacing an existing mapping at that virtual address if present.
fn fillTableEntry(table: *[page_entries]pageTableEntry, vaddr: u32, paddr: u32, flags: pageTableEntryFlags) void {
    const idx = (vaddr >> 12) & 0x3ff;
    if (table[idx].flags.present) {
        return;
    }
    table[idx].flags = flags;
    table[idx].addr_hi = @intCast(paddr >> 12);
}

// The following functions are to be called only when paging is not currently enabled, or when the entirety of memory is identity paged.
// They are designed to be used when paging is first initialized.

/// Maps a page in a page directory, assuming paging is not yet initialized.
fn mapPageInit(directory: *[page_entries]pageDirectoryEntry, vaddr: u32, paddr: u32) pmm.PMMError!void {
    const table: *[page_entries]pageTableEntry = try initTable(directory, vaddr, .{}, false);
    fillTableEntry(table, vaddr, paddr, .{});
}

/// Maps a region in a page directory, assuming paging is not yet initialized.
fn mapRegionInit(directory: *[page_entries]pageDirectoryEntry, vr: *virtual.Vregion) pmm.PMMError!void {
    for (0..vr.pg_cnt) |i| {
        try mapPageInit(directory, vr.vaddr + i * page_size, vr.paddr + i * page_size);
    }
}

var first_region: ?*virtual.Vregion = null;

/// Adds a region to a linked list of regions that will be mapped once paging is enabled. If paging is already enabled, calling this function will have no effect.
pub fn registerRegion(vr: *virtual.Vregion) void {
    vr.next = first_region;
    first_region = vr;
}

// The following functions modify paging structures while paging is already active.

/// Maps a single page with a given virtual and physical address, assuming paging is enabled.
pub fn mapPage(vaddr: u32, paddr: u32, flags: pageTableEntryFlags) pmm.PMMError!void {
    const directory: *[page_entries]pageDirectoryEntry = @ptrFromInt(0xfffff000);
    _ = try initTable(directory, vaddr, .{}, true);
    const table: *[page_entries]pageTableEntry = @ptrFromInt(0xffc00000 + ((vaddr >> 10) & 0x3ff000));
    fillTableEntry(table, vaddr, paddr, flags);
}

/// Maps a region of memory, assuming paging is enabled.
pub fn mapRegion(vr: *const virtual.Vregion) pmm.PMMError!void {
    for (0..vr.pg_cnt) |i| {
        try mapPage(vr.vaddr + i * page_size, vr.paddr + i * page_size, vr.flags);
    }
}

pub fn getPageInfo(vaddr: usize) ?*pageTableEntry {
    const directory: *[page_entries]pageDirectoryEntry = @ptrFromInt(0xfffff000);
    if (!directory[vaddr >> 22].flags.present) {
        return null;
    }
    const table: *[page_entries]pageTableEntry = @ptrFromInt(0xffc00000 + ((vaddr >> 10) & 0x3ff000));
    return &table[(vaddr >> 12) & 0x3ff];
}

/// Module init: Sets up paging structures containing all regions registered with `registerRegion()` and enables paging. Supports loopback.
fn init() modules.ModuleInitError!void {
    const directory: *[page_entries]pageDirectoryEntry = @ptrFromInt(pmm.pmmAlloc() catch return error.ModuleInitFailure);
    @memset(directory, .{
        .flags = .{ .present = false },
    });
    var region = first_region;
    while (region) |vr| {
        mapRegionInit(directory, vr) catch return error.ModuleInitFailure;
        region = vr.next;
    }
    directory[directory.len - 1].flags.present = true;
    directory[directory.len - 1].addr_hi = @intCast(@intFromPtr(directory) >> 12);
    asm volatile (
        \\ mov %[directory], %%cr3
        \\ mov %%cr0, %%eax
        \\ or $0x80000001, %%eax
        \\ mov %%eax, %%cr0
        :: [directory] "r" (directory),
        : .{ .eax = true }
    );
}

pub var mod: modules.Module = .{
    .name = "paging",
    .init = init,
    .deps = &@as([2]*modules.Module, .{ &pmm.mod, &elf.mod }),
};
