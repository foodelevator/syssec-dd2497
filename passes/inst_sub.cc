#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/Support/Casting.h>
#include <vector>
#include <random>

using namespace llvm;

/* Examples:
 a + b  →  a - (-b) or  -(-a + (-b))
 a - b  →  a + (-b)
 a * b  →  -(-a * b) or  (a * (b-1)) + a */

bool inst_sub_pass(Module &M, std::mt19937_64 &gen) {
    bool modified = false;
    // Module → functions → basic blocks → instructions
    for (auto &F : M) {
        // Skip function declarations
        if (F.isDeclaration()) continue;
        std::vector<BinaryOperator*> toReplace; // Store instructions to modify
        for (auto &BB : F) {
            for (auto &I : BB) { // Iterate over each instruction in the block

                // cast instruction to a BinaryOperator (add, sub, mul, div, etc.)
                auto *binOp = dyn_cast<BinaryOperator>(&I);
                if (!binOp) continue;// Not a binary op, skip it
                // transform integer operations
                if (!binOp->getType()->isIntegerTy()) continue;

                // Check if its an operation we know how to substitute
                if (binOp->getOpcode() == Instruction::Add ||
                    binOp->getOpcode() == Instruction::Sub ||
                    binOp->getOpcode() == Instruction::Mul) {
                    toReplace.push_back(binOp);  // Add to our list for processing
                }
            }
        }

        // process each itereated instruction and replace it
        for (auto *binOp : toReplace) {
            // IRBuilder helps create new LLVM instructions
            // Passing binOp means new instructions are inserted RIGHT BEFORE binOp
            IRBuilder<> builder(binOp);
            Value *replacement = nullptr; // holds the new instruction sequence
            std::uniform_int_distribution<> dis(0, 1);   // Random (0 or 1)

            switch (binOp->getOpcode()) {
                case Instruction::Add: {
                    Value *op1 = binOp->getOperand(0);
                    Value *op2 = binOp->getOperand(1);

                    if (dis(gen) == 0) {
                        // negOp2 = 0 - op2
                        // res = op1 - negOp2
                        Value *negOp2 = builder.CreateSub(ConstantInt::get(op2->getType(), 0), op2);
                        replacement = builder.CreateSub(op1, negOp2);
                    } else {
                        // sum = negOp1 + negOp2
                        // res = 0 - sum
                        Value *negOp1 = builder.CreateSub(ConstantInt::get(op1->getType(), 0), op1);
                        Value *negOp2 = builder.CreateSub(ConstantInt::get(op2->getType(), 0), op2);
                        Value *sum = builder.CreateAdd(negOp1, negOp2);
                        replacement = builder.CreateSub(ConstantInt::get(sum->getType(), 0), sum);
                    }
                    break;
                }

                case Instruction::Sub: {
                    Value *op1 = binOp->getOperand(0);
                    Value *op2 = binOp->getOperand(1);

                    Value *negOp2 = builder.CreateSub(ConstantInt::get(op2->getType(), 0), op2);
                    replacement = builder.CreateAdd(op1, negOp2);
                    break;
                }

                case Instruction::Mul: {
                    Value *op1 = binOp->getOperand(0);
                    Value *op2 = binOp->getOperand(1);

                    if (dis(gen) == 0) {

                        // mul = negOp1 * op2
                        // res = 0 - mul
                        Value *negOp1 = builder.CreateSub(ConstantInt::get(op1->getType(), 0), op1);
                        Value *mul = builder.CreateMul(negOp1, op2);
                        replacement = builder.CreateSub(ConstantInt::get(mul->getType(), 0), mul);
                    } else {
                        // a*b = a*(b-1) + a
                        // cMinusOne = op2 - 1
                        // mul = op1 * cMinusOne
                        // res = mul + op1
                        Value *cMinusOne = builder.CreateSub(op2, ConstantInt::get(op2->getType(), 1));
                        Value *mul = builder.CreateMul(op1, cMinusOne);
                        replacement = builder.CreateAdd(mul, op1);
                    }
                    break;
                }
                default:
                    break;
            }


            if (replacement) {
                // Update old instruction to now use new replacement
                binOp->replaceAllUsesWith(replacement);

                // Delete original instruction from the IR
                binOp->eraseFromParent();

                modified = true;
            }
        }
    }

    return modified;
}
