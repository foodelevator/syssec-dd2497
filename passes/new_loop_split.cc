// find simple for-loops and splits it into two for-loops with identical body but different loop lengths

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <vector>
#include <random>

using namespace llvm;  // so i can write Function instead of llvm::Function


bool roll_75_percent(std::mt19937_64 &gen) {
    std::uniform_int_distribution<> n(0, 99);
    return n(gen) < 75;
}

bool new_loop_split_pass(Module &M, std::mt19937_64 &gen) {
    bool modified = false;

    // get context for creating IR elements
    LLVMContext &context = M.getContext();

    // Module -> functions -> basic blocks -> instructions
    for (Function &F : M) {

        if (F.isDeclaration()) continue; // Skip function declarations

        DominatorTree DT(F);
        LoopInfo LI(DT); // only has top-level loops

        std::vector<Loop*> allLoops; // collect EVERY loop, even subloops
        for (Loop *L : LI) {
            allLoops.push_back(L);
            for (int i = 0; i < allLoops.size(); i++) { // go through EVERY loop and check if it contains more loops
                std::vector<Loop*> subloops = allLoops[i]->getSubLoops();
                for (Loop* subloop : subloops) { // we won't hit repeat loops, because each loop only checks its own subloops
                    allLoops.push_back(subloop); // if we find a new loop, add it to the vector to be checked later
                }
            }
        }
        
        
        for (Loop *L : allLoops) {
            if (roll_75_percent(gen)) continue; // ignore 75% of loops

            // gathering info about the loop
            
            BasicBlock *preheader = L->getLoopPreheader(); // i=0
            BasicBlock *header = L->getHeader(); // i<10
            BasicBlock *latch = L->getLoopLatch(); // i++
            BasicBlock *exitBlock = L->getExitBlock(); // what happens after
            std::vector<BasicBlock*> bodyBlocks; // loop body (for cloning)
            for (BasicBlock *B : L->blocks()) {
                bodyBlocks.push_back(B);
            }

            // only include loops with these blocks
            if (!header || !latch || !preheader || !exitBlock) continue;

            /*
                bc all variables are consts in IRs,
                the loop counter (i) is a "phi node" which basically
                deduces the correct value based on earlier path
            */
            PHINode *counterPN = nullptr; // the loop counter
            for (PHINode &PN : header->phis()) { // get phi nodes from header
                if (PN.getType()->isIntegerTy()) { // we just want a simple integer loop counter
                    counterPN = &PN;
                    break;
                }
            }

            if (!counterPN) continue; // failsafe

            // abort if the loop carries any other value across iterations (accumulator etc)
            // because those will probably break when we change loop
            bool hasOtherCarriedValue = false;
            for (PHINode &PN : header->phis()) {
                if (&PN != counterPN) { hasOtherCarriedValue = true; break; }
            }
            if (hasOtherCarriedValue) continue;

            // figure out what the starting value is
            Value *start = nullptr; // llvm::Value is NOT a normal number
            // look at all possible sources for counterPN to change its value
            for (unsigned i = 0; i < counterPN->getNumIncomingValues(); i++) {
                BasicBlock *incomingB = counterPN->getIncomingBlock(i);
                // find the one that DOESNT come from inside the loop
                if (!L->contains(incomingB)) {
                    start = counterPN->getIncomingValue(i);
                    break;
                }
            }
            if (!start) continue;
            int64_t startVal = dyn_cast<ConstantInt>(start)->getSExtValue(); // cast Value to ConstantInt, then store actual value as normal int

            // find the comparison in header
            ICmpInst *loopComparison = nullptr;
            for (User *U : counterPN->users()) { // find all occurrences of *using* counterPN
                if (ICmpInst *comp = dyn_cast<ICmpInst>(U)) { // if the occurrence can be casted to type ICmpInst then it's what we're looking for
                    loopComparison = comp;
                    break;
                }
            }
            if (!loopComparison) continue;
            // it COULD happen that the comparison is in the latch instead
            // like in do-while loops, but we ignore those for simplicity
            if (loopComparison->getParent() != header) continue;
            // SLT = signed less than, ULT = unsigned ...
            if (loopComparison->getPredicate() != ICmpInst::ICMP_SLT &&
                loopComparison->getPredicate() != ICmpInst::ICMP_ULT ) continue;

            // find the loop bound (N in i<N)
            // ...assuming the comparison IS actually in the form of i < N
            ConstantInt *bound = nullptr;
            bound = dyn_cast<ConstantInt>(loopComparison->getOperand(1));
            if (!bound) continue;
            int64_t N = bound->getSExtValue(); // store actual value as normal int
            if (N <= 3 || N > 200) continue; // just in case

            int64_t split2 = startVal + ((N - startVal) / 2); // halfway between startVal and N, start point for the second loop


            // start constructing loop

            Type *counterType = counterPN->getType(); // shorthand used in a few places to ensure type stays identical

            // modify original loop's condition to end at split point
            loopComparison->setOperand(1, ConstantInt::get(counterType, split2));

            // clone body blocks for second loop
            ValueToValueMapTy VMap; // map, key: original blocks, val: new blocks
            std::vector<BasicBlock*> clonedBody;
            for (BasicBlock *B : bodyBlocks) {
                if (B == header || B == latch) continue; // header and latch are part of the loop but not body
                BasicBlock *cloned = CloneBasicBlock(B, VMap, ".split", &F); // append ".split" suffix to all cloned blocks
                clonedBody.push_back(cloned);
                VMap[B] = cloned; // create a 1-to-1 mapping between original and cloned blocks
            }

            // create preheader, header, and latch for second loop
            BasicBlock *preheader2 = BasicBlock::Create(context, "preheader.split", &F);
            BasicBlock *header2 = BasicBlock::Create(context, "header.split", &F);
            BasicBlock *latch2 = BasicBlock::Create(context, "latch.split", &F);

            // create the builder, start from preheader
            IRBuilder<> builder(preheader2);
            builder.CreateBr(header2); // uncond preheader2 -> header2
            
            builder.SetInsertPoint(header2); // insert the following instructions into header2

            // header: create phi starting at split point
            PHINode *counterPN2 = builder.CreatePHI(counterType, 2, "i.split");
            // preheader2 will set counter2's starting value to split2
            counterPN2->addIncoming(ConstantInt::get(counterType, split2), preheader2);

            // header: copy instructions from original header
            for (Instruction &I : *header) {
                if (isa<PHINode>(&I) || isa<BranchInst>(&I)) continue; // exclude phi and terminator (we'll make our own branch inst later)
                Instruction *clonedInst = I.clone();
                builder.Insert(clonedInst);
                VMap[&I] = clonedInst; // mapping

                // if any, set all operands (x in y=x+1) in this cloned inst to not point back to the original insts
                for (unsigned i = 0; i < clonedInst->getNumOperands(); i++) {
                    if (VMap.count(clonedInst->getOperand(i))) { // if this operand's defining instruction has been cloned
                        clonedInst->setOperand(i, VMap[clonedInst->getOperand(i)]); // set the operand's definition to the clone
                    }
                }
            }
            // header: create new comparison: i < N
            Value *cmp2 = builder.CreateICmpSLT(counterPN2, ConstantInt::get(counterType, N), "cmp.split");
            // first block inside body
            BasicBlock *firstBody2 = clonedBody.empty() ? latch2 : clonedBody[0];
            // branch inst: if cmp2 then firstBody2 else exitBlock
            builder.CreateCondBr(cmp2, firstBody2, exitBlock);

            // body
            for (BasicBlock *B : clonedBody) {
                for (Instruction &I : *B) {
                    for (unsigned i = 0; i < I.getNumOperands(); i++) {
                        if (VMap.count(I.getOperand(i))) { // same as earlier, remap operands to clones
                            I.setOperand(i, VMap[I.getOperand(i)]);
                        }
                    }
                    if (BranchInst *br = dyn_cast<BranchInst>(&I)) {            // if inst is a branch
                        for (unsigned i = 0; i < br->getNumSuccessors(); i++) { // check every block it can jump to
                            if (VMap.count(br->getSuccessor(i))) {              // remap jump targets to clones
                                br->setSuccessor(i, cast<BasicBlock>(VMap[br->getSuccessor(i)]));
                            } else if (br->getSuccessor(i) == latch) {          // block goes to latch
                                br->setSuccessor(i, latch2);
                            }
                        }
                    }
                }
            }

            // build second latch (remap latch->latch2 was already done)
            builder.SetInsertPoint(latch2);
            for (Instruction &I : *latch) { // copy all insts from original latch
                if (isa<PHINode>(&I) || isa<BranchInst>(&I)) continue; // exclude phi & terminator
                Instruction *cloned = I.clone();
                builder.Insert(cloned);
                VMap[&I] = cloned;
                for (unsigned i = 0; i < cloned->getNumOperands(); i++) { // same as earlier, remap operands
                    if (VMap.count(cloned->getOperand(i))) {
                        cloned->setOperand(i, VMap[cloned->getOperand(i)]);
                    }
                }
            }

            // logic for backedge (i++ then back to header2)
            Value *next2 = builder.CreateAdd(counterPN2, ConstantInt::get(counterType, 1)); // i++
            counterPN2->addIncoming(next2, latch2); // add new possible value
            builder.CreateBr(header2); // branch back to header2
            
            // point first loop's exit to second loop's preheader
            Instruction *headerTerm = header->getTerminator();
            for (unsigned i = 0; i < headerTerm->getNumSuccessors(); i++) {
                if (headerTerm->getSuccessor(i) == exitBlock) {
                    headerTerm->setSuccessor(i, preheader2);
                }
            }

            // we already excluded do-while style loops earlier for simplicity,
            // but i'm leaving the logic in the comment below

            /*
            Instruction *latchTerm = latch->getTerminator();
            for (unsigned i = 0; i < latchTerm->getNumSuccessors(); i++) {

                if (latchTerm->getSuccessor(i) == exitBlock) { 
                    latchTerm->setSuccessor(i, preheader2);
                }
            }*/

            // also clean up remapping from exitBlock's side in case we missed anything
            for (PHINode &PN : exitBlock->phis()) {
                for (unsigned i = 0; i < PN.getNumIncomingValues(); i++) {
                    if (PN.getIncomingBlock(i) == header) {
                        Value *V = PN.getIncomingValue(i);
                        if (VMap.count(V)) PN.setIncomingValue(i, VMap[V]);
                        PN.setIncomingBlock(i, header2);
                    }
                }
            }

            modified = true;
            break;
        }
    }
    return modified;
}