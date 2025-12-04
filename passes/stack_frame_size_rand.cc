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
    



}