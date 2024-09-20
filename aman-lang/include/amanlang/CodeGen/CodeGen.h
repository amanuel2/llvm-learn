#pragma once
#include "amanlang/AST/AST.h"
#include "amanlang/AST/ASTCtx.h"
#include <llvm/Target/TargetMachine.h>
#include <string>

namespace amanlang {

class CodeGen {
    public:
    // which target architecture we’d like to generate code.
    static CodeGen* create (llvm::LLVMContext& Ctx, llvm::TargetMachine* TM, ASTContext& ASTCtx) {
        return new CodeGen (Ctx, *TM, ASTCtx);
    }

    std::unique_ptr<llvm::Module> run (ModuleDecl* Decl, std::string name);

    protected:
    CodeGen (llvm::LLVMContext& Ctx, llvm::TargetMachine& Machine, ASTContext& ASTCtx)
    : Ctx (Ctx), Machine (Machine), ASTCtx (ASTCtx) {};

    private:
    llvm::LLVMContext& Ctx;
    llvm::TargetMachine& Machine;
    ASTContext& ASTCtx;
};

} // namespace amanlang