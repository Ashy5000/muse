const std = @import("std");
const global = @import("../global.zig");
const paging = @import("../arch.zig").paging;
const mmap = @import("../mmap.zig");
const virtual = @import("../virtual.zig");
const modules = @import("../modules.zig");
const bitmaps = @import("../utils/bitmaps.zig");

const min_chunk_size: usize = paging.page_size;
const min_chunk_size_log: usize = std.math.log2_int(usize, min_chunk_size);
const max_chunk_size: usize = 512 * 1024; // 512K
const max_chunk_size_log: usize = std.math.log2_int(usize, max_chunk_size);
pub const tier_cnt: usize = max_chunk_size_log - min_chunk_size_log + 1;

fn setStatusWithinRegion(
    region: global.Region,
    addr: usize,
    size: usize,
    status: bool,
) void {
    const tier_idx = std.math.log2_int_ceil(usize, size) - min_chunk_size_log;
    const tiers = region.bitmaps.?;
    const chunk_size = min_chunk_size << @intCast(tier_idx);
    const idx = (addr - region.start) / chunk_size;
    if (status) {
        if (chunk_size == min_chunk_size) {
            bitmaps.bitmapSet(tiers[tier_idx], idx, true);
            return;
        }

        setStatusWithinRegion(region, addr, chunk_size / 2, true);
        setStatusWithinRegion(region, addr + chunk_size / 2, size - chunk_size / 2, true);

        // This produces redundant writes with recursion, but my suspicion is
        // that it is faster than an extra branch to break the loop when the
        // status isn't changed.
        for (tier_idx..tiers.len) |i| {
            bitmaps.bitmapSet(tiers[i], idx >> @intCast(i - tier_idx), true);
        }
    } else {
        if (size == chunk_size) {
            bitmaps.bitmapSet(tiers[tier_idx], idx, false);
            if (chunk_size == max_chunk_size) {
                @branchHint(.cold);
                return;
            }
            const buddy_idx: usize = idx ^ 1;
            if (!bitmaps.bitmapGet(tiers[tier_idx], buddy_idx)) {
                const parent_idx: usize = idx / 2;
                bitmaps.bitmapSet(tiers[tier_idx], parent_idx, false);
            }
        }

        setStatusWithinRegion(region, addr, chunk_size / 2, false);
        setStatusWithinRegion(region, addr + chunk_size / 2, size - chunk_size / 2, false);
    }
}

pub const AllocError = error{NoPhysMem} || modules.InitError;

fn pmmAllocInRegion(
    req_size: usize,
    region: global.Region,
) AllocError![*]align(paging.page_size) u8 {
    const size = std.mem.Alignment.forward(
        std.mem.Alignment.fromByteUnits(min_chunk_size),
        req_size,
    );
    const tier_idx = std.math.log2_int_ceil(usize, size) - min_chunk_size_log;
    const tiers = region.bitmaps.?;
    const idx = bitmaps.bitmapFind(
        tiers[tier_idx],
    ) orelse return error.NoPhysMem;
    const addr: usize = region.start + idx * (min_chunk_size << @intCast(
        tier_idx,
    ));
    setStatusWithinRegion(region, addr, size, true);
    return @ptrFromInt(addr);
}

/// Allocates a region of physical memory with a given size and returns its
/// physical address, guarenteed to be aligned to its size rounded up to the
/// next power of 2.
pub fn pmmAlloc(req_size: usize) AllocError![*]align(paging.page_size) u8 {
    const info = (try mod.data(*Payload)).info;
    for (&info.regions) |region| {
        if (region.start < 0xffffffff) {
            continue;
        }
        return pmmAllocInRegion(req_size, region) catch continue;
    }
    for (&info.regions) |region| {
        if (region.start >= 0xffffffff) {
            continue;
        }
        return pmmAllocInRegion(req_size, region) catch continue;
    }
    return error.NoPhysMem;
}

