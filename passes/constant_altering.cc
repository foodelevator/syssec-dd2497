#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/Support/Casting.h>
#include <random>

using namespace llvm;

bool constant_altering_pass(Module &m, std::mt19937_64 &gen) {
    auto &ctx = m.getContext();

    for (auto &f : m) {
        if (f.isDeclaration()) continue;

        for (auto &bb : f) {
            for (auto &inst : bb) {
                // SKIP switch instructions, too agressive
                if (isa<SwitchInst>(&inst)) continue;
                
                auto b = IRBuilder(&inst);
                for (unsigned i = 0; i < inst.getNumOperands(); i++) {
                    auto *op = dyn_cast<ConstantInt>(inst.getOperand(i));
                    if (!op || op->getValue().getSignificantBits() > 64) continue;
                    // Skip if this operand is used as a switch case value                    
                    auto val = op->getSExtValue();
                    uint64_t random = gen();
                    auto *newOp = b.Insert(BinaryOperator::CreateXor(
                        ConstantInt::get(op->getType(), val ^ random),
                        ConstantInt::get(op->getType(), random)
                    ));
                    inst.replaceUsesOfWith(op, newOp);
                }
            }
        }
    }

    return true;
}
