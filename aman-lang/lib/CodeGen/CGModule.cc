#include "amanlang/CodeGen/CGModule.h"
#include "amanlang/AST/AST.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Constants.h"

namespace amanlang {
void CGModule::initialize () {
    VoidTy    = llvm::Type::getVoidTy (getLLVMCtx ());
    Int1Ty    = llvm::Type::getInt1Ty (getLLVMCtx ());
    Int32Ty   = llvm::Type::getInt32Ty (getLLVMCtx ());
    Int64Ty   = llvm::Type::getInt64Ty (getLLVMCtx ());
    Int32Zero = llvm::ConstantInt::get (Int32Ty, 0, true);
}

llvm::Type* CGModule::convertType (TypeDecl* Ty) {
    if (Ty->getName () == "INTEGER")
        return Int64Ty;
    if (Ty->getName () == "BOOLEAN")
        return Int1Ty;
    llvm::report_fatal_error ("Unsupported type");
}

std::string CGModule::mangleName (Decl* D) {
    std::string Mangled ("_t");
    llvm::SmallVector<llvm::StringRef, 8> Parts;

    while (D) {
        Parts.push_back (D->getName ());
        D = D->getEnclosingDecl ();
    }

    for (auto Part : Parts)
        Mangled.append (llvm::Twine (Part.size ()).concat (Part).str ());

    return Mangled;
}

void CGModule::run (ModuleDecl* Mod) {
    for (auto* Decl : Mod->getDecls ()) {
        if (auto* Var = llvm::dyn_cast<VariableDecl> (Decl)) {
            auto Global = new llvm::GlobalVariable (*M, convertType (Var->getType ()),
            false, llvm::GlobalValue::PrivateLinkage, nullptr, mangleName (Var));
            Globals[Var] = Global;
            return;
        }

        if (auto* Var = llvm::dyn_cast<ProcedureDecl> (Decl)) {
            // CGPProcedure.run()
            return;
        }
    }
}

} // namespace amanlang