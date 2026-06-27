const multiboot = @import("multiboot.zig");
const virtual = @import("virtual.zig");
const heap = @import("alloc/heap.zig");
const console = @import("console.zig");
const modules = @import("modules.zig");

pub const RSDPv1 = extern struct {
    signature: [8]u8,
    checksum: u8,
    oem_id: [6]u8,
    rev: u8,
    rsdt_addr: u32,
};

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

const ACPISDTHeader = extern struct {
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

const RSDT = extern struct {
    header: ACPISDTHeader,
    first_sdt_ptr: u32,
};

const XSDT = extern struct {
    header: ACPISDTHeader,
    first_sdt_ptr: u64,
};

var sdt_ptrs: ?[]*ACPISDTHeader = null;

fn backSDT(ptr: *ACPISDTHeader) modules.ModuleInitError!*ACPISDTHeader {
    const ptr_multi: [*]u8 = @as([*]u8, @ptrCast(ptr));
    const ptr_slice: []u8 = ptr_multi[0..@sizeOf(ACPISDTHeader)];
    const header_slice: []u8 = virtual.mapPhysObj(ptr_slice) catch return error.ModuleInitFailure;
    const header: *ACPISDTHeader = @alignCast(@ptrCast(header_slice.ptr));
    const sdt_slice: []u8 = virtual.mapPhysObj(ptr_multi[0..header.length]) catch return error.ModuleInitFailure;
    virtual.unmapPhysObj(header_slice);
    const sdt: *ACPISDTHeader = @alignCast(@ptrCast(sdt_slice.ptr));
    return sdt;
}

fn init() modules.ModuleInitError!void {
    const allocator = heap.allocator() catch return error.ModuleInitFailure;
    const tag_new = multiboot.multibootFindTag(multiboot.MultibootTagAcpiNew) catch {
        const tag_old = multiboot.multibootFindTag(multiboot.MultibootTagAcpiOld) catch return error.ModuleUnsupported;
        const rsdp: RSDPv1 = tag_old.rsdp;
        const rsdt: *RSDT = @ptrCast(try backSDT(@ptrFromInt(rsdp.rsdt_addr)));
        const entry_count: usize = (rsdt.header.length - @sizeOf(ACPISDTHeader)) / @sizeOf(u32);
        const entries: []const u32 = (@as([*]const u32, @ptrCast(&rsdt.first_sdt_ptr)))[0..entry_count];
        const ptrs: []*ACPISDTHeader = allocator.alloc(*ACPISDTHeader, entry_count) catch return error.ModuleInitFailure;
        for (0..entry_count) |i| {
            ptrs[i] = @ptrFromInt(entries[i]);
        }
        sdt_ptrs = ptrs;
        return;
    };
    const rsdp: RSDPv2 = tag_new.rsdp;
    const xsdt: *XSDT = @ptrCast(try backSDT(@ptrFromInt(@as(usize, @intCast(rsdp.xsdt_addr)))));
    const entry_count: usize = (xsdt.header.length - @sizeOf(ACPISDTHeader)) / @sizeOf(u64);
    const entries: []const u64 = (@as([*]const u64, @ptrCast(&xsdt.first_sdt_ptr)))[0..entry_count];
    const ptrs: []*ACPISDTHeader = allocator.alloc(*ACPISDTHeader, entry_count) catch return error.ModuleInitFailure;
    for (0..entry_count) |i| {
        console.print("SDT at 0x{x}.\n", .{entries[i]});
        ptrs[i] = @ptrFromInt(@as(usize, @intCast(entries[i])));
    }
    sdt_ptrs = ptrs;
}

pub var mod: modules.Module = .{
    .name = "acpi",
    .init = init,
    .deps = &@as([4]*modules.Module, .{ &multiboot.mod, &virtual.mod, &heap.mod, &@import("alloc/frames.zig").mod }),
};
