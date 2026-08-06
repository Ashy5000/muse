pub const drivers_display = [_]*@import("subsystems/display.zig").Driver{
    &@import("drivers/video/vga.zig").driver_display,
};
pub const drivers_timer = [_]*@import("subsystems/timer.zig").Driver{
    &@import("drivers/time/hpet.zig").driver_timer,
};
pub const drivers_pci = [_]*@import("subsystems/pci.zig").Driver{
    &@import("drivers/massStorage/ata.zig").driver_pci,
};
