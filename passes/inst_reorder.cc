#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include <vector>
#include <random>
#include <algorithm>

using namespace llvm;

/* Randomly reorders independent instructions in basic blocks.
 Instructions that dont depend on each other swapped 
 changing the code layout without affecting program.
 */

// Check if instruction I2 depends on instruction I1, does I2 use I1?
bool dependsOn(Instruction *I1, Instruction *I2) {
    for (Use &U : I2->operands()) {
        if (U.get() == I1) {
            return true;  // I2 uses I1 results, I2 must come after I1
        }
    }
    return false;
}

// Check if 2 mem ops might access the same memory location. if unsure, assume they might alias
bool mayAlias(Instruction *I1, Instruction *I2) {
    Value *ptr1 = nullptr;
    Value *ptr2 = nullptr;
    
    // Extract pointer from load/store instructions
    if (auto *load = dyn_cast<LoadInst>(I1)) {
        ptr1 = load->getPointerOperand();
    } else if (auto *store = dyn_cast<StoreInst>(I1)) {
        ptr1 = store->getPointerOperand();
    }
    
    if (auto *load = dyn_cast<LoadInst>(I2)) {
        ptr2 = load->getPointerOperand();
    } else if (auto *store = dyn_cast<StoreInst>(I2)) {
        ptr2 = store->getPointerOperand();
    }
    
    if (ptr1 && ptr2 && ptr1 != ptr2) {
        return false; // Different addresses, safe to reorder
    }
    
    // assume they might alias
    return true;
}

bool canReorder(Instruction *I1, Instruction *I2) {
    // Dont reorder terminators, returns must be last
    if (I1->isTerminator() || I2->isTerminator()) {
        return false;
    }
    
    // Dont reorder function calls
    if (isa<CallInst>(I1) || isa<CallInst>(I2)) {
        return false;
    }
    
    // Check mem ops
    bool I1_mem = I1->mayReadOrWriteMemory();
    bool I2_mem = I2->mayReadOrWriteMemory();
    
    if (I1_mem && I2_mem) {
        if (mayAlias(I1, I2)) {
            return false;  // access same memory, dont reorder
        }
    }
    
    // Dont reorder if there is a data dependency in either direction
    if (dependsOn(I1, I2) || dependsOn(I2, I1)) {
        return false;
    }
    
    return true;
}

bool inst_reorder_pass(Module &M, std::mt19937_64 &gen) {
    bool modified = false;
    
    for (auto &F : M) {
        if (F.isDeclaration()) continue;
        
        for (auto &BB : F) {
            // Collect pairs of consecutive instructions that can be reordered
            std::vector<std::pair<Instruction*, Instruction*>> reorderablePairs;
            
            Instruction *prevInst = nullptr;
            for (auto &I : BB) {
                if (prevInst && canReorder(prevInst, &I)) {
                    reorderablePairs.push_back({prevInst, &I});
                }
                prevInst = &I;
            }
            
            // Randomly decide which pairs to swap (50% chance each)
            std::uniform_int_distribution<> dis(0, 1);
            
            for (auto &pair : reorderablePairs) {
                if (dis(gen) == 0) {
                    // Swap by moving second instruction before the first
                    pair.second->moveBefore(pair.first);
                    modified = true;
                }
            }
        }
    }
    
    return modified;
}
