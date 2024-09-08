#pragma once
#include "amanlang/AST/AST.h"
#include <llvm/Target/TargetMachine.h>
#include <string>

namespace amanlang {

class CodeGen {
    public:
    static CodeGen* create (llvm::LLVMContext& Ctx, llvm::TargetMachine* TM);

    std::unique_ptr<llvm::Module> run (ModuleDecl* Decl, std::string name);

    protected:
    CodeGen (llvm::LLVMContext& Ctx, llvm::TargetMachine& Machine)
    : Ctx (Ctx), Machine (Machine) {};

    private:
    llvm::LLVMContext& Ctx;
    llvm::TargetMachine& Machine;
};

} // namespace amanlang