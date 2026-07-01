const std = @import("std");

const ACPIScope = struct {
    name: []u8,
    children: std.ArrayList(ACPIScope),
};
