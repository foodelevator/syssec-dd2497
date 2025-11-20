const std = @import("std");

pub fn build(b: *std.Build) void {
    // const target = b.standardTargetOptions(.{});
    // const optimize = b.standardOptimizeOption(.{});

    const passes: []const []const u8 = &.{ "nop" };

    const generate_main = b.addExecutable(.{
        .name = "generate_main",
        .root_module = b.createModule(.{
            .target = b.graph.host,
            .root_source_file = b.path("gen.zig"),
        }),
    });
    const run_generate_main = b.addRunArtifact(generate_main);
    const root = run_generate_main.addOutputFileArg("root.cc");
    for (passes) |pass| {
        run_generate_main.addFileArg(b.path(b.fmt("passes/{s}.cc", .{pass})));
    }

    const llvm = b.dependency("llvm", .{});

    // NOTE: for some reason, `zig c++` or `zig build-lib` produces a .so file
    // where the callback sent to registerPipelineParsingCallback is not
    // called, so we're manually using clang here instead.
    //
    // const plugin = b.addLibrary(.{
    //     .name = "diversification",
    //     .linkage = .dynamic,
    //     .root_module = b.createModule(.{
    //         .target = target,
    //         .optimize = optimize,
    //     }),
    // });
    // plugin.root_module.addCSourceFile(.{ .file = root });
    // plugin.linkLibC();
    // plugin.linkLibCpp();
    // plugin.root_module.addIncludePath(llvm.path("include"));
    // for (passes) |pass| {
    //     plugin.root_module.addCSourceFile(.{
    //         .file = b.path(b.fmt("passes/{s}.cc", .{pass})),
    //     });
    // }
    // b.installArtifact(plugin);
    const plugin = std.Build.Step.Run.create(b, "diversification");
    plugin.addFileArg(llvm.path("bin/clang++"));
    plugin.addFileArg(root);
    for (passes) |pass| {
        plugin.addFileArg(b.path(b.fmt("passes/{s}.cc", .{pass})));
    }
    plugin.addArgs(&.{"-shared", "-fPIC", "-o"});
    const plugin_so = plugin.addOutputFileArg("libdiversification.so");

    const clang1 = std.Build.Step.Run.create(b, "clang1");
    clang1.addFileArg(llvm.path("bin/clang-21"));
    clang1.addArgs(&.{"-O0", "-emit-llvm", "-c", "-o"});
    const orig_bc = clang1.addOutputFileArg("orig.bc");
    clang1.addArg("./exercise/count-additions/test.c");

    const opt = std.Build.Step.Run.create(b, "opt");
    opt.addFileArg(llvm.path("bin/opt"));
    opt.addArg("-load-pass-plugin");
    opt.addFileArg(plugin_so);
    for (passes) |pass| {
        opt.addArg(b.fmt("-passes={s}", .{pass}));
    }
    opt.addArg("-o");
    const transformed_bc = opt.addOutputFileArg("transformed.bc");
    opt.addFileArg(orig_bc);

    const clang2 = std.Build.Step.Run.create(b, "clang2");
    clang2.addFileArg(llvm.path("bin/clang-21"));
    clang2.addArg("-o");
    const done_file = clang2.addOutputFileArg("transformed.elf");
    clang2.addFileArg(transformed_bc);

    const transform_step = b.step("run", "Run the transformation passes");
    transform_step.dependOn(&clang2.step);

    b.getInstallStep().dependOn(&b.addInstallFile(done_file, "bin").step);
}
