const std = @import("std");
const console = @import("console.zig");

pub const InitError = error{
    InitializationFailure,
    CriticalSystemFailure,
    Unsupported,
};

fn initEmpty() !void {}

pub const Module = struct {
    name: []const u8,
    payload: ?*allowzero anyopaque = null,
    err: ?InitError = null,
    init: *const fn () InitError!void = initEmpty,

    pub fn data(self: *Module, res_type: type) InitError!res_type {
        if (self.err) |err| {
            return err;
        }
        return if (self.payload) |res| @ptrCast(@alignCast(res)) else new: {
            self.init() catch |err| {
                self.err = err;
                return err;
            };
            break :new @ptrCast(@alignCast(self.payload.?));
        };
    }
};
