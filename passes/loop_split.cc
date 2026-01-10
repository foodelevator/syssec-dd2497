#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <vector>
#include <random>

using namespace llvm;

/*
  Splits loops into multiple sequential loops that together perform the same
  iterations. For a loop from 0 to N, splits it into 2-3 sequential loops
  covering ranges [0, mid) and [mid, N). Only transforms simple constant-bounded
  loops to ensure correctness.
*/

bool loop_split_pass(Module &M, std::mt19937_64 &gen) {
    bool modified = false;

    std::uniform_int_distribution<> shouldSplit(0, 99);
    int splitProbability = 25;  // 25% chance to split a loop

    std::uniform_int_distribution<> numSplits(2, 3);  // Split into 2 or 3 loops

    for (Function &F : M) {
        if (F.isDeclaration()) continue;

        DominatorTree DT(F);
        LoopInfo LI(DT);

        std::vector<Loop*> allLoops;
        for (Loop *L : LI) {
            allLoops.push_back(L);
            std::vector<Loop*> worklist = {L};
            for (size_t i = 0; i < worklist.size(); ++i) {
                for (Loop *Sub : worklist[i]->getSubLoops()) {
                    allLoops.push_back(Sub);
                    worklist.push_back(Sub);
                }
            }
        }

        for (Loop *L : allLoops) {
            if (shouldSplit(gen) >= splitProbability) continue;
            
            BasicBlock *header = L->getHeader();
            BasicBlock *latch = L->getLoopLatch();
            BasicBlock *preheader = L->getLoopPreheader();
            BasicBlock *exitBlock = L->getExitBlock();
            
            if (!header || !latch || !preheader || !exitBlock) continue;
            
            // Find the PHI node (induction variable)
            PHINode *indVar = nullptr;
            for (PHINode &PN : header->phis()) {
                if (PN.getType()->isIntegerTy()) {
                    indVar = &PN;
                    break;
                }
            }
            
            if (!indVar) continue;
            
            // Find initial value (should be 0)
            Value *initVal = nullptr;
            for (unsigned i = 0; i < indVar->getNumIncomingValues(); i++) {
                BasicBlock *incomingBB = indVar->getIncomingBlock(i);
                if (!L->contains(incomingBB)) {
                    initVal = indVar->getIncomingValue(i);
                    break;
                }
            }
            
            ConstantInt *initConst = dyn_cast_or_null<ConstantInt>(initVal);
            if (!initConst || !initConst->isZero()) continue;
            
            // Find the compare instruction
            ICmpInst *cmpInst = nullptr;
            for (User *U : indVar->users()) {
                if (ICmpInst *cmp = dyn_cast<ICmpInst>(U)) {
                    cmpInst = cmp;
                    break;
                }
            }
            
            if (!cmpInst) continue;
            
            // Extract bound (only support i < N)
            ConstantInt *bound = nullptr;
            if (cmpInst->getPredicate() == ICmpInst::ICMP_SLT ||
                cmpInst->getPredicate() == ICmpInst::ICMP_ULT) {
                bound = dyn_cast<ConstantInt>(cmpInst->getOperand(1));
            }
            
            if (!bound) continue;
            
            int64_t N = bound->getSExtValue();
            if (N <= 3 || N > 100) continue;  // Need at least 4 iterations to split meaningfully
            
            // Determine split points
            int splits = numSplits(gen);
            std::vector<int64_t> splitPoints;
            splitPoints.push_back(0);
            
            if (splits == 2) {
                splitPoints.push_back(N / 2);
            } else {  // splits == 3
                splitPoints.push_back(N / 3);
                splitPoints.push_back(2 * N / 3);
            }
            splitPoints.push_back(N);
            
            LLVMContext &Ctx = M.getContext();
            Type *intTy = indVar->getType();
            
            // Clone the loop body blocks
            std::vector<BasicBlock*> bodyBlocks;
            for (BasicBlock *BB : L->blocks()) {
                bodyBlocks.push_back(BB);
            }
            
            BasicBlock *lastExit = preheader;
            
            // Create split loops
            for (size_t s = 0; s < splitPoints.size() - 1; s++) {
                int64_t start = splitPoints[s];
                int64_t end = splitPoints[s + 1];
                
                // Create new loop blocks
                BasicBlock *newHeader = BasicBlock::Create(Ctx, "split.header." + std::to_string(s), &F);
                BasicBlock *newLatch = BasicBlock::Create(Ctx, "split.latch." + std::to_string(s), &F);
                
                // Map for cloning
                ValueToValueMapTy VMap;
                
                // Clone body blocks
                std::vector<BasicBlock*> clonedBodyBlocks;
                for (BasicBlock *BB : bodyBlocks) {
                    if (BB == header || BB == latch) continue;
                    
                    BasicBlock *clonedBB = CloneBasicBlock(BB, VMap, ".split." + std::to_string(s), &F);
                    clonedBodyBlocks.push_back(clonedBB);
                    VMap[BB] = clonedBB;
                }
                
                // Build new header
                IRBuilder<> builder(newHeader);
                PHINode *newPhi = builder.CreatePHI(intTy, 2, "split.i." + std::to_string(s));
                newPhi->addIncoming(ConstantInt::get(intTy, start), lastExit);
                VMap[indVar] = newPhi;
                
                // Clone header instructions (except PHI and terminator)
                for (Instruction &I : *header) {
                    if (isa<PHINode>(&I)) continue;
                    if (isa<BranchInst>(&I)) continue;
                    
                    Instruction *cloned = I.clone();
                    builder.Insert(cloned);
                    VMap[&I] = cloned;
                    
                    for (unsigned i = 0; i < cloned->getNumOperands(); i++) {
                        if (VMap.count(cloned->getOperand(i))) {
                            cloned->setOperand(i, VMap[cloned->getOperand(i)]);
                        }
                    }
                }
                
                // Add compare and branch
                Value *newCmp = builder.CreateICmpSLT(newPhi, ConstantInt::get(intTy, end), "split.cmp." + std::to_string(s));
                
                BasicBlock *firstBody = clonedBodyBlocks.empty() ? newLatch : clonedBodyBlocks[0];
                BasicBlock *nextLoop = (s == splitPoints.size() - 2) ? exitBlock : nullptr;
                if (!nextLoop) {
                    nextLoop = BasicBlock::Create(Ctx, "split.next." + std::to_string(s), &F);
                }
                
                builder.CreateCondBr(newCmp, firstBody, nextLoop);
                
                // Remap cloned body blocks
                for (BasicBlock *BB : clonedBodyBlocks) {
                    for (Instruction &I : *BB) {
                        for (unsigned i = 0; i < I.getNumOperands(); i++) {
                            if (VMap.count(I.getOperand(i))) {
                                I.setOperand(i, VMap[I.getOperand(i)]);
                            }
                        }
                        
                        // Fix successors
                        if (BranchInst *br = dyn_cast<BranchInst>(&I)) {
                            for (unsigned i = 0; i < br->getNumSuccessors(); i++) {
                                BasicBlock *succ = br->getSuccessor(i);
                                if (VMap.count(succ)) {
                                    br->setSuccessor(i, cast<BasicBlock>(VMap[succ]));
                                } else if (succ == latch) {
                                    br->setSuccessor(i, newLatch);
                                }
                            }
                        }
                    }
                }
                
                // Build new latch
                builder.SetInsertPoint(newLatch);
                
                // Clone latch instructions (except PHI and terminator)
                for (Instruction &I : *latch) {
                    if (isa<PHINode>(&I)) continue;
                    if (isa<BranchInst>(&I)) continue;
                    
                    Instruction *cloned = I.clone();
                    builder.Insert(cloned);
                    VMap[&I] = cloned;
                    
                    for (unsigned i = 0; i < cloned->getNumOperands(); i++) {
                        if (VMap.count(cloned->getOperand(i))) {
                            cloned->setOperand(i, VMap[cloned->getOperand(i)]);
                        }
                    }
                }
                
                Value *newNext = builder.CreateAdd(newPhi, ConstantInt::get(intTy, 1), "split.next." + std::to_string(s));
                newPhi->addIncoming(newNext, newLatch);
                builder.CreateBr(newHeader);
                
                // Connect from previous section
                Instruction *lastExitTerm = lastExit->getTerminator();
                if (lastExit == preheader) {
                    for (unsigned i = 0; i < lastExitTerm->getNumSuccessors(); i++) {
                        if (lastExitTerm->getSuccessor(i) == header) {
                            lastExitTerm->setSuccessor(i, newHeader);
                        }
                    }
                } else {
                    IRBuilder<> exitBuilder(lastExit);
                    exitBuilder.CreateBr(newHeader);
                }
                
                lastExit = nextLoop;
            }
            
            // Update exit block PHIs
            for (PHINode &PN : exitBlock->phis()) {
                for (unsigned i = 0; i < PN.getNumIncomingValues(); i++) {
                    if (L->contains(PN.getIncomingBlock(i))) {
                        PN.setIncomingBlock(i, lastExit);
                    }
                }
            }
            
            // Delete original loop
            std::vector<BasicBlock*> toDelete;
            for (BasicBlock *BB : L->blocks()) {
                toDelete.push_back(BB);
            }
            
            for (BasicBlock *BB : toDelete) {
                BB->dropAllReferences();
            }
            for (BasicBlock *BB : toDelete) {
                BB->eraseFromParent();
            }
            
            modified = true;
            break;  // Only split one loop per function
        }
    }

    return modified;
}
