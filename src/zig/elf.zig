const std = @import("std");
const virtual = @import("virtual.zig");
const multiboot = @import("multiboot.zig");
const modules = @import("modules.zig");
const console = @import("console.zig");
const paging = @import("arch/x86/paging.zig");

pub const ELFSectionType = enum(u32) {
    null,
    progbits,
    symtab,
    strtab,
    rela,
    hash,
    dynamic,
    note,
    nobits,
    rel,
    shlib,
    dynsym,
    loproc,
    hiproc,
    louser,
    hiuser,
};

pub const ELFSectionHeader = extern struct {
    name: u32,
    section_type: ELFSectionType,
    flags: u32,
    vaddr: u32,
    foffset: u32,
    size: u32,
    link: u32,
    info: u32,
    addralign: u32,
    entsize: u32,
};

const elf_section_flag_alloc: u32 = 0x2;

pub var trampoline_region: ?virtual.Vregion = null;

pub const ELFInitError = error{
    ELFNoSections,
} || multiboot.MultibootTagError;

fn init() modules.ModuleInitError!void {
    const elf_tag = try multiboot.multibootFindTag(multiboot.MultibootTagELFSections);
    const section_headers: []ELFSectionHeader = (@as([*]ELFSectionHeader, @ptrCast(&elf_tag.first_section)))[0..elf_tag.num];
    var start: ?usize = null;
    var end: ?usize = null;
    for (section_headers) |h| {
        if ((h.section_type == .null) or (h.flags & elf_section_flag_alloc == 0)) {
            continue;
        }
        start = @min(start orelse std.math.maxInt(usize), h.vaddr);
        end = @max(end orelse 0, h.vaddr + h.size);
    }
    const start_res = start orelse return error.ELFNoSections;
    const end_res = end orelse return error.ELFNoSections;
    trampoline_region = .{
        .vaddr = start_res,
        .paddr = start_res,
        .pg_cnt = (end_res - start_res + paging.page_size - 1) / paging.page_size,
    };
    paging.registerRegion(&trampoline_region.?);
}

pub var mod: modules.Module = .{
    .name = "elf",
    .init = init,
    .deps = &@as([2]*modules.Module, .{ &multiboot.mod, &console.mod }),
};
