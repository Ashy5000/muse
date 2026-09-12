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
    addr: [*]allowzero align(paging.page_size) u8,
    size: usize,
    status: bool,
) void {
    const tier_idx = std.math.log2_int_ceil(usize, size) - min_chunk_size_log;
    const tiers = region.bitmaps.?;
    const chunk_size = min_chunk_size << @intCast(tier_idx);
    const idx = (@intFromPtr(addr) - @intFromPtr(region.start)) / chunk_size;
    if (status) {
        // This produces redundant writes with recursion, but my suspicion is
        // that it is faster than an extra branch to break the loop when the
        // status isn't changed.
        for (tier_idx..tiers.len) |i| {
            bitmaps.bitmapSet(tiers[i], idx >> @intCast(i - tier_idx), true);
        }
        if (chunk_size == min_chunk_size) return;

        setStatusWithinRegion(region, addr, chunk_size / 2, true);
        setStatusWithinRegion(
            region,
            @alignCast(addr + chunk_size / 2),
            size - chunk_size / 2,
            true,
        );
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
        setStatusWithinRegion(
            region,
            @alignCast(addr + chunk_size / 2),
            size - chunk_size / 2,
            false,
        );
    }
}

pub const AllocError = error{NoPhysMem} || modules.InitError;

fn allocInRegion(
    pg_cnt: usize,
    region: global.Region,
) AllocError![*]align(paging.page_size) u8 {
    const size = pg_cnt * paging.page_size;
    const tier_idx = std.math.log2_int_ceil(usize, size) - min_chunk_size_log;
    const tiers = region.bitmaps.?;
    const bitmap = tiers[tier_idx];
    const idx = bitmaps.bitmapFind(bitmap) orelse return error.NoPhysMem;
    const start: [*]allowzero align(paging.page_size) u8 = region.start;
    const addr: [*]align(paging.page_size) u8 = @ptrCast(@alignCast(
        start + idx * (min_chunk_size << @intCast(
            tier_idx,
        )),
    ));
    setStatusWithinRegion(region, addr, size, true);
    return addr;
}

pub fn alloc(pg_cnt: usize) AllocError![*]align(paging.page_size) u8 {
    const info = (try mod.data()).info;
    for (&info.regions) |region| {
        if (@intFromPtr(region.start) < 0xffffffff) {
            continue;
        }
        return allocInRegion(pg_cnt, region) catch continue;
    }
    for (&info.regions) |region| {
        if (@intFromPtr(region.start) >= 0xffffffff) {
            continue;
        }
        return allocInRegion(pg_cnt, region) catch continue;
    }
    return error.NoPhysMem;
}

pub fn allocLow(req_size: usize) AllocError![*]align(paging.page_size) u8 {
    const info = (try mod.data()).info;
    for (info.regions) |region| {
        if (@intFromPtr(region.start) > 0xffffffff) {
            continue;
        }
        const region_trunc: global.Region = .{
            .start = region.start,
            .pg_cnt = @min(
                region.pg_cnt,
                (0xffffffff - @intFromPtr(region.start)) / paging.page_size,
            ),
            .bitmaps = region.bitmaps,
        };
        return allocInRegion(req_size, region_trunc) catch continue;
    }
    return error.NoPhysMem;
}

pub fn setStatus(
    addr: [*]allowzero align(paging.page_size) u8,
    pg_cnt: usize,
    status: bool,
) modules.InitError!void {
    const addr_int = @intFromPtr(addr);
    const info = (try mod.data()).info;
    for (0..info.region_cnt) |i| {
        const vr: global.Region = info.regions[i];
        const start: usize = @intFromPtr(vr.start);
        const region_end: usize = start + vr.pg_cnt * paging.page_size;
        if (addr_int >= region_end) continue;
        const end: usize = @min(
            start + vr.pg_cnt * paging.page_size,
            @intFromPtr(addr) + pg_cnt * paging.page_size,
        );
        const size_trunc: usize = end - @intFromPtr(addr);
        setStatusWithinRegion(vr, addr, size_trunc, status);
        return;
    }
}

pub fn free(addr: [*]align(paging.page_size) u8, pg_cnt: usize) void {
    setStatus(addr, pg_cnt, false) catch return;
}

pub const Payload = struct {
    info: *allowzero align(paging.page_size) global.CoreInfo,
    global_vr: virtual.Vregion,
};

fn init() modules.InitError!Payload {
    var info = try mmap.mod.data();
    for (0..info.region_cnt) |i| {
        var region = &info.regions[i];
        var tiers: [tier_cnt][]bitmaps.BitmapUnit = undefined;
        for (min_chunk_size_log..max_chunk_size_log + 1, 0..tier_cnt) |tier_idx, j| {
            const chunk_size = @as(usize, 1) << @intCast(tier_idx);
            const bit_cnt = (region.pg_cnt * paging.page_size) / chunk_size;
            const len = (bit_cnt + @bitSizeOf(
                bitmaps.BitmapUnit,
            ) - 1) / @bitSizeOf(bitmaps.BitmapUnit);
            const bitmap_ptr: [*]bitmaps.BitmapUnit = @ptrFromInt(
                @intFromPtr(info) + info.size,
            );
            const bitmap: []bitmaps.BitmapUnit = bitmap_ptr[0..len];
            @memset(bitmap, 0);
            const extra_bits: usize = len * @bitSizeOf(
                bitmaps.BitmapUnit,
            ) - bit_cnt;
            // TODO: do this more efficiently with ~extra_bits +% 1, u3 instead
            // of usize, and some @truncate action.
            const invalid_chunks: usize = if (extra_bits > 0)
                @bitSizeOf(bitmaps.BitmapUnit) - extra_bits
            else
                0;
            for (0..invalid_chunks) |k| {
                const bit_idx: std.math.Log2Int(bitmaps.BitmapUnit) = @intCast(
                    @bitSizeOf(bitmaps.BitmapUnit) - 1 - k,
                );
                bitmap[bitmap.len - 1] |= @as(bitmaps.BitmapUnit, 1) << bit_idx;
            }
            info.size += len;
            tiers[j] = bitmap;
        }
        region.bitmaps = tiers;
    }

    const global_pg_cnt = (info.size + paging.page_size - 1) / paging.page_size;
    mod.payload = .{
        .info = info,
        .global_vr = .{
            .vaddr = @ptrCast(@alignCast(info)),
            .paddr = @ptrCast(@alignCast(info)),
            .pg_cnt = global_pg_cnt,
        },
    };

    setStatus(
        @ptrCast(@alignCast(info)),
        global_pg_cnt,
        true,
    ) catch unreachable;
    const multiboot = @import("../multiboot.zig");
    const multiboot_vr = (try multiboot.mod.data()).multiboot_vr;
    setStatus(
        multiboot_vr.vaddr,
        multiboot_vr.pg_cnt,
        true,
    ) catch unreachable;
    return mod.payload.?;
}

pub var mod: modules.Module(Payload) = .{
    .name = "pmm",
    .init = init,
};
