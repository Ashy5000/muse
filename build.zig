const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.addModule("trampoline", .{
        .root_source_file = b.path("src/zig/trampoline.zig"),
        .target = target,
        .optimize = optimize,
    });
    mod.addAssemblyFile(b.path("boot.S"));

    const exe = b.addExecutable(.{
        .name = "muse_trampoline",
        .root_module = mod,
    });
    exe.setLinkerScript(b.path("linker/trampoline.ld"));

    b.installArtifact(exe);
}
