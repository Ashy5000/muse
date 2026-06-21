pub const Vregion = struct {
    vaddr: usize,
    paddr: usize,
    pg_cnt: usize,
    next: ?*Vregion = null,
};
