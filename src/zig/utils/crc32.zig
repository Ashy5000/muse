const table: [256]u32 = init: {
    var res: [256]u32 = undefined;
    var crc32: u32 = 1;
    var i: u32 = 128;
    while (i > 0) : (i >>= 1) {
        crc32 = (crc32 >> @as(u6, 1)) ^ ((crc32 & 1) * 0xedb88320);
        var j: u32 = 0;
        while (j < 256) : (j += 2 * i) {
            res[i + j] = crc32 ^ res[j];
        }
    }
    break :init res;
};

pub fn calc(data: []const u8) u32 {
    var crc32: u32 = 0xffffffff;
    for (data) |b| {
        crc32 ^= b;
        crc32 = (crc32 >> @as(u6, 8)) ^ table[crc32 & 0xFF];
    }
    crc32 ^= 0xffffffff;
    return crc32;
}
