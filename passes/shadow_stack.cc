#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/Casting.h"
#include "llvm/IR/DerivedTypes.h"

#include <random>
#include <vector>

using namespace llvm;

static constexpr unsigned SS_SIZE = 1024;


// Function to create a global shadow stack:
// @shadow_sp = internal global i32 0
static GlobalVariable *createShadowStackSP(Module &M) {
    LLVMContext &Ctxt = M.getContext(); // Get reference/alias for the context.
    Type *i32Type = Type::getInt32Ty(Ctxt);
    auto *InitZero = ConstantInt::get(i32Type, 0);
    auto *GlobalVar = new GlobalVariable(
        M, // Push into M
        i32Type, // Holds i32 type data
        false, // isConstant = false, so mutable
        GlobalValue::InternalLinkage,
        InitZero, // Initialize with zeros
        "shadow_sp" // Name of the global
    );
    // Note the GlobalVariable stores an i32, but referencing it returns an i32* ie. a pointer to that i32
    return GlobalVar;
}


// Helper: get or create the global shadow stack array: [SHADOW_STACK_SIZE x i8*]
static GlobalVariable *createShadowStack(Module &M) {
    LLVMContext &Ctxt = M.getContext();
    Type *i8Type    = Type::getInt8Ty(Ctxt);
    Type *i8PtrType = PointerType::getUnqual(i8Type);
    ArrayType *stackType = ArrayType::get(i8PtrType, SS_SIZE);

    auto *Init = ConstantAggregateZero::get(stackType);
    auto *GV = new GlobalVariable(
        M,
        stackType,
        false, // isConstant = false, so mutable
        GlobalValue::InternalLinkage,
        Init,
        "shadow_stack"
    );
    return GV;
}

/**
Helper
Get *Function corresponding to the returnaddress intrinsic. 
corresponding to:
declare i8* @llvm.returnaddress(i32)
**/
static Function *getReturnAddress(Module &M) {
    return Intrinsic::getDeclaration(&M, Intrinsic::returnaddress);
}

// Helper: abort() 
static FunctionCallee getAbortFn(Module &M) {
    LLVMContext &Ctxt = M.getContext();
    Type *voidType = Type::getVoidTy(Ctxt);
    FunctionType *FType = FunctionType::get(voidType, /*isVarArg=*/false);
    return M.getOrInsertFunction("abort", FType);
}