pub fn pmmAllocLow(req_size: usize) AllocError![*]align(paging.page_size) u8 {
    const info = (try mod.data(*Payload)).info;
    for (info.regions) |region| {
        if (region.start > 0xffffffff) {
            continue;
        }
        const region_trunc: global.Region = .{
            .start = region.start,
            .pg_cnt = @min(
                region.pg_cnt,
                (0xffffffff - region.start) / paging.page_size,
            ),
            .bitmaps = region.bitmaps,
        };
        return pmmAllocInRegion(req_size, region_trunc) catch continue;
    }
    return error.NoPhysMem;
}

pub const StatusError = error{InvalidAddress} || modules.InitError;

/// If `status` is true, sets the status of the physical page at physical
/// address `addr` as in use. Otherwise, frees the physical page at the
/// address. If the status of the page is equal to the requested status,
/// no error is returned. This means that double-frees do not produce an error.
pub fn pmmSetStatus(addr: usize, req_size: usize, status: bool) StatusError!void {
    const info = (try mod.data(*Payload)).info;
    const size = std.mem.Alignment.forward(
        std.mem.Alignment.fromByteUnits(min_chunk_size),
        req_size,
    );
    for (0..info.region_cnt) |i| {
        const vr: global.Region = info.regions[i];
        const start: usize = vr.start;
        const end: usize = start + vr.pg_cnt * paging.page_size;
        if (addr >= start and addr < end) {
            setStatusWithinRegion(vr, addr, size, status);
            return;
        }
    }
    return error.InvalidAddress;
}

// "Resource deallocation must succeed."
// - zig zen
pub fn pmmFree(addr: usize, size: usize) void {
    pmmSetStatus(addr, size, false) catch return;
}

pub const Payload = struct {
    info: *allowzero align(paging.page_size) global.CoreInfo,
    global_vr: virtual.Vregion,
};

fn init() modules.InitError!void {
    const info = try mmap.mod.data(*allowzero align(paging.page_size) global.CoreInfo);
    for (0..info.region_cnt) |i| {
        var region = &info.regions[i];
        var tiers: [tier_cnt][]bitmaps.BitmapUnit = undefined;
        for (min_chunk_size_log..max_chunk_size_log + 1, 0..tier_cnt) |tier_idx, j| {
            const chunk_size = @as(usize, 1) << @intCast(tier_idx);
            const bit_cnt = (region.pg_cnt * paging.page_size) / chunk_size;
            const len = (bit_cnt + @bitSizeOf(bitmaps.BitmapUnit) - 1) / @bitSizeOf(bitmaps.BitmapUnit);
            const bitmap_ptr: [*]bitmaps.BitmapUnit = @ptrFromInt(@intFromPtr(info) + info.size);
            const bitmap: []bitmaps.BitmapUnit = bitmap_ptr[0..len];
            @memset(bitmap, 0);
            const extra_bits: usize = len * @bitSizeOf(bitmaps.BitmapUnit) - bit_cnt;
            // TODO: do this more efficiently with ~extra_bits +% 1, u3 instead
            // of usize, and some @truncate action.
            const invalid_chunks: usize = if (extra_bits > 0)
                @bitSizeOf(bitmaps.BitmapUnit) - extra_bits
            else
                0;
            for (0..invalid_chunks) |k| {
                const bit_idx: std.math.Log2Int(bitmaps.BitmapUnit) = @intCast(@bitSizeOf(bitmaps.BitmapUnit) - 1 - k);
                bitmap[bitmap.len - 1] |= @as(bitmaps.BitmapUnit, 1) << bit_idx;
            }
            info.size += len;
            tiers[j] = bitmap;
        }
        region.bitmaps = tiers;
    }

    // Used for local variables with a global lifetime, like with C's static keyword.
    const Static = struct {
        var payload: Payload = undefined;
    };
    Static.payload = .{
        .info = info,
        .global_vr = .{
            .vaddr = @ptrCast(@alignCast(info)),
            .paddr = @ptrCast(@alignCast(info)),
            .pg_cnt = (info.size + paging.page_size - 1) / paging.page_size,
        },
    };

    mod.payload = &Static.payload;
    pmmSetStatus(@intFromPtr(info), info.size, true) catch unreachable;
}

pub var mod: modules.Module = .{
    .name = "pmm",
    .init = init,
};
