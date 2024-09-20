#include "amanlang/CodeGen/CGModule.h"
#include "amanlang/AST/AST.h"
#include "amanlang/CodeGen/CGProcedure.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/CommandLine.h"

namespace amanlang {

static llvm::cl::opt<bool>
Debug ("g", llvm::cl::desc ("Generate debug information"), llvm::cl::init (false));

void CGModule::initialize () {
    VoidTy    = llvm::Type::getVoidTy (getLLVMCtx ());
    Int1Ty    = llvm::Type::getInt1Ty (getLLVMCtx ());
    Int32Ty   = llvm::Type::getInt32Ty (getLLVMCtx ());
    Int64Ty   = llvm::Type::getInt64Ty (getLLVMCtx ());
    Int32Zero = llvm::ConstantInt::get (Int32Ty, 0, true);
}

llvm::Type* CGModule::convertType (TypeDecl* Ty) {
    // Check if its in the cache
    if (auto* T = TypeCache[Ty])
        return T;

    // built-in type
    if (llvm::isa<PervasiveTypeDecl> (Ty)) {
        if (Ty->getName () == "INTEGER")
            return Int64Ty;
        if (Ty->getName () == "BOOLEAN")
            return Int1Ty;
    }

    // is an alias
    else if (auto* AliasTy = llvm::dyn_cast<AliasTypeDecl> (Ty)) {
        llvm::Type* T = convertType (AliasTy->getType ()); // recursive call to get actual type
        return TypeCache[Ty] = T;
    }

    // Array (all of same type)
    else if (auto* ArrayTy = llvm::dyn_cast<ArrayTypeDecl> (Ty)) {
        llvm::Type* Component = convertType (ArrayTy->getType ());
        Expr* Nums            = ArrayTy->getNums ();

        if (auto NumsLit = llvm::dyn_cast<IntegerLiteral> (Nums)) {
            auto NumElems        = NumsLit->getValue ().getZExtValue ();
            llvm::Type* T        = llvm::ArrayType::get (Component, NumElems);
            return TypeCache[Ty] = T;
        }
    }

    // Record (different type for each field)
    else if (auto* RecordTy = llvm ::dyn_cast<RecordTypeDecl> (Ty)) {
        llvm::SmallVector<llvm::Type*, 4> Elements;
        for (const auto& F : RecordTy->getFields ()) {
            Elements.push_back (convertType (F.getType ())); // recursively get type of field
        }
        llvm::Type* T = llvm::StructType::create (Elements, RecordTy->getName (), false);
        return TypeCache[Ty] = T;
    }

    llvm::report_fatal_error ("Unsupported type");
}

std::string CGModule::mangleName (Decl* D) {
    std::string Mangled ("_t");
    llvm::SmallString<16> Tmp;

    while (D) {
        llvm::StringRef Name = D->getName();
        Tmp.clear();
        Tmp.append(llvm::itostr(Name.size()));
        Tmp.append (Name);
        Mangled.insert(0, Tmp.c_str()); // push_front
        D = D->getEnclosingDecl ();
    }

    return "_t" + Mangled;
}

void CGModule::run (ModuleDecl* Mod) {
    this->ModDecl = Mod;

    for (auto* Decl : Mod->getDecls ()) {
        if (auto* Var = llvm::dyn_cast<VariableDecl> (Decl)) {
            auto Global = new llvm::GlobalVariable (*M, convertType (Var->getType ()),
            false, llvm::GlobalValue::PrivateLinkage, nullptr, mangleName (Var));
            Globals[Var] = Global;
            return;
        }

        if (auto* Procedure = llvm::dyn_cast<ProcedureDecl> (Decl)) {
            CGProcedure CGP (*this);
            CGP.run (Procedure);
            return;
        }
    }
}

} // namespace amanlang