#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/Support/Casting.h>
#include <random>

using namespace llvm;

Value *blackbox(IRBuilder<> &b, Constant *v) {
    // FunctionType *fty = FunctionType::get(v->getType(), {v->getType()}, false);
    // InlineAsm *ia = InlineAsm::get(fty, "", "={r},0", true);
    // return b.CreateCall(ia, {v});

    auto *tmp = b.CreateAlloca(v->getType(), nullptr);
    b.CreateStore(v, tmp);
    return b.CreateLoad(v->getType(), tmp);
}

bool constant_altering_pass(Module &m, std::mt19937_64 &gen) {
    auto &ctx = m.getContext();

    for (auto &f : m) {
        if (f.isDeclaration()) continue;

        for (auto &bb : f) {
            for (auto &inst : bb) {
                // It would be great to just check if an operand must be a constant or may be a
                // (dynamically computed) value, but that doesn't seem possible so instead we
                // hard-code the types of instructions we know must have constant inputs. Some
                // instructions may require some but not all operands to be constant, so this
                // solution is not amazing.
                if (isa<SwitchInst>(&inst)) continue;
                if (isa<AllocaInst>(&inst)) continue;
                if (isa<PHINode>(&inst)) continue;
                if (isa<GetElementPtrInst>(&inst)) continue;
                if (auto *CI = dyn_cast<CallInst>(&inst)) {
                    if (CI->getCalledFunction() &&
                        CI->getCalledFunction()->isIntrinsic()) {
                        continue;
                    }
                }



                auto b = IRBuilder(&inst);
                for (unsigned i = 0; i < inst.getNumOperands(); i++) {
                    auto *op = dyn_cast<ConstantInt>(inst.getOperand(i));
                    if (!op || op->getValue().getSignificantBits() > 64) continue;

                    auto val = op->getSExtValue();
                    uint64_t random = gen();
                    auto *newOp = b.Insert(BinaryOperator::CreateXor(
                        blackbox(b, ConstantInt::get(op->getType(), val ^ random)),
                        ConstantInt::get(op->getType(), random)
                    ));
                    inst.replaceUsesOfWith(op, newOp);
                }
            }
        }
    }

    return true;
}
