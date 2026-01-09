#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include <vector>
#include <random>
#include <algorithm>

using namespace llvm;

/*
 Shuffles the order of alloca instructions in functions
 randomizes the stack layout
 */

bool stack_reorder_pass(Module &M, std::mt19937_64 &gen) {
    bool modified = false;

    for (auto &F : M) {
        if (F.isDeclaration()) continue;

        // alloca instructions
        std::vector<AllocaInst*> allocas;
        BasicBlock &entryBlock = F.getEntryBlock(); // allocs at start

        for (auto &I : entryBlock) { // collect all the allocas
            if (auto *AI = dyn_cast<AllocaInst>(&I)) {
                allocas.push_back(AI);
            } else {
                // stop when we hit non-alloca
                break;
            }
        }

        if (allocas.size() < 2) continue; // at least 2 allocas to shuffle

        std::shuffle(allocas.begin(), allocas.end(), gen);

        // First alloca goes at the very beginning of the block
        Instruction *insertPoint = &*entryBlock.begin();

        for (AllocaInst *AI : allocas) {
            AI->moveBefore(insertPoint); // Move this alloca before insertPoint
            insertPoint = AI->getNextNode(); // update insertPoint to after this alloca
        }

        /*
        alloca= [%a, %b, %c]
        After shuffle: allocas = [%c, %a, %b]`
        insertPoint > %a

        Move %c before insertPoint (%a)
        insertPoint > %a (next node after %c)

        Move %a before insertPoint (%a)
        already here
        insertPoint > %b (next node after %a)

        Move %b before insertPoint (%b)
        already here
        */

        modified = true;
    }

    return modified;
}
