#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/Support/Casting.h>

using namespace llvm;

bool nop_pass(Module &m) {
    auto &ctx = m.getContext();

    for (auto &f : m) {
        if (f.isDeclaration()) continue;

        auto builder = IRBuilder<>(&*f.getEntryBlock().getFirstInsertionPt());
        auto funcName = builder.CreateGlobalString(f.getName());

        for (auto &bb : f) {
            for (auto &inst : bb) {
                auto b = IRBuilder(&inst);
            }
        }
    }

    return true;
}
