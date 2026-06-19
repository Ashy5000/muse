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

const MultibootTag = struct {
    tag_type: TagType,
    size: u32,
};

const MultibootFramebufferType = enum(u8) {
    indexed,
    rgb,
    ega_text,
};

pub const MultibootTagFramebuffer = struct {
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

pub const MultibootInfo = struct {
    size: u32,
    rsvd: u32,
    first_tag: MultibootTag,
};

const multiboot2_magic: u32 = 0x36D76289;

var multiboot_magic: ?u32 = null;
var multiboot_info: ?*MultibootInfo = null;

const MultibootInfoError = error{
    MultibootNoInfo,
};

pub const MultibootInitError = error{
    MultibootInvalidMagic,
} || MultibootInfoError;

pub fn config(info: *MultibootInfo, magic: u32) void {
    multiboot_magic = magic;
    multiboot_info = info;
}

pub fn init() MultibootInitError!void {
    if ((multiboot_magic orelse return error.MultibootNoInfo) != multiboot2_magic) {
        return error.MultibootInvalidMagic;
    }
    _ = multiboot_info orelse return error.MultibootNoInfo;
}

pub const MultibootTagError = error{
    MultibootTagNotFound,
} || MultibootInfoError;

pub fn multibootFindTag(res_type: type) MultibootTagError!*res_type {
    const struct_info = @typeInfo(res_type).@"struct";
    const field_type = struct_info.field_types[0];
    if (field_type != TagType) {
        @compileError("Invalid multiboot tag struct: first field should be a TagType.");
    }
    const tag_type = struct_info.field_attrs[0].defaultValue(field_type);
    const info = multiboot_info orelse return error.MultibootNoInfo;
    var tag: *MultibootTag = &info.first_tag;
    while (@intFromPtr(tag) < @intFromPtr(info) + info.size) {
        if (tag.tag_type == tag_type) {
            return @ptrCast(tag);
        }
        var tag_addr = @intFromPtr(tag) + tag.size;
        if (tag_addr % 8 > 0) {
            tag_addr += 8 - (tag_addr % 8);
        }
        tag = @ptrFromInt(tag_addr);
    }
    return error.MultibootTagNotFound;
}
