const std = @import("std");
const modules = @import("../modules.zig");
const scheduler = @import("../scheduler.zig");

pub const Mutex = enum {
    unlocked,
    locked,

    pub fn acquire(self: *Mutex) modules.InitError!void {
        while (@cmpxchgWeak(
            Mutex,
            self,
            .unlocked,
            .locked,
            .acquire,
            .monotonic,
        )) |_| try scheduler.preempt();
    }

    pub fn release(self: *Mutex) void {
        std.debug.assert(self.* == .locked);
        @atomicStore(Mutex, self, .unlocked, .release);
    }
};
