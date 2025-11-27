const std = @import("std");

pub fn build(b: *std.Build) void {
    // const target = b.standardTargetOptions(.{});
    // const optimize = b.standardOptimizeOption(.{});

    // TODO: make it possible to instead specify which passes to include on the command line
    var passes = std.ArrayList([]const u8).empty;
    var passes_dir = (b.build_root.handle.openDir("passes", .{ .iterate = true }) catch @panic("Could not open passes directory")).iterate();
    while (passes_dir.next() catch unreachable) |pass| {
        passes.append(b.allocator, std.fs.path.stem(pass.name)) catch @panic("OOM");
    }

    const generate_main = b.addExecutable(.{
        .name = "generate_main",
        .root_module = b.createModule(.{
            .target = b.graph.host,
            .root_source_file = b.path("gen.zig"),
        }),
    });
    const run_generate_main = b.addRunArtifact(generate_main);
    const root = run_generate_main.addOutputFileArg("root.cc");
    for (passes.items) |pass| {
        run_generate_main.addFileArg(b.path(b.fmt("passes/{s}.cc", .{pass})));
    }

    const llvm = b.dependency("llvm", .{});

    const input_file = if (b.args != null and b.args.?.len > 0) b.args.?[0] else "you must specify an input path for the run step";

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
    for (passes.items) |pass| {
        plugin.addFileArg(b.path(b.fmt("passes/{s}.cc", .{pass})));
    }
    plugin.addArgs(&.{ "-g", "-shared", "-fPIC", "-o" });
    const plugin_so = plugin.addOutputFileArg("libdiversification.so");

    b.getInstallStep().dependOn(&b.addInstallFile(plugin_so, "libdiversification.so").step);

    const clang1 = std.Build.Step.Run.create(b, "clang1");
    clang1.addFileArg(llvm.path("bin/clang-21"));
    clang1.addArgs(&.{ "-O0", "-emit-llvm", "-c", "-o" });
    const orig_bc = clang1.addOutputFileArg("orig.bc");
    clang1.addFileArg(.{ .cwd_relative = input_file });

    const opt = std.Build.Step.Run.create(b, "opt");
    opt.addFileArg(llvm.path("bin/opt"));
    opt.addArg("-load-pass-plugin");
    opt.addFileArg(plugin_so);
    var passes_flag: []const u8 = "-passes=";
    for (0.., passes.items) |i, pass| {
        passes_flag = b.fmt("{s}{s}{s}", .{
            passes_flag,
            if (i == 0) "" else ",",
            pass,
        });
    }
    opt.addArg(passes_flag);
    opt.addArg("-o");
    const transformed_bc = opt.addOutputFileArg("transformed.bc");
    opt.addFileArg(orig_bc);

    const clang2 = std.Build.Step.Run.create(b, "clang2");
    clang2.addFileArg(llvm.path("bin/clang-21"));
    clang2.addArg("-o");
    const transformed_elf = clang2.addOutputFileArg("transformed.elf");
    clang2.addFileArg(transformed_bc);

    const run_step = b.step("run", "Run the transformation passes");
    run_step.dependOn(&b.addInstallFile(transformed_elf, b.fmt("{s}.transformed.elf", .{std.fs.path.stem(input_file)})).step);
}
