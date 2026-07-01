const console = @import("../console.zig");

pub const BitmapUnit = u8;

pub fn bitmapAlloc(bitmap: []BitmapUnit) ?usize {
    for (0..bitmap.len) |i| {
        for (0..@bitSizeOf(BitmapUnit)) |j| {
            if (((bitmap[i] >> @intCast(j)) & 1) == 0) {
                bitmap[i] |= @as(BitmapUnit, 1) << @as(u3, @intCast(j));
                return i * @bitSizeOf(BitmapUnit) + j;
            }
        }
    }
    return null;
}

pub fn bitmapAllocContig(bitmap: []BitmapUnit, cnt: usize) ?usize {
    if (cnt == 0) {
        @branchHint(.cold);
        return null;
    }
    var contig: usize = 0;
    var offset: usize = 0;
    for (0..bitmap.len) |i| top: {
        for (0..@bitSizeOf(BitmapUnit)) |j| {
            if (contig == cnt) {
                break :top;
            }
            if (((bitmap[i] >> @intCast(j)) & 1) == 0) {
                contig += 1;
            } else {
                contig = 0;
                offset = i * @bitSizeOf(BitmapUnit) + j + 1;
            }
        }
    }
    if (contig == cnt) {
        for (offset..offset + contig) |i| {
            bitmap[i / 8] |= @as(BitmapUnit, 1) << @as(u3, @intCast(i % @bitSizeOf(BitmapUnit)));
        }
        return offset;
    }
    return null;
}

pub fn bitmapSet(bitmap: []BitmapUnit, idx: usize, status: bool) void {
    const mask: BitmapUnit = @as(BitmapUnit, 1) << @as(u3, @intCast(idx % @bitSizeOf(BitmapUnit)));
    if (status) {
        bitmap[idx / @bitSizeOf(BitmapUnit)] |= mask;
    } else {
        bitmap[idx / @bitSizeOf(BitmapUnit)] &= ~mask;
    }
}
