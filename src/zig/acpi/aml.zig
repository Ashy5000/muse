const console = @import("../console.zig");
const sdt = @import("sdt.zig");

pub const AMLParseError = error{
    UnexpectedEndOfInput,
    TrailingChars,
    UnknownOpcode,
};

fn parseObject(_: *[*]u8, _: *usize) AMLParseError!bool {
    return false;
}

fn parseStatement(_: *[*]u8, _: *usize) AMLParseError!bool {
    return false;
}

fn parseExpr(_: *[*]u8, _: *usize) AMLParseError!bool {
    return false;
}

fn parseTermList(head: *[*]u8, len: *usize) AMLParseError!void {
    if (try parseObject(head, len)) return;
    if (try parseStatement(head, len)) return;
    if (try parseExpr(head, len)) return;
    return error.UnknownOpcode;
}

pub fn parseAML(data: []u8) AMLParseError!void {
    var head = data.ptr;
    var len = data.len;
    const header: *sdt.DefBlockHeader = @alignCast(@ptrCast(head));
    console.print("DefBlockHeader {}\n", .{header.*});
    head += @sizeOf(sdt.DefBlockHeader);
    len -= @sizeOf(sdt.DefBlockHeader);
    if (head - data.ptr >= data.len) {
        return error.UnexpectedEndOfInput;
    }

    try parseTermList(&head, &len);

    if (head - data.ptr < data.len) {
        return error.TrailingChars;
    }
}
