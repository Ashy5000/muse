//! Defines a system for allocating page-sized chunks of virtual memory.

const std = @import("std");
const paging = @import("../arch.zig").paging;
const bitmaps = @import("../utils/bitmaps.zig");
const global = @import("../global.zig");
const heap = @import("heap.zig");
const elf = @import("../elf.zig");
const console = @import("../console.zig");
const modules = @import("../modules.zig");

const FrameAllocInfo = struct {
    start: [*]align(paging.page_size) u8,
    bitmap: []bitmaps.BitmapUnit,
};

pub const FrameAllocError = error{
    FrameAllocUninit,
    FrameAllocNoMem,
};

var frame_alloc_global: ?FrameAllocInfo = null;

/// Allocates a page-sized chunk of virtual memory, which may or may not be mapped to a physical page.
pub fn frameAlloc() FrameAllocError![*]align(paging.page_size) u8 {
    const info: FrameAllocInfo = frame_alloc_global orelse return error.FrameAllocUninit;
    const idx: usize = bitmaps.bitmapAlloc(info.bitmap) orelse return error.FrameAllocNoMem;
    return info.start + (idx * paging.page_size);
}

pub fn frameAllocContig(cnt: usize) FrameAllocError![*]align(paging.page_size) u8 {
    const info: FrameAllocInfo = frame_alloc_global orelse return error.FrameAllocUninit;
    const idx: usize = bitmaps.bitmapAllocContig(info.bitmap, cnt) orelse return error.FrameAllocNoMem;
    return @alignCast(info.start + (idx * paging.page_size));
}

/// Frees a page-sized chunk of virtual memory, not modifying its page mapping.
pub fn frameFree(start: *align(paging.page_size) u8) void {
    const info: FrameAllocInfo = frame_alloc_global orelse return;
    const idx = (start - info.start) / paging.page_size;
    bitmaps.bitmapSet(info.bitmap, idx, false);
}

fn init() modules.ModuleInitError!void {
    const start: usize = std.mem.Alignment.forward(paging.page_align, @intFromPtr(global.info) + global.info.size);
    const end: usize = elf.trampoline_region.?.vaddr;
    const bitmap_cnt: usize = ((end - start) / paging.page_size) / 8;
    const allocator = heap.allocator() catch return error.ModuleInitFailure;
    const bitmap: []bitmaps.BitmapUnit = allocator.alloc(bitmaps.BitmapUnit, bitmap_cnt) catch return error.ModuleInitFailure;
    frame_alloc_global = .{
        .start = @ptrFromInt(start),
        .bitmap = bitmap,
    };
}

pub var mod: modules.Module = .{
    .name = "frames",
    .init = init,
    .deps = &@as([4]*modules.Module, .{ &paging.mod, &global.mod, &heap.mod, &elf.mod }),
};
