#include "amanlang/CodeGen/CodeGen.h"
#include "amanlang/CodeGen/CGModule.h"
#include <memory>

namespace amanlang {
CodeGen* CodeGen::create (llvm::LLVMContext& Ctx, llvm::TargetMachine* TM) {
    return new CodeGen (Ctx, *TM);
}

/// A Module instance is used to store all the information related to an
/// LLVM module. Modules are the top level container of all other LLVM
/// Intermediate Representation (IR) objects. Each module directly contains a
/// list of globals variables, a list of functions, a list of libraries (or
/// other modules) this module depends on, a symbol table, and various data
/// about the target's characteristics.
std::unique_ptr<llvm::Module> CodeGen::run (ModuleDecl* Decl, std::string name) {
    auto M = std::make_unique<llvm::Module> (name, Ctx);

    M->setTargetTriple (Machine.getTargetTriple ().getTriple ());
    M->setDataLayout (Machine.createDataLayout ());

    CGModule CGM (M.get ());
    CGM.initialize ();
    CGM.run (Decl);
    return M;
}
} // namespace amanlang
