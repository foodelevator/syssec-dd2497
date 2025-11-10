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

#define DEBUG_TYPE "count-additions"

bool runOnModule(Module &M) {
    auto &CTX = M.getContext();
    PointerType *printfArgType = PointerType::getUnqual(CTX);

    // Create (or _get_ in cases where it's already available) the following
    // declaration in the IR module:
    //    declare i32 @printf(i8*, ...)
    // It corresponds to the following C declaration:
    //    int printf(char *, ...)
    FunctionType *printfType = FunctionType::get(IntegerType::getInt32Ty(CTX), printfArgType, true);

    FunctionCallee printfFuncRef = M.getOrInsertFunction("printf", printfType);

    Function *printfFunc = dyn_cast<Function>(printfFuncRef.getCallee());
    printfFunc->setDoesNotThrow();
    printfFunc->addParamAttr(0, llvm::Attribute::getWithCaptureInfo(M.getContext(), llvm::CaptureInfo::none()));
    printfFunc->addParamAttr(0, Attribute::ReadOnly);

    llvm::Constant *printfFmtStr = llvm::ConstantDataArray::getString(
        CTX,
        "Execution of %s required %d additions.\n"
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

        // In every function, we create a stack variable which will be used to count how many additions are performed.
        auto counter = builder.CreateAlloca(i64, nullptr, "counter");
        builder.CreateAlignedStore(zero, counter, MaybeAlign());

        for (auto &bb : f) {
            for (auto &inst : bb) {
                auto b = IRBuilder(&inst);
                auto *binOp = dyn_cast<BinaryOperator>(&inst);
                if (binOp &&
                    binOp->getOpcode() == Instruction::Add &&
                    binOp->getType()->isIntegerTy()
                ) {
                    // If we find an integer addition, load the counter, add one to it, and store the value back.
                    auto counterVal = b.CreateAlignedLoad(i64, counter, MaybeAlign(), "counter_val");
                    b.CreateAlignedStore(b.CreateAdd(counterVal, one, "counter_new_val"), counter, MaybeAlign());
                } else if (isa<ReturnInst>(inst)) {
                    // If we find a return instruction, insert a call to printf right before it.
                    auto counterVal = b.CreateAlignedLoad(i64, counter, MaybeAlign(), "counter_val");
                    auto *fmtStr = b.CreatePointerCast(printfFmtStrVar, printfArgType);
                    b.CreateCall(printfFuncRef, {fmtStr, funcName, counterVal});
                }
            }
        }
    }

    return true;
}

struct CountAdditions : public llvm::PassInfoMixin<CountAdditions> {
    llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &) {
        bool changed =  runOnModule(M);

        return changed ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
    }

    // Without isRequired returning true, this pass will be skipped for functions
    // decorated with the optnone LLVM attribute. Note that clang -O0 decorates
    // all functions with optnone.
    static bool isRequired() { return true; }
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
    return {
        LLVM_PLUGIN_API_VERSION,
        "count-additions",
        LLVM_VERSION_STRING,
        [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager &MPM, ArrayRef<PassBuilder::PipelineElement>) {
               if (Name == "count-additions") {
                    MPM.addPass(CountAdditions());
                    return true;
                }
                return false;
            });
        },
    };
}
