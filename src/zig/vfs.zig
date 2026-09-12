const std = @import("std");

pub const Vnode = struct {
    pub const TransferError = error{ CriticalSystemFailure, IOFailed };
    pub const Direction = enum { read, write };

    payload: *anyopaque,
    data: union(enum) {
        file: struct {
            transfer: *const fn (
                vnode: *Vnode,
                data: []u8,
                dir: Direction,
                pos: usize,
            ) TransferError!void,
        },
        directory: struct {
            children: std.StringHashMap(Vnode),
        },
    },
};

const modules = @import("modules.zig");

fn init() modules.InitError!Vnode {
    const heap = @import("alloc/heap.zig");
    const gpa = try heap.mod.data();
    var root: Vnode = .{
        .payload = undefined,
        .data = .{
            .directory = .{
                .children = .init(gpa),
            },
        },
    };
    root.data.directory.children.put("dev", .{
        .payload = undefined,
        .data = .{
            .directory = .{
                .children = .init(gpa),
            },
        },
    }) catch return error.CriticalSystemFailure;
    return root;
}

pub var mod: modules.Module(Vnode) = .{
    .name = "vfs",
    .init = init,
};
