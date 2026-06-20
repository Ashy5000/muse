pub const Region = struct {
    start: usize,
    pg_cnt: usize,
};

pub const CoreInfo = struct {
    regions: [16]Region,
    region_cnt: usize,
};

pub var info: *allowzero CoreInfo = @ptrFromInt(0);
