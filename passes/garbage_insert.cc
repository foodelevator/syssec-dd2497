#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include <llvm/IR/Constants.h>
#include <vector>
#include <random>

using namespace llvm;

/*Inserts useless instructions that dont change program.
  pollutes the code with fake operations
  makes ROP gadget hunting harder.
 */
 

bool garbage_insert_pass(Module &M, std::mt19937_64 &gen) {
    bool modified = false;
    
    std::uniform_int_distribution<> insertChance(0, 99);
    int insertProbability = 30;  // 30% chance to insert garbage after each instruction
    
    std::uniform_int_distribution<> randomValue(1, 100); // Random values for garbage
    std::uniform_int_distribution<> patternChoice(0, 4); // garbage pattern
    
    for (auto &F : M) {
        if (F.isDeclaration()) continue;
        
        for (auto &BB : F) {
            // Collect (instruction, baseValue) pairs
            std::vector<std::pair<Instruction*, Value*>> insertionPoints;
            
            // Track which integer values are available
            std::vector<Value*> availableValues;
            
            for (auto &I : BB) {
                // Dont insert after terminators, PHI nodes, alloca
                if (I.isTerminator()) continue; // must be last
                if (isa<PHINode>(&I)) continue; // must be first
                if (isa<AllocaInst>(&I)) continue; // stack alloc must be at start
                
                // chance to insert garbage after this instruction
                if (insertChance(gen) < insertProbability && !availableValues.empty()) {
                    // random available value to use, picks one at random
                    std::uniform_int_distribution<> valuePick(0, availableValues.size() - 1);
                    Value *baseValue = availableValues[valuePick(gen)];
                    insertionPoints.push_back({&I, baseValue}); //stores where to insert and the value
                }
                
                // If instruction produces an integer, add it to available values
                if (I.getType()->isIntegerTy()) {
                    availableValues.push_back(&I);
                }
            }
            
            // Insert garbage at collected points
            for (auto &[insertAfter, baseValue] : insertionPoints) {
                // we will insert before the next instruction
                /*
                a > b > c
                insertAfter = B
                insertBefore = C
                a > b > GARBAGE > c
                */
                auto nextIt = std::next(insertAfter->getIterator());
                if (nextIt == BB.end()) continue; //if nothing, skip
                
                Instruction *insertBefore = &*nextIt;
                IRBuilder<> builder(insertBefore);
                
                Type *intTy = baseValue->getType();
                int randVal = randomValue(gen);
                int pattern = patternChoice(gen);
                
                switch (pattern) {
                    case 0: {
                        // x + n - n, just creates instructions
                        Value *added = builder.CreateAdd(baseValue, ConstantInt::get(intTy, randVal), "");
                        builder.CreateSub(added, ConstantInt::get(intTy, randVal), "");
                        break;
                    }
                    case 1: {
                        // x * 2 / 2
                        Value *doubled = builder.CreateMul(baseValue, ConstantInt::get(intTy, 2), "");
                        builder.CreateSDiv(doubled, ConstantInt::get(intTy, 2), "");
                        break;
                    }
                    case 2: {
                        // x ^ n ^ n, XOR twice = original
                        Value *xor1 = builder.CreateXor(baseValue, ConstantInt::get(intTy, randVal), "");
                        builder.CreateXor(xor1, ConstantInt::get(intTy, randVal), "");
                        break;
                    }
                    case 3: {
                        // x | 0 
                        builder.CreateOr(baseValue, ConstantInt::get(intTy, 0), "");
                        break;
                    }
                    case 4: {
                        // x & -1, AND with all 1s = original
                        builder.CreateAnd(baseValue, ConstantInt::get(intTy, -1), "");
                        break;
                    }
                }
                
                modified = true;
            }
        }
    }
    
    return modified;
}
