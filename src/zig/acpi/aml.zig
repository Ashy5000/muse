const std = @import("std");
const console = @import("../console.zig");
const sdt = @import("sdt.zig");
const heap = @import("../alloc/heap.zig");
const scoping = @import("scoping.zig");

pub const AMLParseError = error{
    UnexpectedEndOfInput,
    TrailingChars,
    UnknownOpcode,
    InvalidNameChars,
} || std.mem.Allocator.Error;

pub const ParserState = struct {
    head: [*]u8,
    len: usize,

    pub fn take(self: *ParserState, n: usize) AMLParseError![]u8 {
        if (n > self.len) {
            return error.UnexpectedEndOfInput;
        }
        const res = self.head[0..n];
        self.head += n;
        self.len -= n;
        return res;
    }

    pub fn next(self: *ParserState) AMLParseError!u8 {
        return (try self.take(1))[0];
    }
};

pub fn parsePkgLength(state: *ParserState) AMLParseError!usize {
    const lead: u8 = try state.next();
    const length_len: usize = lead >> 6;
    if (length_len == 0) {
        return length_len;
    }
    var res: usize = 0;
    for (0..length_len) |i| {
        res |= @as(usize, try state.next()) << @intCast(i * 8);
    }
    res <<= 4;
    res |= lead & 0xf;
    return res;
}

fn parseAlias(_: *ParserState) AMLParseError!bool {
    return false;
}

fn parseName(_: *ParserState) AMLParseError!bool {
    return false;
}

fn parseNamespaceModifierObj(state: *ParserState, gpa: std.mem.Allocator) AMLParseError!bool {
    if (try parseAlias(state)) return true;
    if (try parseName(state)) return true;
    if (try scoping.parseScope(state, gpa)) return true;
    return false;
}

fn parseNamedObj(_: *ParserState) AMLParseError!bool {
    return false;
}

fn parseObject(state: *ParserState, gpa: std.mem.Allocator) AMLParseError!bool {
    if (try parseNamespaceModifierObj(state, gpa)) return true;
    if (try parseNamedObj(state)) return true;
    return false;
}

fn parseStatement(_: *ParserState) AMLParseError!bool {
    return false;
}

fn parseExpr(_: *ParserState) AMLParseError!bool {
    return false;
}

fn parseTermList(state: *ParserState, gpa: std.mem.Allocator) AMLParseError!void {
    while (true) {
        if (try parseObject(state, gpa)) continue;
        if (try parseStatement(state)) continue;
        if (try parseExpr(state)) continue;
        return;
    }
}

pub fn parseAML(data: []u8, gpa: std.mem.Allocator) AMLParseError!void {
    var state: ParserState = .{
        .head = data.ptr,
        .len = data.len,
    };
    const header: *sdt.DefBlockHeader = @alignCast(@ptrCast(try state.take(@sizeOf(sdt.DefBlockHeader))));
    console.print("DefBlockHeader {}\n", .{header.*});

    try parseTermList(&state, gpa);

    if (state.head - data.ptr < data.len) {
        @branchHint(.cold);
        return error.TrailingChars;
    }
}
