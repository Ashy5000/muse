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
    pg_cnt: usize,
    bitmap: []bitmaps.BitmapUnit,
};

pub const FrameAllocError = error{NoVirtFrames} || modules.InitError;

pub fn alloc() FrameAllocError![*]align(paging.page_size) u8 {
    const payload = try mod.data();
    const idx: usize = bitmaps.bitmapAlloc(
        payload.bitmap,
    ) orelse return error.FrameAllocNoMem;
    return @alignCast(payload.start + (idx * paging.page_size));
}

pub fn allocContig(cnt: usize) FrameAllocError![*]align(paging.page_size) u8 {
    const payload = try mod.data();
    const idx: usize = bitmaps.bitmapAllocContig(
        payload.bitmap,
        cnt,
    ) orelse return error.NoVirtFrames;
    return @alignCast(payload.start + (idx * paging.page_size));
}

pub fn setStatus(
    start: [*]align(paging.page_size) u8,
    cnt: usize,
    in_use: bool,
) modules.InitError!void {
    const payload = try mod.data();
    const start_idx = (@intFromPtr(start) -| @intFromPtr(payload.start)) / paging.page_size;
    const end = @intFromPtr(start) + cnt * paging.page_size;
    const end_idx = @min(
        payload.pg_cnt,
        (end -| @intFromPtr(payload.start)) / paging.page_size,
    );
    for (start_idx..end_idx) |idx| {
        bitmaps.bitmapSet(payload.bitmap, idx, in_use);
    }
}

pub fn free(start: [*]align(paging.page_size) u8, cnt: usize) void {
    setStatus(start, cnt, false) catch unreachable;
}

const multiboot = @import("../multiboot.zig");

fn init() modules.InitError!Payload {
    const trampoline_region = try elf.mod.data();
    const start: usize = paging.page_align.forward(@intFromPtr(
        trampoline_region.vaddr + trampoline_region.pg_cnt * paging.page_size,
    ));
    const end: usize = start + paging.page_size * 4096;
    const pg_cnt = (end - start) / paging.page_size;
    const bitmap_cnt: usize = pg_cnt / 8;
    const allocator = try heap.mod.data();
    const bitmap: []bitmaps.BitmapUnit = allocator.alloc(
        bitmaps.BitmapUnit,
        bitmap_cnt,
    ) catch return error.InitializationFailure;
    @memset(bitmap, 0);

    const payload: Payload = .{
        .start = @ptrFromInt(start),
        .pg_cnt = pg_cnt,
        .bitmap = bitmap,
    };
    mod.payload = payload;
    const multiboot_vr = (try multiboot.mod.data()).multiboot_vr;
    setStatus(
        @ptrCast(multiboot_vr.vaddr),
        multiboot_vr.pg_cnt,
        true,
    ) catch unreachable;
    return payload;
}

/// The frames module, which initializes an allocator for allocating virtual
/// page-sized chunks of memory.
pub var mod: modules.Module(Payload) = .{
    .name = "frames",
    .init = init,
};
