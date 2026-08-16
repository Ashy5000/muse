const std = @import("std");
const console = @import("console.zig");

pub const InitError = error{
    InitializationFailure,
    CriticalSystemFailure,
    Unsupported,
};

pub fn Module(comptime T: type) type {
    return struct {
        const Self = @This();

        name: []const u8,
        payload: ?T = null,
        err: ?InitError = null,
        init: *const fn () InitError!T,

        pub fn data(self: *Self) InitError!T {
            if (self.err) |err| {
                return err;
            }
            if (self.payload) |res| return res else {
                const res = self.init() catch |err| {
                    self.err = err;
                    return err;
                };
                self.payload = res;
                return res;
            }
        }

        pub fn data_ref(self: *Self) InitError!*T {
            if (self.err) |err| {
                return err;
            }
            if (self.payload) |*res| return res else {
                try self.load();
                return &self.payload.?;
            }
        }

        pub fn load(self: *Self) InitError!void {
            if (self.err) |err| {
                return err;
            }
            self.payload = self.init() catch |err| {
                self.err = err;
                return err;
            };
        }
    };
}
