//! Defines a system for allocating page-sized chunks of virtual memory.

const std = @import("std");
const paging = @import("../arch.zig").paging;
const bitmaps = @import("../utils/bitmaps.zig");
const heap = @import("heap.zig");
const elf = @import("../elf.zig");
const console = @import("../console.zig");
const modules = @import("../modules.zig");

const Payload = struct {
    start: [*]align(paging.page_size) u8,
    bitmap: []bitmaps.BitmapUnit,
};

pub const FrameAllocError = error{NoVirtFrames} || modules.InitError;

/// Allocates a page-sized chunk of virtual memory, which may or may not be
/// mapped to a physical page.
pub fn frameAlloc() FrameAllocError![]align(paging.page_size) u8 {
    const payload = try mod.data();
    const idx: usize = bitmaps.bitmapAlloc(
        payload.bitmap,
    ) orelse return error.FrameAllocNoMem;
    return payload.start + (idx * paging.page_size);
}

/// Allocates `cnt` contiguous page-sized chunks of virtual memory, which
/// may or may not be mapped to physical pages.
pub fn frameAllocContig(cnt: usize) FrameAllocError![]align(paging.page_size) u8 {
    const payload = try mod.data();
    const idx: usize = bitmaps.bitmapAllocContig(
        payload.bitmap,
        cnt,
    ) orelse return error.NoVirtFrames;
    return @alignCast(
        (payload.start + (idx * paging.page_size))[0 .. cnt * paging.page_size],
    );
}

/// Frees a page-sized chunk of virtual memory, not modifying its page mapping.
pub fn frameFree(start: usize) void {
    const payload: *Payload = &mod.payload.?; // Page being freed must be allocated, so module must be inited.
    const idx = (start - @intFromPtr(payload.start)) / paging.page_size;
    bitmaps.bitmapSet(payload.bitmap, idx, false);
}

fn init() modules.InitError!Payload {
    const trampoline_region = try elf.mod.data();
    const start: usize = std.mem.Alignment.forward(paging.page_align, @intFromPtr(
        trampoline_region.vaddr + trampoline_region.pg_cnt * paging.page_size,
    ));
    const end: usize = start + paging.page_size * 4096;
    const bitmap_cnt: usize = ((end - start) / paging.page_size) / 8;
    const allocator = try heap.mod.data();
    const bitmap: []bitmaps.BitmapUnit = allocator.alloc(
        bitmaps.BitmapUnit,
        bitmap_cnt,
    ) catch return error.InitializationFailure;
    @memset(bitmap, 0);

    return .{
        .start = @ptrFromInt(start),
        .bitmap = bitmap,
    };
}

/// The frames module, which initializes an allocator for allocating virtual
/// page-sized chunks of memory.
pub var mod: modules.Module(Payload) = .{
    .name = "frames",
    .init = init,
};
