const std = @import("std");

pub fn main() !void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();
    var args = try std.process.argsWithAllocator(allocator);
    _ = args.skip();
    const outputPath = args.next() orelse @panic("Expected an output path as argument");
    const outputFile = try std.fs.createFileAbsolute(outputPath, .{});
    var outputBuffer: [4096]u8 = undefined;
    var outputWriter = outputFile.writer(&outputBuffer);
    const output = &outputWriter.interface;
    defer _ = output.flush() catch {};

    var passes = try std.ArrayList(struct { name: []const u8, path: []const u8 }).initCapacity(allocator, 8);
    while (args.next()) |path| {
        const filename = std.fs.path.basename(path);
        std.debug.assert(filename.len > 3);
        const suffix = filename[filename.len - 3 ..];
        std.debug.assert(std.mem.eql(u8, suffix, ".cc"));
        const name = filename[0 .. filename.len - 3];
        std.debug.assert(std.mem.indexOfAny(u8, name, "\\\"\n") == null);
        try passes.append(allocator, .{ .name = name, .path = path });
    }

    try output.writeAll(
        \\#include "llvm/IR/IRBuilder.h"
        \\#include "llvm/IR/Module.h"
        \\#include "llvm/IR/PassManager.h"
        \\#include "llvm/Passes/PassBuilder.h"
        \\#include "llvm/Passes/PassPlugin.h"
        \\#include "llvm/Pass.h"
        \\#include <llvm/IR/Constants.h>
        \\#include <llvm/IR/InstrTypes.h>
        \\#include <llvm/Support/Casting.h>
        \\
        \\using namespace llvm;
        \\
    );
    for (passes.items) |pass| {
        try output.print(
            \\
            \\bool nop_pass(llvm::Module &m);
            \\struct nop_pass_struct : public llvm::PassInfoMixin<nop_pass_struct> {{
            \\    llvm::PreservedAnalyses run(llvm::Module &m, llvm::ModuleAnalysisManager &) {{
            \\        bool changed = false; // {[0]s}_pass(m);
            \\
            \\        return changed ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
            \\    }}
            \\
            \\    // Without isRequired returning true, this pass will be skipped for functions
            \\    // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
            \\    // all functions with optnone.
            \\    static bool isRequired() {{ return true; }}
            \\}};
            \\
        , .{pass.name});
    }
    try output.writeAll(
        \\extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
        \\    return {
        \\        LLVM_PLUGIN_API_VERSION,
        \\        "diversification",
        \\        LLVM_VERSION_STRING,
        \\        [](llvm::PassBuilder &PB) {
        \\            PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
        \\                bool any = false;
        \\
    );
    for (passes.items) |pass| {
        try output.print(
            \\                if (Name == "{[0]s}") {{
            \\                    MPM.addPass({[0]s}_pass_struct());
            \\                    any = true;
            \\                }}
            \\
        , .{pass.name});
    }
    try output.writeAll(
        \\                return any;
        \\            });
        \\        },
        \\    };
        \\}
        \\
    );
}
