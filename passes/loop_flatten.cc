#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <vector>
#include <random>

using namespace llvm;

/*
  Flattens nested loops by converting two-level nested loops into a single loop.
  For outer loop i from 0 to N and inner loop j from 0 to M, creates a single
  loop k from 0 to N*M where i = k/M and j = k%M. Only transforms simple
  constant-bounded loops to ensure correctness.
*/

bool loop_flatten_pass(Module &M, std::mt19937_64 &gen) {
    bool modified = false;

    std::uniform_int_distribution<> shouldFlatten(0, 99);
    int flattenProbability = 30;  // 30% chance to flatten a nested loop

    for (Function &F : M) {
        if (F.isDeclaration()) continue;

        DominatorTree DT(F);
        LoopInfo LI(DT);

        // Collect outer loops
        std::vector<Loop*> outerLoops;
        for (Loop *L : LI) {
            outerLoops.push_back(L);
        }

        for (Loop *outerLoop : outerLoops) {
            // Check if we should transform this loop
            if (shouldFlatten(gen) >= flattenProbability) continue;
            
            // We need exactly one subloop
            if (outerLoop->getSubLoops().size() != 1) continue;
            
            Loop *innerLoop = outerLoop->getSubLoops()[0];
            
            // Get headers and check basic structure
            BasicBlock *outerHeader = outerLoop->getHeader();
            BasicBlock *innerHeader = innerLoop->getHeader();
            if (!outerHeader || !innerHeader) continue;
            
            // Find PHI nodes (induction variables)
            PHINode *outerPhi = nullptr;
            PHINode *innerPhi = nullptr;
            
            for (PHINode &PN : outerHeader->phis()) {
                if (PN.getType()->isIntegerTy()) {
                    outerPhi = &PN;
                    break;
                }
            }
            
            for (PHINode &PN : innerHeader->phis()) {
                if (PN.getType()->isIntegerTy()) {
                    innerPhi = &PN;
                    break;
                }
            }
            
            if (!outerPhi || !innerPhi) continue;
            
            // Check for simple constant bounds
            // Outer loop should start at 0
            Value *outerInit = nullptr;
            for (unsigned i = 0; i < outerPhi->getNumIncomingValues(); i++) {
                BasicBlock *incomingBB = outerPhi->getIncomingBlock(i);
                if (!outerLoop->contains(incomingBB)) {
                    outerInit = outerPhi->getIncomingValue(i);
                    break;
                }
            }
            
            ConstantInt *outerInitConst = dyn_cast_or_null<ConstantInt>(outerInit);
            if (!outerInitConst || !outerInitConst->isZero()) continue;
            
            // Inner loop should start at 0
            Value *innerInit = nullptr;
            for (unsigned i = 0; i < innerPhi->getNumIncomingValues(); i++) {
                BasicBlock *incomingBB = innerPhi->getIncomingBlock(i);
                if (!innerLoop->contains(incomingBB)) {
                    innerInit = innerPhi->getIncomingValue(i);
                    break;
                }
            }
            
            ConstantInt *innerInitConst = dyn_cast_or_null<ConstantInt>(innerInit);
            if (!innerInitConst || !innerInitConst->isZero()) continue;
            
            // Find compare instructions for bounds
            ICmpInst *outerCmp = nullptr;
            ICmpInst *innerCmp = nullptr;
            
            for (User *U : outerPhi->users()) {
                if (ICmpInst *cmp = dyn_cast<ICmpInst>(U)) {
                    outerCmp = cmp;
                    break;
                }
            }
            
            for (User *U : innerPhi->users()) {
                if (ICmpInst *cmp = dyn_cast<ICmpInst>(U)) {
                    innerCmp = cmp;
                    break;
                }
            }
            
            if (!outerCmp || !innerCmp) continue;
            
            // Extract bounds (only support i < N pattern)
            ConstantInt *outerBound = nullptr;
            ConstantInt *innerBound = nullptr;
            
            if (outerCmp->getPredicate() == ICmpInst::ICMP_SLT ||
                outerCmp->getPredicate() == ICmpInst::ICMP_ULT) {
                outerBound = dyn_cast<ConstantInt>(outerCmp->getOperand(1));
            }
            
            if (innerCmp->getPredicate() == ICmpInst::ICMP_SLT ||
                innerCmp->getPredicate() == ICmpInst::ICMP_ULT) {
                innerBound = dyn_cast<ConstantInt>(innerCmp->getOperand(1));
            }
            
            if (!outerBound || !innerBound) continue;
            
            // Bounds must be positive and reasonable (avoid huge loops)
            int64_t N = outerBound->getSExtValue();
            int64_t innerBoundVal = innerBound->getSExtValue();
            if (N <= 0 || innerBoundVal <= 0 || N > 100 || innerBoundVal > 100) continue;
            
            // Calculate total iterations
            int64_t totalIters = N * innerBoundVal;
            
            LLVMContext &Ctx = M.getContext();
            Type *intTy = outerPhi->getType();
            
            // Create new flattened loop structure
            BasicBlock *preheader = outerLoop->getLoopPreheader();
            if (!preheader) continue;
            
            BasicBlock *flatHeader = BasicBlock::Create(Ctx, "flat.header", &F);
            BasicBlock *flatBody = BasicBlock::Create(Ctx, "flat.body", &F);
            BasicBlock *flatLatch = BasicBlock::Create(Ctx, "flat.latch", &F);
            BasicBlock *flatExit = BasicBlock::Create(Ctx, "flat.exit", &F);
            
            // Build flattened loop header
            IRBuilder<> builder(flatHeader);
            PHINode *flatPhi = builder.CreatePHI(intTy, 2, "flat.k");
            flatPhi->addIncoming(ConstantInt::get(intTy, 0), preheader);
            
            Value *flatCmp = builder.CreateICmpSLT(flatPhi, ConstantInt::get(intTy, totalIters), "flat.cmp");
            builder.CreateCondBr(flatCmp, flatBody, flatExit);
            
            // Build body: compute i = k / M, j = k % M
            builder.SetInsertPoint(flatBody);
            Value *outerVal = builder.CreateSDiv(flatPhi, ConstantInt::get(intTy, innerBoundVal), "flat.i");
            Value *innerVal = builder.CreateSRem(flatPhi, ConstantInt::get(intTy, innerBoundVal), "flat.j");
            
            // Clone inner loop body instructions
            ValueToValueMapTy VMap;
            VMap[outerPhi] = outerVal;
            VMap[innerPhi] = innerVal;
            
            // Get inner loop blocks (excluding header)
            std::vector<BasicBlock*> innerBlocks;
            for (BasicBlock *BB : innerLoop->blocks()) {
                if (BB != innerHeader) {
                    innerBlocks.push_back(BB);
                }
            }
            
            // Clone instructions from inner loop header (after PHI)
            for (Instruction &I : *innerHeader) {
                if (isa<PHINode>(&I)) continue;
                if (isa<BranchInst>(&I)) continue;
                
                Instruction *cloned = I.clone();
                builder.Insert(cloned);
                VMap[&I] = cloned;
                
                // Remap operands
                for (unsigned i = 0; i < cloned->getNumOperands(); i++) {
                    if (VMap.count(cloned->getOperand(i))) {
                        cloned->setOperand(i, VMap[cloned->getOperand(i)]);
                    }
                }
            }
            
            builder.CreateBr(flatLatch);
            
            // Build latch
            builder.SetInsertPoint(flatLatch);
            Value *flatNext = builder.CreateAdd(flatPhi, ConstantInt::get(intTy, 1), "flat.next");
            flatPhi->addIncoming(flatNext, flatLatch);
            builder.CreateBr(flatHeader);
            
            // Build exit - find outer loop exit
            builder.SetInsertPoint(flatExit);
            BasicBlock *outerExit = outerLoop->getExitBlock();
            if (outerExit) {
                builder.CreateBr(outerExit);
                
                // Update PHIs in exit block
                for (PHINode &PN : outerExit->phis()) {
                    for (unsigned i = 0; i < PN.getNumIncomingValues(); i++) {
                        if (outerLoop->contains(PN.getIncomingBlock(i))) {
                            PN.setIncomingBlock(i, flatExit);
                        }
                    }
                }
            } else {
                // No clear exit, just return
                if (F.getReturnType()->isVoidTy()) {
                    builder.CreateRetVoid();
                } else {
                    builder.CreateRet(UndefValue::get(F.getReturnType()));
                }
            }
            
            // Redirect preheader to new loop
            Instruction *preheaderTerm = preheader->getTerminator();
            for (unsigned i = 0; i < preheaderTerm->getNumSuccessors(); i++) {
                if (preheaderTerm->getSuccessor(i) == outerHeader) {
                    preheaderTerm->setSuccessor(i, flatHeader);
                }
            }
            
            // Mark old loop blocks for deletion
            std::vector<BasicBlock*> toDelete;
            for (BasicBlock *BB : outerLoop->blocks()) {
                toDelete.push_back(BB);
            }
            
            // Remove old blocks
            for (BasicBlock *BB : toDelete) {
                BB->dropAllReferences();
            }
            for (BasicBlock *BB : toDelete) {
                BB->eraseFromParent();
            }
            
            modified = true;
            break;  // Only flatten one loop per function
        }
    }

    return modified;
}
