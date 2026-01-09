#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"

#include <random>

using namespace llvm;

bool stackpad_rand_pass(Module &M, std::mt19937_64 &gen) {
    bool changed = false;
    LLVMContext &Ctx = M.getContext();
    const unsigned kMin = 1;
    const unsigned kMax = 16;
    std::uniform_int_distribution<unsigned> dist(kMin, kMax);
    Type *i8Ty  = Type::getInt8Ty(Ctx);
    Type *i32Ty = Type::getInt32Ty(Ctx);

    for (Function &F : M) {
        // no body function
        if (F.isDeclaration())
            continue;
        // no basic blocks
        if (F.empty())
            continue;
        BasicBlock &Entry = F.getEntryBlock();

        // Find insertion place after existing allocas (if there are any)
        // but before the first non-alloca instruction.
        Instruction *InsertPt = nullptr;
        // Look for the first alloca in the entry block
        for (Instruction &I : Entry) {
            if (isa<AllocaInst>(&I)) {
                InsertPt = &I;
                break;
            }
        }

        if (!InsertPt) {
        // No allocas at all: insert at the first real instruction in the block
        // (or fallback to terminator if the block is weirdly empty)
            auto FI = Entry.getFirstInsertionPt();
            if (FI != Entry.end()) {
                InsertPt = &*FI;
            } else {
                InsertPt = Entry.getTerminator();
            }
        }

        IRBuilder<> B(InsertPt);

        unsigned k = dist(gen); //get random num in [1, 16]
        unsigned padBytes = 16 * k; // 16-byte aligned (x86-64)

        // Allocate padBytes number of bytes ie. i8[padBytes]
        Value *padSizeVal = ConstantInt::get(i32Ty, padBytes);
        AllocaInst *padAlloca = B.CreateAlloca(i8Ty, padSizeVal, "rand_pad");

        // Make volatile store of 0 to the first byte so the compiler doesn't optimize away.
        Value *zero = ConstantInt::get(i8Ty, 0);
        B.CreateStore(zero, padAlloca, /*isVolatile=*/true);

        changed = true;
    }
    return changed;
}
