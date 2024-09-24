#pragma once
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>

namespace amanlang {

class CGEh {
    public:
    explicit CGEh (llvm::Module* M);

    // Cache types
    llvm::Type* VoidTy;
    llvm::Type* Int8Ty;
    llvm::Type* Int32Ty;
    llvm::Type* Int64Ty;
    llvm::PointerType* Int8PtrTy;
    llvm::PointerType* Int32PtrTy;
    llvm::PointerType* Int8PtrPtrTy;
    llvm::Constant* Int32Zero;

    private:
    llvm::Module* M;
    llvm::LLVMContext& Ctx;
    llvm::IRBuilder<> Builder;

    llvm::GlobalVariable* TypeInfo;
    llvm::FunctionType* AllocEHFty;
    llvm::Function* AllocEHFn;
    llvm::FunctionType* ThrowEHFty;
    llvm::Function* ThrowEHFn;
    llvm::BasicBlock* LPadBB;
    llvm::BasicBlock* UnreachableBB;

    // Create IR utils
    void createFunc (llvm::FunctionType*& Fty,
    llvm::Function*& Fn,
    const llvm::Twine& N,
    llvm::Type* Result,
    llvm::ArrayRef<llvm::Type*> Params = std::nullopt,
    bool IsVarArgs                     = false);

    void createICmpEq (llvm::Value* Left,
    llvm::Value* Right,
    llvm::BasicBlock*& TrueDest,
    llvm::BasicBlock*& FalseDest,
    const llvm::Twine& TrueLabel  = "",
    const llvm::Twine& FalseLabel = "");


    // Generic Eh utils
    void addThrow (int PayloadVal);
    void addLandingPad ();
};

} // namespace amanlang