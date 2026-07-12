const std = @import("std");
const aml = @import("aml.zig");
const console = @import("../console.zig");

const NamespaceChildren = std.ArrayList(NamespaceTag);

const Namespace = struct {
    children: NamespaceChildren,
};

const NamespaceTag = struct {
    name: *const [4]u8,
    namespace: *Namespace,
};

const Path = struct {
    absolute: bool,
    parent_cnt: usize,
    segs: []const *const [4]u8,
};

const ScopingError = error{NamespaceNotFound} || std.mem.Allocator.Error;

var global_namespace: Namespace = .{
    .children = NamespaceChildren.empty,
};

pub fn init(allocator: std.mem.Allocator) std.mem.Allocator.Error!void {
    const predefined_namespaces: [5]*const [4]u8 = .{
        "_GPE",
        "_PR_",
        "_SB_",
        "_SI_",
        "_TZ_",
    };
    for (predefined_namespaces) |namespace| {
        createNamespace(&global_namespace, .{
            .absolute = true,
            .parent_cnt = 0,
            .segs = &.{namespace},
        }, allocator) catch |err| switch (err) {
            error.NamespaceNotFound => unreachable,
            else => return @errorCast(err),
        };
    }
}

fn createNamespace(start: *Namespace, path: Path, allocator: std.mem.Allocator) ScopingError!void {
    var parent = start;
    for (0..path.segs.len - 1) |i| {
        const seg = path.segs[i];
        var success: bool = false;
        for (parent.children.items) |child| {
            if (std.mem.eql(u8, child.name, seg)) {
                parent = child.namespace;
                success = true;
                break;
            }
        }
        if (!success) {
            return error.NamespaceNotFound;
        }
    }
    const namespace: *Namespace = try allocator.create(Namespace);
    namespace.* = .{
        .children = NamespaceChildren.empty,
    };
    const tag: NamespaceTag = .{
        .name = path.segs[path.segs.len - 1],
        .namespace = namespace,
    };
    try parent.children.append(allocator, tag);
}

fn parseLeadNameChar(c: u8) aml.AMLParseError!u8 {
    return if ((c >= 'A' and c <= 'Z') or c == '_') c else error.InvalidNameChars;
}

fn parseNameChar(c: u8) aml.AMLParseError!u8 {
    return parseLeadNameChar(c) catch |e| {
        if (c >= '0' and c <= '9') {
            return c;
        }
        return e;
    };
}

fn parseNameString(state: *aml.ParserState, gpa: std.mem.Allocator) aml.AMLParseError!Path {
    var path: Path = .{
        .absolute = undefined,
        .parent_cnt = 0,
        .segs = undefined,
    };
    if (state.head[0] == '\\') {
        if (state.len == 0) {
            @branchHint(.cold);
            return error.UnexpectedEndOfInput;
        }
        state.head += 1;
        state.len -= 1;
        path.absolute = true;
    } else {
        while (state.head[0] == '^') {
            if (state.len == 0) {
                @branchHint(.cold);
                return error.UnexpectedEndOfInput;
            }
            state.head += 1;
            state.len -= 1;
            path.parent_cnt += 1;
        }
        path.absolute = false;
    }
    const seg_cnt = switch (try state.next()) {
        0x00 => 0,
        0x2e => 2,
        0x2f => try state.next(),
        else => 1,
    };
    const segs = try gpa.alloc(*const [4]u8, seg_cnt);
    for (0..seg_cnt) |i| {
        const seg = try gpa.alloc(u8, 4);
        seg[0] = try parseLeadNameChar(try state.next());
        seg[1] = try parseNameChar(try state.next());
        seg[2] = try parseNameChar(try state.next());
        seg[3] = try parseNameChar(try state.next());
        segs[i] = @ptrCast(seg.ptr);
    }
    path.segs = segs;
    return path;
}

pub fn parseScope(state: *aml.ParserState, gpa: std.mem.Allocator) aml.AMLParseError!bool {
    if (state.head[0] != 0x10) {
        return false;
    }
    state.head += 1;
    state.len -= 1;
    _ = try aml.parsePkgLength(state);
    const path = try parseNameString(state, gpa);
    console.print("Path {}\n", .{path});
    unreachable;
}
