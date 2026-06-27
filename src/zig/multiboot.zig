const std = @import("std");
const paging = @import("arch.zig").paging;
const elf = @import("elf.zig");
const virtual = @import("virtual.zig");
const acpi = @import("acpi.zig");
const modules = @import("modules.zig");

pub const TagType = enum(u32) {
    end,
    cmdline,
    bootloader_name,
    module,
    basic_meminfo,
    bootdev,
    mmap,
    vbe,
    framebuffer,
    elf_sections,
    apm,
    efi32,
    efi64,
    smbios,
    acpi_old,
    acpi_new,
    network,
    efi_mmap,
    efi_bs,
    efi32_ih,
    efi64_ih,
    load_base_addr,
};

const MultibootTag = extern struct {
    tag_type: TagType,
    size: u32,
};

pub const MultibootMmapType = enum(u32) {
    available = 1,
    reserved,
    acpi_reclaimable,
    nvs,
    bad_ram,
};

pub const MultibootMmapEntry = extern struct {
    addr: u64,
    len: u64,
    type: MultibootMmapType,
    rsvd: u32,
};

pub const MultibootTagMmap = extern struct {
    type: TagType = .mmap,
    size: u32,
    entry_size: u32,
    version: u32,
    first_entry: MultibootMmapEntry,
};

const MultibootFramebufferType = enum(u8) {
    indexed,
    rgb,
    ega_text,
};

pub const MultibootTagFramebuffer = extern struct {
    type: TagType = .framebuffer,
    size: u32,
    addr: u64,
    pitch: u32,
    width: u32,
    height: u32,
    bpp: u8,
    framebuffer_type: MultibootFramebufferType,
    rsvd: u16,
};

pub const MultibootTagELFSections = extern struct {
    type: TagType = .elf_sections,
    size: u32,
    num: u32,
    entsize: u32,
    shndx: u32,
    first_section: elf.ELFSectionHeader,
};

pub const MultibootTagAcpiOld = extern struct {
    type: TagType = .acpi_old,
    size: u32,
    rsdp: acpi.RSDPv1,
};

pub const MultibootTagAcpiNew = extern struct {
    type: TagType = .acpi_new,
    size: u32,
    rsdp: acpi.RSDPv2,
};

pub const MultibootInfo = extern struct {
    size: u32,
    rsvd: u32,
    first_tag: MultibootTag,
};

const multiboot2_magic: u32 = 0x36D76289;

var multiboot_magic: ?u32 = null;
pub var multiboot_info: ?*MultibootInfo = null;

const MultibootInfoError = error{
    MultibootNoInfo,
};

pub const MultibootInitError = error{
    MultibootInvalidMagic,
} || MultibootInfoError;

pub const MultibootTagError = error{
    MultibootTagNotFound,
} || MultibootInfoError;

pub fn multibootFindTag(res_type: type) MultibootTagError!*align(4) res_type {
    const struct_info = @typeInfo(res_type).@"struct";
    const field_type = struct_info.field_types[0];
    if (field_type != TagType) {
        @compileError("invalid multiboot tag struct: first field should be a TagType.");
    }
    const tag_type = struct_info.field_attrs[0].defaultValue(field_type);
    const info = multiboot_info orelse return error.MultibootNoInfo;
    var tag: *MultibootTag = &info.first_tag;
    while (@intFromPtr(tag) < @intFromPtr(info) + info.size) {
        if (tag.tag_type == tag_type) {
            return @ptrCast(tag);
        }
        const tag_addr = std.mem.Alignment.forward(std.mem.Alignment.@"8", @intFromPtr(tag) + tag.size);
        tag = @ptrFromInt(tag_addr);
    }
    return error.MultibootTagNotFound;
}

pub fn config(info: *MultibootInfo, magic: u32) void {
    multiboot_magic = magic;
    multiboot_info = info;
}

var multiboot_region: ?virtual.Vregion = null;

pub fn init() modules.ModuleInitError!void {
    if ((multiboot_magic orelse return error.ModuleMissingConfig) != multiboot2_magic) {
        return error.ModuleInitFailure;
    }
    const info = multiboot_info orelse return error.ModuleMissingConfig;
    const start = std.mem.Alignment.backward(paging.page_align, @intFromPtr(info));
    const end = std.mem.Alignment.forward(paging.page_align, @intFromPtr(info) + info.size);
    const pg_cnt = (end - start) / paging.page_size;
    multiboot_region = .{
        .vaddr = start,
        .paddr = start,
        .pg_cnt = pg_cnt,
    };
    paging.registerRegion(&multiboot_region.?);
}

pub var mod: modules.Module = .{
    .name = "multiboot2",
    .init = init,
};