bool shadow_stack_pass(Module &M, std::mt19937_64 &gen) {
    (void)gen; // Not used
    LLVMContext &Ctxt = M.getContext();
    Type *i32Type = Type::getInt32Ty(Ctxt);
    Type *i8Ty    = Type::getInt8Ty(Ctxt);
    Type *i8PtrType = PointerType::getUnqual(i8Ty);

    auto *shadowStackGV = createShadowStack(M);
    auto *shadowSPGV = createShadowStackSP(M);
    auto *retAddrFn = getReturnAddress(M);
    auto abortFn = getAbortFn(M);

    // Return pointer to the stack type ie. [1024 x i8*]
    auto *stackType = cast<ArrayType>(shadowStackGV->getValueType());
    
    bool changed = false;

    for (Function &F : M) {
        if (F.isDeclaration()) 
            continue;

        if (F.getName() == "abort")
            continue;

        // Function prologue
        {
            BasicBlock &EntryBB = F.getEntryBlock(); // Get first block of each function
            IRBuilder<> B(&*EntryBB.getFirstInsertionPt()); // Create builder at first insertion point of EntryBB
            // Create a load of the shadow_sp value from memory so we know where to push
            Value *sp = B.CreateLoad(i32Type, shadowSPGV, "shadow.sp"); 
            // Create pointer (slotPtr) to shadow_stack[sp]
            Value *zero = B.getInt32(0);
            Value *indices[] = {zero, sp};
            Value *slotPtr = B.CreateInBoundsGEP(stackType, shadowStackGV, indices, "shadow.slot");
            
            // Store function return address as: retaddr = llvm.returnaddress(0)  
            Value *zeroForRA = B.getInt32(0); // Zero for current function's return address
            Value *retaddr = B.CreateCall(retAddrFn, {zeroForRA}, "shadow.retaddr");

            // shadow_stack[sp] = retaddr
            B.CreateStore(retaddr, slotPtr);

            // shadow_sp = sp + 1
            Value *spNext = B.CreateAdd(sp, B.getInt32(1), "shadow.sp.next");
            B.CreateStore(spNext, shadowSPGV);
        }

        // 2) Collect all return instructions in the current function F.
        std::vector<ReturnInst *> returns;
        for (BasicBlock &BB : F) {
            Instruction *term = BB.getTerminator(); // Get terminator of each block in F: ret, br, switch etc...
            ReturnInst *Ret = dyn_cast<ReturnInst>(term); // Get if return
            if (Ret) {
                returns.push_back(Ret);
            }
        }
        if (returns.empty()) {continue;}
        changed = true;

        // Create epilogue / trap / final-ret basic blocks.
        BasicBlock *epilogueBB = BasicBlock::Create(Ctxt, "shadow.epilogue", &F);
        BasicBlock *trapBB     = BasicBlock::Create(Ctxt, "shadow.trap", &F);
        BasicBlock *retBB      = BasicBlock::Create(Ctxt, "shadow.ret", &F);

        IRBuilder<> EpilogueB(epilogueBB); // Set IR Builder at epilogueBB
        
        /* ########################################################## */
        // If we have non-void returns (ie returns with an arg) we need
        // a phi block to merge
        Type *retType = F.getReturnType();
        PHINode *retValPhi = nullptr;
        if (!retType->isVoidTy()) {
            retValPhi = EpilogueB.CreatePHI(retType, returns.size(), "shadow.retval");
        } // returns.size = # of incoming edges

        /* ######################################################### */


        // Pop, decrement, compute return address  
        // sp.cur = shadow_sp
        Value *spCur = EpilogueB.CreateLoad(i32Type, shadowSPGV, "shadow.sp.cur");
        // sp.dec = sp.cur - 1
        Value *spDec = EpilogueB.CreateAdd(spCur, ConstantInt::get(i32Type, -1), "shadow.sp.dec");
        EpilogueB.CreateStore(spDec, shadowSPGV);

        // slot = &shadow_stack[sp.dec] ie. we index shadow_stack[sp - 1]
        Value *zero2 = EpilogueB.getInt32(0);
        Value *indices2[] = {zero2, spDec};
        Value *slotPtr2 = EpilogueB.CreateInBoundsGEP(stackType, shadowStackGV, indices2, "shadow.slot2");

        // saved_ra = shadow_stack[sp.dec]
        Value *savedRA = EpilogueB.CreateLoad(i8PtrType, slotPtr2, "shadow.saved_ra");
        // real_ra = llvm.returnaddress(0)
        Value *realRA = EpilogueB.CreateCall(retAddrFn, {EpilogueB.getInt32(0)}, "shadow.real_ra");
        // ok = (saved_ra == real_ra)
        Value *ok = EpilogueB.CreateICmpEQ(savedRA, realRA, "shadow.ok");
        // Branch depends on comparison
        EpilogueB.CreateCondBr(ok, retBB, trapBB);

        // retBB does the actual return
        IRBuilder<> RetB(retBB); // Insert builder into shadow.ret block
        if (retType->isVoidTy()) {
            RetB.CreateRetVoid(); // ret void
        } else {
            RetB.CreateRet(retValPhi); // ret i32 %shadow.retval
        }

        // trapBB abort if mismatch 
        IRBuilder<> TrapB(trapBB);
        TrapB.CreateCall(abortFn);
        TrapB.CreateUnreachable();

        for (ReturnInst *Ret : returns) {
            IRBuilder<> B(Ret);
            if (retValPhi) { // We have a PHI ie non-void returning function
                Value *retVal = Ret->getReturnValue();    // the SSA value returned by this ret               
                BasicBlock *retParentBB = Ret->getParent();     // the block that contained the ret
                // Ensure this return has a value (some return instr has no op). Insert in PHI.
                if (retVal) { retValPhi->addIncoming(retVal, retParentBB); }
            }
            

            // Replace "ret" with a branch to the epilogue.
            B.CreateBr(epilogueBB);
            Ret->eraseFromParent();
        }
    }
    return changed;
}







