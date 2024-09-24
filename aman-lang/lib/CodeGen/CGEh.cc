#include "amanlang/CodeGen/CGEh.h"

namespace amanlang {
CGEh::CGEh (llvm::Module* M) : M (M), Ctx (M->getContext ()), Builder (Ctx) {
    VoidTy       = llvm::Type::getVoidTy (Ctx);
    Int32Ty      = llvm::Type::getInt32Ty (Ctx);
    Int64Ty      = llvm::Type::getInt64Ty (Ctx);
    Int8PtrTy    = llvm::PointerType::get (Int8Ty, 0);
    Int32PtrTy   = llvm::PointerType::get (Int32Ty, 0);
    Int8PtrPtrTy = Int8PtrTy->getPointerTo ();

    Int32Zero = llvm::ConstantInt::get (Int32Ty, 0);
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGEh (Create Utils)
/////////////////////////////////////////////////////////////////////////////

void CGEh::createFunc (llvm::FunctionType*& Fty,
llvm::Function*& Fn,
const llvm::Twine& N,
llvm::Type* Result,
llvm::ArrayRef<llvm::Type*> Params,
bool IsVarArgs) {
    Fty = llvm::FunctionType::get (Result, Params, IsVarArgs);
    Fn = llvm::Function::Create (Fty, llvm::GlobalVariable::ExternalLinkage, N, M);
}

void CGEh::createICmpEq (llvm::Value* Left,
llvm::Value* Right,
llvm::BasicBlock*& TrueDest,
llvm::BasicBlock*& FalseDest,
const llvm::Twine& TrueLabel,
const llvm::Twine& FalseLabel) {
    auto* Fn = Builder.GetInsertBlock ()->getParent ();

    TrueDest  = llvm::BasicBlock::Create (M->getContext (), TrueLabel, Fn);
    FalseDest = llvm::BasicBlock::Create (M->getContext (), FalseLabel, Fn);

    llvm::Value* Cmp = Builder.CreateCmp (llvm::CmpInst::ICMP_EQ, Left, Right);
    Builder.CreateCondBr (Cmp, TrueDest, FalseDest);
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGEh (Utils)

void CGEh::addThrow (int PayloadVal) {
    // First time adding a throw for int (we only throw one type of exception now)
    if (!TypeInfo) {
        // Initate basic blocks

        TypeInfo = new llvm::GlobalVariable (*this->M, Int8Ty,
        /*isConstant=*/true, llvm::GlobalValue::ExternalLinkage,
        /*Initializer=*/nullptr, "_ZTIi");

        // Declare the __cxa_allocate_exception function.
        createFunc (AllocEHFty, AllocEHFn, "__cxa_allocate_exception",
        Int8PtrTy, { Int64Ty });

        // Declare the __cxa_throw function.
        createFunc (ThrowEHFty, ThrowEHFn, "__cxa_throw", VoidTy,
        { Int8PtrTy, Int8PtrTy, Int8PtrTy });


        // Declare personality function (using c++ runtime handler)
        llvm::Function* PersFn;
        llvm::FunctionType* PersFty;
        createFunc (PersFty, PersFn, "__gxx_personality_v0", Int32Ty, std::nullopt, true);

        // Attach personality function to main()
        llvm::Function* Fn = Builder.GetInsertBlock ()->getParent ();
        Fn->setPersonalityFn (PersFn);

        // Create and populate the landingpad block and
        // the resume block. (resume = exception type no match)
        llvm::BasicBlock* SaveBB = Builder.GetInsertBlock ();
        LPadBB = llvm::BasicBlock::Create (M->getContext (), "lpad", Fn);
        Builder.SetInsertPoint (LPadBB);
        addLandingPad ();

        // After landing pad forwards should not be reachable, once we go inside
        // an exception we never come back
        UnreachableBB = llvm::BasicBlock::Create (M->getContext (), "unreachable", Fn);
        Builder.SetInsertPoint (UnreachableBB);
        Builder.CreateUnreachable ();

        Builder.SetInsertPoint (SaveBB);
    }


    llvm::Constant* PayloadSz = llvm::ConstantInt::get (Int64Ty, 4, false);
    llvm::CallInst* EH = Builder.CreateCall (AllocEHFty, AllocEHFn, { PayloadSz });

    // Store the payload value.
    llvm::Value* PayloadPtr = Builder.CreateBitCast (EH, Int32PtrTy);
    Builder.CreateStore (llvm::ConstantInt::get (Int32Ty, PayloadVal, true), PayloadPtr);

    // Raise the exception with a call to __cxa_throw
    Builder.CreateInvoke (ThrowEHFty, ThrowEHFn, UnreachableBB, LPadBB,
    { EH, llvm::ConstantExpr::getBitCast (TypeInfo, Int8PtrTy),
    llvm::ConstantPointerNull::get (Int8PtrTy) });
}

void CGEh::addLandingPad () {
    if (TypeInfo) {

        // Need to get type info from Exception
        llvm::FunctionType* TypeIdFty;
        llvm::Function* TypeIdFn;
        createFunc (TypeIdFty, TypeIdFn, "llvm.eh.typeid.for", Int32Ty, { Int8PtrTy });

        // Begin and End blocks of the exception

        llvm::FunctionType* BeginCatchFty;
        llvm::Function* BeginCatchFn;
        createFunc (BeginCatchFty, BeginCatchFn, "__cxa_begin_catch", Int8PtrTy,
        { Int8PtrTy });

        llvm::FunctionType* EndCatchFty;
        llvm::Function* EndCatchFn;
        createFunc (EndCatchFty, EndCatchFn, "__cxa_end_catch", VoidTy);

        // Lets just print now for our exception handling
        llvm::FunctionType* PutsFty;
        llvm::Function* Puts;
        createFunc (PutsFty, Puts, "puts", VoidTy);

        // Create landing pad inst (remember we came from adding the lpad IR
        // %exc = landingpad { ptr, i32 } cleanup catch ptr @_ZTIi filter [1 x ptr] [ptr @_ZTIi]
        // %exc.ptr = extractvalue { ptr, i32 } %exc, 0
        // %exc.sel = extractvalue { ptr, i32 } %exc, 1
        llvm::LandingPadInst* Exc = Builder.CreateLandingPad (
        llvm::StructType::get (Int8PtrTy, Int32Ty), 1, "exc");
        Exc->addClause (llvm::ConstantExpr::getBitCast (TypeInfo, Int8PtrTy));

        // lpad always returns [Ptr to exception, Type of exception <catch (int& x)> ]

        // Lets extract the type from lpad and also extract the value of llvm.eh.typeid.for(int8ptr_type)
        llvm::Value* Sel   = Builder.CreateExtractValue (Exc, { 1 }, "exc.sel");
        llvm::CallInst* Id = Builder.CreateCall (TypeIdFty, TypeIdFn,
        { llvm::ConstantExpr::getBitCast (TypeInfo, Int8PtrTy) });


        // Compare the two above
        llvm::BasicBlock *TrueDest, *FalseDest;
        createICmpEq (Sel, Id, TrueDest, FalseDest, "match", "resume");

        // If false just do a Unwind_Resume => Go to next exception clause if there are any
        Builder.SetInsertPoint (FalseDest);
        Builder.CreateResume (Exc);

        // Else Exceute the function (user-provided handler)
        // We do this by => __cxa_begin_catch(Pointer to user provided handler)
        Builder.SetInsertPoint (TrueDest);
        llvm::Value* Ptr = Builder.CreateExtractValue (Exc, { 0 }, "exc.ptr");
        Builder.CreateCall (BeginCatchFty, BeginCatchFn, { Ptr });


        // And then also print our msg =)
        llvm::Value* MsgPtr =
        Builder.CreateGlobalStringPtr ("Divide by zero!", "msg", 0, M);
        Builder.CreateCall (PutsFty, Puts, { MsgPtr });

        // Finnaly call => __cxa_end_catch()
        Builder.CreateCall (EndCatchFty, EndCatchFn);
        Builder.CreateRet (Int32Zero);
    }
}


/////////////////////////////////////////////////////////////////////////////

} // namespace amanlang