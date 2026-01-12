#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <vector>
#include <random>
#include <algorithm>
#include <set>
#include <map>

using namespace llvm;

static bool canSplitFunction(Function &F) {
    if (F.isDeclaration()) return false;
    if (F.getName() == "main") return false;
    if (F.isVarArg()) return false;
    if (F.size() < 2) return false;  //need at least 2 blocks
    
    for (BasicBlock &BB : F) {
        if (isa<InvokeInst>(BB.getTerminator())) return false;
        if (isa<ResumeInst>(BB.getTerminator())) return false;
        if (BB.isLandingPad()) return false;
    }
    
    return true;
}

//find blocks that can be split
static BasicBlock* findSplitBlock(Function &F, std::mt19937_64 &gen) {
    std::vector<BasicBlock*> candidates;
    
    for (BasicBlock &BB : F) {
        if (&BB == &F.getEntryBlock()) continue;
        
        //need single predecessor
        BasicBlock *Pred = BB.getSinglePredecessor();
        if (!Pred) continue;
        
        //must end with unconditional branch
        BranchInst *BI = dyn_cast<BranchInst>(Pred->getTerminator());
        if (!BI || BI->isConditional()) continue;
        
        candidates.push_back(&BB);
    }
    
    if (candidates.empty()) return nullptr;
    return candidates[gen() % candidates.size()];
}

bool func_splitter_pass(Module &M, std::mt19937_64 &gen) {
    bool modified = false;
    LLVMContext &Ctx = M.getContext();
    
    std::vector<Function*> candidates;
    for (Function &F : M) {
        if (canSplitFunction(F)) {
            candidates.push_back(&F);
        }
    }
    
    if (candidates.empty()) return false;
    
    std::shuffle(candidates.begin(), candidates.end(), gen);
    
    for (Function *F : candidates) {
        BasicBlock *SplitBB = findSplitBlock(*F, gen);
        if (!SplitBB) continue;
        
        BasicBlock *PredBB = SplitBB->getSinglePredecessor();
        if (!PredBB) continue;
        
        //blocks from SplitBB onwards
        std::set<BasicBlock*> extractBlocks;
        bool found = false;
        for (BasicBlock &BB : *F) {
            if (&BB == SplitBB) found = true;
            if (found) extractBlocks.insert(&BB);
        }
        
        std::vector<Value*> liveIns;
        std::set<Value*> seen;
        
        for (BasicBlock *BB : extractBlocks) {
            for (Instruction &I : *BB) {
                for (Use &U : I.operands()) {
                    Value *V = U.get();
                    if (isa<Constant>(V)) continue;
                    if (isa<BasicBlock>(V)) continue;
                    
                    bool definedOutside = false;
                    if (Instruction *Def = dyn_cast<Instruction>(V)) {
                        definedOutside = !extractBlocks.count(Def->getParent());
                    } else if (isa<Argument>(V)) {
                        definedOutside = true;
                    }
                    
                    if (definedOutside && !seen.count(V)) {
                        liveIns.push_back(V);
                        seen.insert(V);
                    }
                }
            }
        }
        
        //new function type
        std::vector<Type*> paramTypes;
        for (Value *V : liveIns) {
            paramTypes.push_back(V->getType());
        }
        
        FunctionType *newFT = FunctionType::get(F->getReturnType(), paramTypes, false);
        
        //cereate new function
        Function *SplitF = Function::Create(
            newFT, GlobalValue::InternalLinkage,
            F->getName() + ".split", &M);
        
        //old values to new function params
        ValueToValueMapTy VMap;
        unsigned idx = 0;
        for (Value *V : liveIns) {
            Argument *NewArg = SplitF->getArg(idx++);
            NewArg->setName(V->getName());
            VMap[V] = NewArg;
        }
        
        //clone  blocks into new function
        std::map<BasicBlock*, BasicBlock*> BBMap;
        for (BasicBlock *OldBB : extractBlocks) {
            BasicBlock *NewBB = BasicBlock::Create(Ctx, OldBB->getName(), SplitF);
            BBMap[OldBB] = NewBB;
            VMap[OldBB] = NewBB;
        }
        
        //clone instruc
        for (BasicBlock *OldBB : extractBlocks) {
            BasicBlock *NewBB = BBMap[OldBB];
            for (Instruction &OldI : *OldBB) {
                Instruction *NewI = OldI.clone();
                NewI->insertInto(NewBB, NewBB->end());
                VMap[&OldI] = NewI;
            }
        }
        
        for (BasicBlock *OldBB : extractBlocks) {
            BasicBlock *NewBB = BBMap[OldBB];
            for (Instruction &NewI : *NewBB) {
                RemapInstruction(&NewI, VMap, RF_NoModuleLevelChanges | RF_IgnoreMissingLocals);
            }
        }
        
        BranchInst *OldBranch = cast<BranchInst>(PredBB->getTerminator());
        IRBuilder<> Builder(OldBranch);
        
        std::vector<Value*> callArgs;
        for (Value *V : liveIns) {
            callArgs.push_back(V);
        }
        
        CallInst *SplitCall = Builder.CreateCall(SplitF, callArgs);
        
        if (F->getReturnType()->isVoidTy()) {
            Builder.CreateRetVoid();
        } else {
            Builder.CreateRet(SplitCall);
        }
        
        OldBranch->eraseFromParent();
        
        //remove old blocks from original function
        std::vector<BasicBlock*> toRemove(extractBlocks.begin(), extractBlocks.end());
        for (BasicBlock *BB : toRemove) {
            BB->dropAllReferences();
        }
        for (BasicBlock *BB : toRemove) {
            BB->eraseFromParent();
        }
        
        modified = true;
        break;  //test one func
    }
    
    return modified;
}
