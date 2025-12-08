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

// Get the pointer operand from a memory instruction
Value* getPointer(Instruction *I) {
    if (auto *load = dyn_cast<LoadInst>(I)) {
        return load->getPointerOperand();
    } else if (auto *store = dyn_cast<StoreInst>(I)) {
        return store->getPointerOperand();
    }
    return nullptr;
}

// Check if two pointers definitely point to different memory
bool definitelyDifferentMemory(Value *ptr1, Value *ptr2) {
    if (!ptr1 || !ptr2) return false;
    
    // Same pointer = same memory
    if (ptr1 == ptr2) return false;
    
    // If both are allocas, theyre different stack slots
    if (isa<AllocaInst>(ptr1) && isa<AllocaInst>(ptr2)) {
        return true;  // Different allocas = different memory
    }
    
    // If one is alloca and other is global, theyre different
    if ((isa<AllocaInst>(ptr1) && isa<GlobalVariable>(ptr2)) ||
        (isa<AllocaInst>(ptr2) && isa<GlobalVariable>(ptr1))) {
        return true;
    }
    
    // If both are different globals, theyre different
    if (isa<GlobalVariable>(ptr1) && isa<GlobalVariable>(ptr2)) {
        return true;
    }
    
    // Otherwise, we cant prove theyre different
    return false;
}

bool canReorder(Instruction *I1, Instruction *I2) {
    // Dont reorder alloca instructions - they must stay at block start
    if (isa<AllocaInst>(I1) || isa<AllocaInst>(I2)) {
        return false;
    }

    // Dont reorder terminators, returns must be last
    if (I1->isTerminator() || I2->isTerminator()) {
        return false;
    }
    
    // Dont reorder function calls (they may have side effects)
    if (isa<CallInst>(I1) || isa<CallInst>(I2)) {
        return false;
    }
    
    // Dont reorder PHI nodes
    if (isa<PHINode>(I1) || isa<PHINode>(I2)) {
        return false;
    }
    
    // Check for data dependencies
    if (dependsOn(I1, I2) || dependsOn(I2, I1)) {
        return false;
    }
    
    // Memory dependency checking
    bool I1_writes = I1->mayWriteToMemory();
    bool I2_writes = I2->mayWriteToMemory();
    bool I1_reads = I1->mayReadFromMemory();
    bool I2_reads = I2->mayReadFromMemory();
    
    // If neither accesses memory, safe to reorder
    if (!I1_writes && !I2_writes && !I1_reads && !I2_reads) {
        return true;
    }
    
    // If both only read, safe to reorder
    if (!I1_writes && !I2_writes) {
        return true;
    }
    
    // At least one writes - need to check if they access different memory
    Value *ptr1 = getPointer(I1);
    Value *ptr2 = getPointer(I2);
    
    // If we can prove they access different memory, safe to reorder
    if (definitelyDifferentMemory(ptr1, ptr2)) {
        return true;
    }
    
    // Otherwise dont reorder
    // This covers: store-load, load-store, store-store to potentially same memory
    return false;
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
