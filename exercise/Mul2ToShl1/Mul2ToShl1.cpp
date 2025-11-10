#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"  
#define DEBUG_TYPE "Mul2ToShl1"

using namespace llvm;
using namespace llvm::PatternMatch;

namespace {


    struct Mul2ToShl1Pass : PassInfoMixin<Mul2ToShl1Pass> { // PassInfoMixin a template class from llvm/IR/PassManager.h.
        PreservedAnalyses run(Function &F, FunctionAnalysisManager &) { // [1]
            SmallVector<Instruction*, 8> ToErase; // [2]
            unsigned Rewrites = 0;
            errs() << "[Mul2ToShl1] running on " << F.getName() << "\n"; // For debugging

            for (auto &BasicBlck : F) { // [3]
                for (auto &Instr : BasicBlck) { // [4]
                    if (Instr.getOpcode() != Instruction::Mul) // [5]
                        continue;

                    auto *BinaryOp = cast<BinaryOperator>(&Instr); // [6]

                    Value *X = nullptr;
                    ConstantInt *ConstInt = nullptr; // [7]

                    // Filter for multiplication with 2 ie. (X*2 or 2*X)
                    if (!(match(BinaryOp, m_Mul(m_Value(X), m_ConstantInt(ConstInt))) ||
                        match(BinaryOp, m_Mul(m_ConstantInt(ConstInt), m_Value(X)))))
                        continue;

                    if (!X->getType()->isIntegerTy())
                        continue;

                    if (!ConstInt->equalsInt(2))
                        continue;

                    // Build the shl instruction
                    IRBuilder<> B(BinaryOp); //[8]
                    auto *One = ConstantInt::get(X->getType(), 1); // [9]
                    auto *Shl = B.CreateShl(X, One, BinaryOp->getName());

                    // Replace and put old in erase list
                    BinaryOp->replaceAllUsesWith(Shl);
                    ToErase.push_back(BinaryOp);    
                    Rewrites++;
                }
            }

            for (Instruction *Replaced_Instr : ToErase)
                Replaced_Instr->eraseFromParent();

            if (Rewrites)
                LLVM_DEBUG(dbgs() << "[Mul2ToShl1] Rewrote " << Rewrites
                        << " mul-by-2 to shl-by-1 in function "
                        << F.getName() << "\n");

            return ToErase.empty() ? PreservedAnalyses::all() : PreservedAnalyses::none();
        }
    };
}

// opt -passes=Mul2Toshl1 
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "Mul2ToShl1", "0.2",
    [](PassBuilder &PB) {
      PB.registerPipelineParsingCallback(
        [](StringRef Name, FunctionPassManager &FPM,
           ArrayRef<PassBuilder::PipelineElement>) {
          if (Name == "Mul2ToShl1") {
            FPM.addPass(Mul2ToShl1Pass());
            return true;
          }
          return false;
        }
      );
    }
  };
}


/**
[1]
FunctionAnalysisManager a helper with extra info about the function.     
PreservedAnalyses is returned to show pass changes.

[2]
A vector that can hold 8 instructions on the stack (then it'll allocate on the heap.)
We collect them to erase when done.

[3]
Loop over basic blocks of/inside the function.

[4]
Iterate over each instruction inside the block from [3].

[5]
Only interested in multiplication so check for the muliplication opcode.

[6]
Cast to a BinaryOperator (a subclass of Instruction), we need methods from there. 

[7]
Will be used for pattern matching

[8]
Create an InstructionBuilder that inserts the new IR at the instruction BinaryOp (multiply since we filtered).

[9]
Make a constant 1 but type(X), since we will use shift 1 step.
**/
