#pragma once

#include "amanlang/AST/AST.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

namespace amanlang {

class CGModule {
    public:
    CGModule (llvm::Module* M) : M (M) {
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
    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE llvm::LLVMContext& getLLVMCtx () {
        return M->getContext ();
    }
    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE llvm::Module* getModule () {
        return M;
    }
    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE ModuleDecl* getModuleDeclaration () {
        return ModDecl;
    }

    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE llvm::GlobalObject* getGlobal (Decl* D) {
        return Globals[D];
    }

    llvm::Type* convertType (TypeDecl* Ty);
    std::string mangleName (Decl* D);

    private:
    llvm::Module* M;
    ModuleDecl* ModDecl;

    // Repository of global objects.
    llvm::DenseMap<Decl*, llvm::GlobalObject*> Globals;
};
} // namespace amanlang