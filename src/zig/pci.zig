const io = @import("utils/io.zig");

const config_addr: io.Port = 0xcf8;
const config_data: io.Port = 0xcfc;

const PCIAddr = packed struct {
    reg_offset: u8,
    func: u3,
    dev: u5,
    bus: u8,
    rsvd: u7,
    enable: bool,
};
