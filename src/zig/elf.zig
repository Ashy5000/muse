const std = @import("std");
const virtual = @import("virtual.zig");
const multiboot = @import("multiboot.zig");
const modules = @import("modules.zig");
const console = @import("console.zig");
const paging = @import("arch.zig").paging;

/// The type of an ELF section.
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

/// A header for an ELF section.
pub const ELFSectionHeader = extern struct {
    name: u32,
    section_type: ELFSectionType,
    flags: usize,
    vaddr: usize,
    foffset: usize,
    size: usize,
    link: u32,
    info: u32,
    addralign: usize,
    entsize: usize,
};

const elf_section_flag_alloc: u32 = 0x2;

var trampoline_region: virtual.Vregion = undefined;

fn init() modules.InitError!void {
    const elf_tag = multiboot.multibootFindTag(multiboot.MultibootTagELFSections) catch return error.Unsupported;
    var start: ?usize = null;
    var end: ?usize = null;
    var offset: usize = 0;
    while (offset < elf_tag.num * elf_tag.entsize) : (offset += elf_tag.entsize) {
        const h: *align(1) ELFSectionHeader = @ptrFromInt(@intFromPtr(&elf_tag.first_section) + offset);
        if ((h.section_type == .null) or (h.flags & elf_section_flag_alloc == 0)) {
            continue;
        }
        start = @min(start orelse std.math.maxInt(usize), h.vaddr);
        end = @max(end orelse 0, h.vaddr + h.size);
    }
    const start_res = start orelse return error.InitializationFailure;
    const end_res = end orelse return error.InitializationFailure;
    const start_ptr: [*]align(paging.page_size) u8 = @ptrFromInt(paging.page_align.forward(start_res));
    trampoline_region = .{
        .vaddr = start_ptr,
        .paddr = start_ptr,
        .pg_cnt = (end_res - start_res + paging.page_size - 1) / paging.page_size,
    };
    paging.registerRegion(&trampoline_region);
    mod.payload = @ptrCast(&trampoline_region);
}

/// The elf module, which parses information about the prekernel ELF file.
pub var mod: modules.Module = .{
    .name = "elf",
    .init = init,
};
