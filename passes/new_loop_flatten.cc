// find a simple doubly-nested loop (for i { for j { body } }) and flatten it into
// a single loop over k = i*M+j, by widening the outer loop's bound and turning the
// inner loop into straight-line code that runs exactly once per outer iteration.

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include <vector>
#include <random>

using namespace llvm;

bool roll_30_percent(std::mt19937_64 &gen) {
    std::uniform_int_distribution<> n(0, 99);
    return n(gen) < 30;
}

bool my_loop_flatten_pass(Module &Module, std::mt19937_64 &gen) { // M is used for inner loop's bound
    bool modified = false;

    for (Function &F : Module) {
        if (F.isDeclaration()) continue;

        DominatorTree DT(F);
        LoopInfo LI(DT); // top-level loops only

        std::vector<Loop*> outerLoops(LI.begin(), LI.end());

        for (Loop *outerLoop : outerLoops) {
            if (roll_30_percent(gen)) continue;

            // only handle exactly one inner loop, no siblings
            if (outerLoop->getSubLoops().size() != 1) continue;
            Loop *innerLoop = outerLoop->getSubLoops()[0];


            BasicBlock *outerHeader = outerLoop->getHeader();
            BasicBlock *innerHeader = innerLoop->getHeader();
            BasicBlock *outerLatch = outerLoop->getLoopLatch();
            BasicBlock *innerLatch = innerLoop->getLoopLatch();
            if (!outerHeader || !innerHeader || !outerLatch || !innerLatch) continue; // make sure both loops are well formed

            // find each loop's integer induction variable
            PHINode *outerPhi = nullptr;
            for (PHINode &PN : outerHeader->phis())
                if (PN.getType()->isIntegerTy()) { outerPhi = &PN; break; }

            PHINode *innerPhi = nullptr;
            for (PHINode &PN : innerHeader->phis())
                if (PN.getType()->isIntegerTy()) { innerPhi = &PN; break; }

            if (!outerPhi || !innerPhi) continue;

            // for simplicity lets assume both loops must start at 0
            Value *outerInit = outerPhi->getIncomingValueForBlock(outerLoop->getLoopPreheader());
            Value *innerInit = innerPhi->getIncomingValueForBlock(innerLoop->getLoopPreheader());
            ConstantInt *outerInitC = dyn_cast_or_null<ConstantInt>(outerInit);
            ConstantInt *innerInitC = dyn_cast_or_null<ConstantInt>(innerInit);
            if (!outerInitC || !outerInitC->isZero()) continue;
            if (!innerInitC || !innerInitC->isZero()) continue;

            // find the i<N and j<M comparisons
            ICmpInst *outerCmp = nullptr;
            for (User *U : outerPhi->users())
                if (ICmpInst *cmp = dyn_cast<ICmpInst>(U)) { outerCmp = cmp; break; }

            ICmpInst *innerCmp = nullptr;
            for (User *U : innerPhi->users())
                if (ICmpInst *cmp = dyn_cast<ICmpInst>(U)) { innerCmp = cmp; break; }

            if (!outerCmp || !innerCmp) continue; // if either is missing do nothing
            if (outerCmp->getPredicate() != ICmpInst::ICMP_SLT && outerCmp->getPredicate() != ICmpInst::ICMP_ULT) continue; // if they're not < or > do nothing
            if (innerCmp->getPredicate() != ICmpInst::ICMP_SLT && innerCmp->getPredicate() != ICmpInst::ICMP_ULT) continue;

            // find N and M
            ConstantInt *outerBound = dyn_cast<ConstantInt>(outerCmp->getOperand(1));
            ConstantInt *innerBound = dyn_cast<ConstantInt>(innerCmp->getOperand(1));
            if (!outerBound || !innerBound) continue;

            int64_t N = outerBound->getSExtValue();
            int64_t M = innerBound->getSExtValue();
            if (N <= 0 || M <= 0) continue; // optionally add an upper limit so N*M doesn't get too big

            // check that the inner loop's back-edge is a plain unconditional jump to its header
            // there IS a chance (do-while loops) the branch check is in the latch instead. we ignore those loops by checking this
            BranchInst *innerHeaderTerm = dyn_cast<BranchInst>(innerHeader->getTerminator());
            BranchInst *innerLatchTerm = dyn_cast<BranchInst>(innerLatch->getTerminator());
            if (!innerHeaderTerm || !innerHeaderTerm->isConditional()) continue;
            if (!innerLatchTerm || innerLatchTerm->isConditional()) continue;

            Type *outerPhiType = outerPhi->getType(); // shorthand
            BasicBlock *innerBodyEntry = innerHeaderTerm->getSuccessor(0); // block if true
            BasicBlock *afterInner = innerHeaderTerm->getSuccessor(1); // block if false

            // change outer loop's bound so it counts all N*M iterations
            outerCmp->setOperand(1, ConstantInt::get(outerPhiType, N * M));

            // let k = new outerPhi (0 -> N*M)
            // compute i = k / M and j = k % M right before the inner loop's old body
            IRBuilder<> builder(innerHeader->getFirstNonPHIOrDbg()); // start just before the first non-phi non-debug inst in header
            Value *iVal = builder.CreateSDiv(outerPhi, ConstantInt::get(outerPhiType, M), "flattened.i"); // i = k / M
            Value *jVal = builder.CreateSRem(outerPhi, ConstantInt::get(outerPhiType, M), "flattened.j"); // j = k % M

            // replace all outerPhi with i, except in header and latch
            Instruction *outerIncr = dyn_cast<Instruction>(outerPhi->getIncomingValueForBlock(outerLatch)); // get latch instruction: i + 1

            // make_early_inc_range allows mutation during iteration
            // https://medium.com/@samarth.colleges/data-structure-and-iterator-kung-fu-in-llvm-42aa9657ff47
            for (Use &U : make_early_inc_range(outerPhi->uses())) {  // for every occurrence (use) of outerPhi:
                Instruction *user = cast<Instruction>(U.getUser());  // if the inst using outerPhi
                if (user == outerCmp || user == outerIncr) continue; // is NOT the header or latch
                U.set(iVal);                                         // replace occurrence
            }

            // inner phi can be fully replaced with j
            innerPhi->replaceAllUsesWith(jVal);
            innerPhi->eraseFromParent();

            innerHeaderTerm->eraseFromParent();                 // delete inner header's logic
            BranchInst::Create(innerBodyEntry, innerHeader);    // replace with unconditional branch to body
            innerLatchTerm->setSuccessor(0, afterInner);        // latch also dont loop

            modified = true;
            break; // one flatten per function for simplicity
        }
    }

    return modified;
}
