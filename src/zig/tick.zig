const scheduler = @import("scheduler.zig");

pub fn tick() void {
    scheduler.preempt();
}
