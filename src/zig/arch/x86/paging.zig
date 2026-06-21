const pmm = @import("../../pmm.zig");
const virtual = @import("../../virtual.zig");
const elf = @import("../../elf.zig");
const modules = @import("../../modules.zig");

pub const page_size = 4096;

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

const pageDirectoryEntry = packed struct {
    flags: pageDirectoryEntryFlags,
    available: u4 = 0,
    addr_hi: u20 = 0,
};

const pageTableEntryFlags = packed struct {
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

const pageTableEntry = packed struct {
    flags: pageTableEntryFlags,
    available: u3 = 0,
    addr_hi: u20 = 0,
};

const page_entries: usize = page_size / @sizeOf(pageDirectoryEntry);

// The following functions are used multiple times throughout the lifetime of the OS.

fn initTable(directory: *[page_entries]pageDirectoryEntry, vaddr: u32, flags: pageDirectoryEntryFlags) pmm.PMMError!*[page_entries]pageTableEntry {
    const idx = vaddr >> 22;
    if (directory[idx].flags.present) {
        return @ptrFromInt(directory[idx].addr_hi << 12);
    }
    const table_addr: u32 = @intCast(try pmm.pmm_alloc());
    const table: *[page_entries]pageTableEntry = @ptrFromInt(table_addr);
    @memset(table, .{
        .flags = .{ .present = false },
    });
    directory[idx].flags = flags;
    directory[idx].addr_hi = @intCast(table_addr >> 12);
    return table;
}

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

fn mapPageInit(directory: *[page_entries]pageDirectoryEntry, vaddr: u32, paddr: u32) pmm.PMMError!void {
    const table: *[page_entries]pageTableEntry = try initTable(directory, vaddr, .{});
    fillTableEntry(table, vaddr, paddr, .{});
}

fn mapRegionInit(directory: *[page_entries]pageDirectoryEntry, vr: *virtual.Vregion) pmm.PMMError!void {
    for (0..vr.pg_cnt) |i| {
        try mapPageInit(directory, vr.vaddr + i * page_size, vr.paddr + i * page_size);
    }
}

var first_region: ?*virtual.Vregion = null;

pub fn register_region(vr: *virtual.Vregion) void {
    vr.next = first_region;
    first_region = vr;
}

fn init() modules.ModuleInitError!void {
    const directory: *[page_entries]pageDirectoryEntry = @ptrFromInt(try pmm.pmm_alloc());
    @memset(directory, .{
        .flags = .{ .present = false },
    });
    var region = first_region;
    while (region) |vr| {
        try mapRegionInit(directory, vr);
        region = vr.next;
    }
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
