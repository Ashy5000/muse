const std = @import("std");

pub const Vnode = struct {
    pub const TransferError = error{ CriticalSystemFailure, IOFailed };
    pub const Direction = enum { read, write };

    data: union(enum) {
        file: struct {
            payload: *anyopaque,
            transfer: *const fn (
                vnode: *Vnode,
                data: []u8,
                dir: Direction,
                pos: usize,
            ) TransferError!void,
        },
        directory: struct {
            children: std.StringHashMap(Vnode),
            payload: *anyopaque,
        },
    },
};

const modules = @import("modules.zig");

fn init() modules.InitError!Vnode {
    const heap = @import("alloc/heap.zig");
    const gpa = try heap.mod.data();
    var root: Vnode = .{
        .data = .{
            .directory = .{
                .children = .init(gpa),
                .payload = undefined,
            },
        },
    };
    root.data.directory.children.put("dev", .{
        .data = .{
            .directory = .{
                .children = .init(gpa),
                .payload = undefined,
            },
        },
    }) catch return error.CriticalSystemFailure;
    return root;
}

pub var mod: modules.Module(Vnode) = .{
    .name = "vfs",
    .init = init,
};
