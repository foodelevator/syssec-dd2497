const std = @import("std");

pub fn build(b: *std.Build) void {
    // const target = b.standardTargetOptions(.{});
    // const optimize = b.standardOptimizeOption(.{});
    const llvm_path = b.option([]const u8, "llvm", "The path to llvm to link to and use binaries from. Omit to fetch in build script.");
    const passes = b.option([][]const u8, "pass", "Specify to run only specific pass. (Default is to run all). Specify flag multiple times to run multiple.") orelse blk: {
        var passes = std.ArrayList([]const u8).empty;
        var passes_dir = (b.build_root.handle.openDir("passes", .{ .iterate = true }) catch @panic("Could not open passes directory")).iterate();
        while (passes_dir.next() catch unreachable) |pass| {
            passes.append(b.allocator, std.fs.path.stem(pass.name)) catch @panic("OOM");
        }
        break :blk passes.items;
    };
    const clang1args = b.option([][]const u8, "clang1args", "Add arguments to the second (llvm bitcode -> binary) clang invocation") orelse &.{};
    const clang2args = b.option([][]const u8, "clang2args", "Add arguments to the second (llvm bitcode -> binary) clang invocation") orelse &.{};

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
    plugin.addFileArg(llvm(b, llvm_path, "bin/clang++"));
    plugin.addFileArg(root);
    for (passes) |pass| {
        plugin.addFileArg(b.path(b.fmt("passes/{s}.cc", .{pass})));
    }
    plugin.addPrefixedDirectoryArg("-I", llvm(b, llvm_path, "include"));
    plugin.addArgs(&.{ "-g", "-shared", "-fPIC", "-o" });
    const plugin_so = plugin.addOutputFileArg("libdiversification.so");

    b.getInstallStep().dependOn(&b.addInstallFile(plugin_so, "libdiversification.so").step);

    const clang1 = std.Build.Step.Run.create(b, "clang1");
    clang1.addFileArg(llvm(b, llvm_path, "bin/clang"));
    clang1.addArgs(&.{ "-O0", "-emit-llvm", "-c", "-o" });
    const orig_bc = clang1.addOutputFileArg("orig.bc");
    clang1.addFileArg(.{ .cwd_relative = input_file });
    clang1.addArgs(clang1args);

    const opt = std.Build.Step.Run.create(b, "opt");
    // prevent running opt from being cached, since the seed will (probably) be different each time
    opt.has_side_effects = true;
    opt.addFileArg(llvm(b, llvm_path, "bin/opt"));
    opt.addArg("-load-pass-plugin");
    opt.addFileArg(plugin_so);
    var passes_flag: []const u8 = "-passes=";
    for (0.., passes) |i, pass| {
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
    clang2.addFileArg(llvm(b, llvm_path, "bin/clang"));
    clang2.addArg("-o");
    const transformed_elf = clang2.addOutputFileArg("transformed.elf");
    clang2.addFileArg(transformed_bc);
    clang2.addArgs(clang2args);

    const run_step = b.step("run", "Run the transformation passes");
    run_step.dependOn(&b.addInstallFile(orig_bc, b.fmt("{s}.orig.bc", .{std.fs.path.stem(input_file)})).step);
    run_step.dependOn(&b.addInstallFile(transformed_bc, b.fmt("{s}.transformed.bc", .{std.fs.path.stem(input_file)})).step);
    run_step.dependOn(&b.addInstallFile(transformed_elf, b.fmt("{s}.elf", .{std.fs.path.stem(input_file)})).step);
}

fn llvm(b: *std.Build, llvm_path: ?[]const u8, sub_path: []const u8) std.Build.LazyPath {
    if (llvm_path) |path| {
        return .{ .cwd_relative = b.fmt("{s}/{s}", .{ path, sub_path }) };
    } else {
        if (b.lazyDependency("llvm", .{})) |llvm_dep| {
            return llvm_dep.path(sub_path);
        } else {
            return .{ .cwd_relative = "" };
        }
    }
}
