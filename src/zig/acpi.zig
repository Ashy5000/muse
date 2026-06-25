const RSDPv1 = packed struct {
    signature: [8]u8,
    checksum: u8,
    oem_id: [6]u8,
    rev: u8,
    rsdt_addr: u32,
};

const RSDPv2 = packed struct {
    signature: [8]u8,
    checksum: u8,
    oem_id: [6]u8,
    rev: u8,
    rsdt_addr: u32,
    len: u32,
    xsdt_addr: u64,
    checksum_ext: u8,
    rsvd: [3]u8,
};

const ACPISDTHeader = packed struct {
    signature: [4]u8,
    length: u32,
    rev: u8,
    checksum: u8,
    oem_id: [6]u8,
    oem_table_id: [8]u8,
    oem_rev: u32,
    creator_id: u32,
    creator_rev: u32,
};

const RSDT = packed struct {
    header: ACPISDTHeader,
    first_sdt_ptr: u32,
};

const XSDT = packed struct {
    header: ACPISDTHeader,
    first_sdt_ptr: u64,
};
