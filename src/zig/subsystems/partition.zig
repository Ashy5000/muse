const vfs = @import("../vfs.zig");
const BlockDevice = @import("../BlockDevice.zig");

pub const Driver = struct {
    init: *const fn (dev: *BlockDevice) vfs.Vnode.TransferError!bool,
};

const drivers = @import("../drivers.zig");

pub fn probePartitions(dev: *BlockDevice) vfs.Vnode.TransferError!void {
    for (drivers.drivers_partition) |d| {
        if (try d.init(dev)) return;
    }
}
