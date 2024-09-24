#pragma once

#include "amanlang/AST/AST.h"
#include "amanlang/AST/ASTCtx.h"
#include "amanlang/CodeGen/CGTbaa.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace amanlang {

class CGModule {
    public:
    CGModule (llvm::Module* M, ASTContext& ASTCtx)
    : M (M), ASTCtx (ASTCtx), Tbaa (*this) {
        initialize ();
    }

    // Cache types
    llvm::Type* VoidTy;
    llvm::Type* Int1Ty;
    llvm::Type* Int32Ty;
    llvm::Type* Int64Ty;
    llvm::Constant* Int32Zero;

    void initialize ();
    void run (ModuleDecl* Mod);

    // getters
    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE auto& getLLVMCtx () {
        return M->getContext ();
    }
    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE auto* getModule () {
        return M;
    }
    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE auto* getModuleDeclaration () {
        return ModDecl;
    }

    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE auto* getGlobal (Decl* D) {
        return Globals[D];
    }

    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE auto& getASTCtx () {
        return ASTCtx;
    }

    llvm::Type* convertType (TypeDecl* Ty);
    std::string mangleName (Decl* D);

    private:
    llvm::Module* M;
    ModuleDecl* ModDecl;

    // Repository of global objects.
    llvm::DenseMap<Decl*, llvm::GlobalObject*> Globals;
    llvm::DenseMap<TypeDecl*, llvm::Type*> TypeCache;

    ASTContext& ASTCtx;

    // Ch.6
    CGTbaa Tbaa;
    void decorateInst (llvm::Instruction* Inst, TypeDecl* Type);
};
} // namespace amanlang