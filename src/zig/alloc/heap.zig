const std = @import("std");
const virtual = @import("../virtual.zig");
const modules = @import("../modules.zig");
const paging = @import("../arch.zig").paging;
const global = @import("../global.zig");
const pmm = @import("../alloc/pmm.zig");
const elf = @import("../elf.zig");

const min_chunk_size_log: usize = 5;
const min_chunk_size: usize = (1 << min_chunk_size_log);
const tier_cnt: usize = @bitSizeOf(usize) - min_chunk_size_log;
const chunk_overhead: usize = (2 * @sizeOf(usize));

const Heap = struct {
    size: usize,
    free: usize,
    tiers: [tier_cnt]?*align(1) Chunk,
    limit: [*]u8,
    max_limit: [*]u8,
};

const Chunk = extern struct {
    prev_size: usize,
    size: usize,
    free: bool,
    next: ?*align(1) Chunk,
};

fn heapSbrk(heap: *Heap, inc: usize) ?[*]u8 {
    const prev_lim: [*]u8 = heap.limit;
    if (@intFromPtr(prev_lim) + inc > @intFromPtr(heap.max_limit)) {
        return null;
    }
    virtual.backSlice(heap.limit[0..inc]) catch return null;
    heap.limit += inc;
    heap.size += inc;
    return prev_lim;
}

fn alloc(heap_opaque: *anyopaque, len: usize, alignment: std.mem.Alignment, _: usize) ?[*]u8 {
    const heap: *Heap = @ptrCast(@alignCast(heap_opaque));
    const size: usize = @max(len + chunk_overhead, min_chunk_size);
    const t_idx: usize = @bitSizeOf(usize) - 1 - @clz(size) - min_chunk_size_log;
    var previous_chunk: ?*align(1) Chunk = null;
    var current_chunk: ?*align(1) Chunk = heap.tiers[t_idx];
    while (current_chunk) |ch| {
        const aligned_start: usize = std.mem.Alignment.forward(alignment, @intFromPtr(ch) + chunk_overhead);
        const offset: usize = aligned_start - chunk_overhead - @intFromPtr(ch);

        if (@intFromPtr(ch) + ch.size < aligned_start - chunk_overhead + size) {
            previous_chunk = ch;
            current_chunk = ch.next;
            continue;
        }

        const o_ch: Chunk = ch.*;

        const n_ch: *align(1) Chunk = @ptrFromInt(aligned_start - chunk_overhead);
        n_ch.prev_size = 0;
        n_ch.size = o_ch.size - offset;

        if (previous_chunk) |prev| {
            prev.next = o_ch.next;
        } else {
            heap.tiers[t_idx] = o_ch.next;
        }

        if (ch.prev_size > 0) {
            const p_ch: *align(1) Chunk = @ptrFromInt(@intFromPtr(ch) - o_ch.prev_size);
            p_ch.size += offset;
            n_ch.prev_size = p_ch.size;
        }

        const s_ch: *align(1) Chunk = @ptrFromInt(@intFromPtr(n_ch) + n_ch.size);
        s_ch.prev_size = n_ch.size;

        return @ptrFromInt(aligned_start);
    }

    const prev_size: *align(1) usize = @ptrFromInt(@intFromPtr(heap.limit) - @sizeOf(usize));

    const aligned_start: usize = std.mem.Alignment.forward(alignment, @intFromPtr(heap.limit) - @sizeOf(usize) + chunk_overhead);
    const end: usize = aligned_start - chunk_overhead + size;

    const n_ch: *align(1) Chunk = @ptrFromInt(aligned_start - chunk_overhead);
    n_ch.size = size;
    n_ch.prev_size = prev_size.*;

    const limit_old = heapSbrk(heap, end + @sizeOf(usize) - @intFromPtr(heap.limit)) orelse return null;

    if (prev_size.* > 0) {
        const p_ch: *align(1) Chunk = @ptrFromInt(@intFromPtr(limit_old) - @sizeOf(usize) - n_ch.prev_size);
        p_ch.size += @intFromPtr(n_ch) - (@intFromPtr(limit_old) - @sizeOf(usize));
    }
    const prev_size_next: *align(1) usize = @ptrFromInt(@intFromPtr(heap.limit) - @sizeOf(usize));
    prev_size_next.* = n_ch.size;
    return @ptrFromInt(aligned_start);
}

fn free(heap_opaque: *anyopaque, memory: []u8, _: std.mem.Alignment, _: usize) void {
    const heap: *Heap = @ptrCast(@alignCast(heap_opaque));
    var ch: *align(1) Chunk = @ptrCast(memory.ptr - chunk_overhead);
    const s_ch: *align(1) Chunk = @ptrFromInt(@intFromPtr(ch) + ch.size);
    if (@intFromPtr(s_ch) + @sizeOf(Chunk) <= @intFromPtr(heap.limit)) {
        if (s_ch.free) {
            ch.size += s_ch.size;
        }
    }
    if (ch.prev_size > 0) {
        const p_ch: *align(1) Chunk = @ptrFromInt(@intFromPtr(ch) - ch.prev_size);
        if (p_ch.free) {
            p_ch.size += ch.size;
            ch = p_ch;
        }
    }
    ch.free = true;
    const t_idx: usize = @bitSizeOf(usize) - 1 - @clz(ch.size) - min_chunk_size_log;
    ch.next = heap.tiers[t_idx];
    heap.tiers[t_idx] = ch;
    heap.free += memory.len + chunk_overhead;
}

fn resize(_: *anyopaque, memory: []u8, _: std.mem.Alignment, new_len: usize, _: usize) bool {
    const ch: *align(1) Chunk = @ptrFromInt(@intFromPtr(memory.ptr) - chunk_overhead);
    return ch.size >= new_len;
}

fn remap(heap_opaque: *anyopaque, memory: []u8, alignment: std.mem.Alignment, new_len: usize, ret_addr: usize) ?[*]u8 {
    if (resize(heap_opaque, memory, alignment, new_len, ret_addr)) {
        return memory.ptr;
    }
    return null;
}

const vtable: std.mem.Allocator.VTable = .{
    .alloc = alloc,
    .resize = resize,
    .remap = remap,
    .free = free,
};

/// An error that occured while creating an std.mem.Allocator.
pub const AllocCreateError = error{
    AllocUninit,
};

var global_heap: Heap = undefined;

fn init() modules.InitError!std.mem.Allocator {
    const region = try elf.mod.data();
    const info = (try pmm.mod.data()).info;
    global_heap = .{
        .limit = @ptrFromInt(@intFromPtr(info) + info.size + 1),
        .max_limit = @ptrCast(region.vaddr),
        .size = 0,
        .free = 0,
        .tiers = @splat(null),
    };
    const prev_size: *align(1) usize = @ptrCast(
        heapSbrk(&global_heap, @sizeOf(usize)).?,
    );
    prev_size.* = 0;
    return .{
        .ptr = &global_heap,
        .vtable = &vtable,
    };
}

/// The heap module, which initializes a heap for the kernel.
pub var mod: modules.Module(std.mem.Allocator) = .{
    .name = "heap",
    .init = init,
};
