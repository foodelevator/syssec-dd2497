#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Pass.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/Support/Casting.h>

using namespace llvm;

#define DEBUG_TYPE "count-calls"

bool runOnModule(Module &M) {
    auto &CTX = M.getContext();
    PointerType *printfArgType = PointerType::getUnqual(CTX);
    FunctionType *printfType = FunctionType::get(IntegerType::getInt32Ty(CTX), printfArgType, true);
    FunctionCallee printfFuncRef = M.getOrInsertFunction("printf", printfType);
    Function *printfFunc = dyn_cast<Function>(printfFuncRef.getCallee());
    
    printfFunc->setDoesNotThrow();
    printfFunc->addParamAttr(0, Attribute::ReadOnly);

    llvm::Constant *printfFmtStr = llvm::ConstantDataArray::getString(
        CTX,
        "Execution of %s made %d function calls.\n"
    );

    Constant *printfFmtStrVar = M.getOrInsertGlobal("printfFmtStr", printfFmtStr->getType());
    dyn_cast<GlobalVariable>(printfFmtStrVar)->setInitializer(printfFmtStr);

    auto i64 = llvm::Type::getInt64Ty(CTX);
    auto zero = ConstantInt::get(i64, 0, false);
    auto one = ConstantInt::get(i64, 1, false);

    for (auto &f : M) {
        if (f.isDeclaration()) continue;

        auto builder = IRBuilder<>(&*f.getEntryBlock().getFirstInsertionPt());
        auto funcName = builder.CreateGlobalString(f.getName());

        auto counter = builder.CreateAlloca(i64, nullptr, "call_counter");
        builder.CreateAlignedStore(zero, counter, MaybeAlign());

        for (auto &bb : f) {
            for (auto &inst : bb) {
                auto b = IRBuilder(&inst);
                
                if (auto *callInst = dyn_cast<CallInst>(&inst)) {
                    Function *calledFunc = callInst->getCalledFunction();
                    if (calledFunc && calledFunc->getName() == "printf") {
                        continue;
                    }
                    
                    auto counterVal = b.CreateAlignedLoad(i64, counter, MaybeAlign(), "call_counter_val");
                    b.CreateAlignedStore(b.CreateAdd(counterVal, one, "call_counter_new_val"), counter, MaybeAlign());
                } else if (isa<ReturnInst>(inst)) {
                    auto counterVal = b.CreateAlignedLoad(i64, counter, MaybeAlign(), "call_counter_val");
                    auto *fmtStr = b.CreatePointerCast(printfFmtStrVar, printfArgType);
                    b.CreateCall(printfFuncRef, {fmtStr, funcName, counterVal});
                }
            }
        }
    }

    return true;
}

struct CountCalls : public llvm::PassInfoMixin<CountCalls> {
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
        bool changed = runOnModule(M);

        return changed ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
    }

    static bool isRequired() { return true; }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "count-calls",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
               if (Name == "count-calls") {
                    MPM.addPass(CountCalls());
                    return true;
                }
                return false;
            });
        },
    };
}