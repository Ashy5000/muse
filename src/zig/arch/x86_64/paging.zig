//! Handles muse's paging system when building for x86_64. Some of the functions contained in this file are only used when paging is first initialized, and others are used throughout the lifetime of the OS.
const std = @import("std");

pub const page_size: usize = 4096;
pub const page_align = std.mem.Alignment.fromByteUnits(page_size);

/// Flags common across all x86_64 paging structures.
const pagingFlags = packed struct {
    present: bool = true,
    write: bool = true,
    user: bool = true,
    pat_2: bool = false,
    pat_1: bool = false,
    accessed: bool = false,
    dirty: bool = false,
    page_size: bool = false,
};
