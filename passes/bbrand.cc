#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include <random>
#include <vector>
#include <algorithm>

using namespace llvm;

// File: passes/bbrand.cc
// Pass name: bbrand_pass
//
// Randomly shuffles the order of basic blocks in each function,
// keeping the entry block first and preserving the CFG.

bool bbrand_pass(Module &M, std::mt19937_64 &gen) {
    bool changed = false;

    for (Function &F : M) {
        // Skip declarations (no body)
        if (F.isDeclaration())
            continue;

        // If there are 0 or 1 blocks, do nothing
        if (F.size() <= 1)
            continue;

        BasicBlock &Entry = F.getEntryBlock();
        std::vector<BasicBlock *> Blocks;

        // Gather all non-entry blocks
        for (BasicBlock &BB : F) {
            if (&BB != &Entry)
                Blocks.push_back(&BB);
        }

        if (Blocks.size() <= 1)
            continue;

        // Use the RNG 
        std::shuffle(Blocks.begin(), Blocks.end(), gen);

        // Reorder blocks: Entry stays first and then shuffled blocks
        BasicBlock *Prev = &Entry;
        for (BasicBlock *BB : Blocks) {
            BB->moveAfter(Prev);
            Prev = BB;
        }

        changed = true;
    }

    return changed;
}

