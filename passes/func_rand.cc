#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include <random>
#include <vector>
#include <algorithm>

using namespace llvm;

bool func_rand_pass(Module &M, std::mt19937_64 &gen) {
    bool changed = false;

    // Store all defined functions in the vector
    std::vector<Function *> Funcs;
    for (Function &F : M) {
        if (F.isDeclaration()) {continue;} // Function without body
        Funcs.push_back(&F);
    }

    if (Funcs.size() <= 1) {return false;}
    // Shuffle
    std::shuffle(Funcs.begin(), Funcs.end(), gen);
    // Create alias/reference to the module's function list 
    // By changing FL, we change the order :)
    auto &FL = M.getFunctionList();
    Function *Prev = nullptr;

    // Insert F from shuffled Funcs into FL
    for (Function &F : Funcs) {
        auto FuncIt = F->getIterator(); // Function to be inserted
        if(!Prev) { // Case for the first function in Funcs
            auto InsertPos = FL.begin(); // Insert at the beginning
            FL.splice(InsertPos, FL, FuncIt); // Move F (first shuffled func) to the front
            Prev = F;
        } else { // Case for the other functions
            auto PrevIt = Prev->getIterator(); // Iterator to the previous function in M
            // new insert position is after the previous one
            prevIt++; 
            auto InsertPos = PrevIt;
            FL.splice(InsertPos, FL, FuncIt); // Insert F at position
            prev = F;
        }
    }



        
}

