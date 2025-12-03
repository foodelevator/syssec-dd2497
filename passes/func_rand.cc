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

