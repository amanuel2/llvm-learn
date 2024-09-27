#include "amanlang/CodeGen/CGDebugInfo.h"
#include "amanlang/AST/AST.h"
#include <llvm/ADT/SmallString.h>

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace amanlang {

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGDebugInfo (Initalization)
/////////////////////////////////////////////////////////////////////////////

CGDebugInfo::CGDebugInfo (CGModule& CGM)
: CGM (CGM), Builder (*CGM.getModule ()) {
    llvm::SmallString<128> Path (CGM.getASTCtx ().getFilename ());
    llvm::sys::fs::make_absolute (Path);

    llvm::DIFile* File = Builder.createFile (
    llvm::sys::path::filename (Path), llvm::sys::path::parent_path (Path));

    this->CU = Builder.createCompileUnit (llvm::dwarf::DW_LANG_Modula2, File, "amanlang",
    /* isOptimized */ false, StringRef (), /* ObjCRunTimeVersion */ 0, StringRef (),
    /* EmissionKind */ llvm::DICompileUnit::DebugEmissionKind::FullDebug);
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGDebugInfo (Type Retrieval)
/////////////////////////////////////////////////////////////////////////////

llvm::DIType* CGDebugInfo::getType (PervasiveTypeDecl* Ty) {
    if (Ty->getName () == "INTEGER") {
        return Builder.createBasicType (Ty->getName (), 64, llvm::dwarf::DW_ATE_signed);
    }
    if (Ty->getName () == "BOOLEAN") {
        return Builder.createBasicType (Ty->getName (), 1, llvm::dwarf::DW_ATE_boolean);
    }
    llvm::report_fatal_error ("Unsupported pervasive type");
}

llvm::DIType* CGDebugInfo::getType (AliasTypeDecl* Ty) {
    return Builder.createTypedef (getType (Ty->getType ()), Ty->getName (),
    CU->getFile (), getLineNumber (Ty->getLocation ()), getScope ());
}

llvm::DIType* CGDebugInfo::getType (ArrayTypeDecl* Ty) {
    // Convert
    auto* ATy = llvm::cast<llvm::ArrayType> (CGM.convertType (Ty));
    const llvm::DataLayout& DL = CGM.getModule ()->getDataLayout ();

    // Get number of elems
    Expr* Nums = Ty->getNums ();
    uint64_t NumElements = llvm::cast<IntegerLiteral> (Nums)->getValue ().getZExtValue ();

    // Create a descriptor for a value range
    llvm::SmallVector<llvm::Metadata*, 4> Subscripts;
    Subscripts.push_back (Builder.getOrCreateSubrange (0, NumElements));

    return Builder.createArrayType (/* Size */ DL.getTypeSizeInBits (ATy) * 8,
    /* Aligment in bits */ 1 << Log2 (DL.getABITypeAlign (ATy)),
    getType (Ty->getType ()), Builder.getOrCreateArray (Subscripts));
}

llvm::DIType* CGDebugInfo::getType (RecordTypeDecl* Ty) {
    return nullptr;
}

llvm::DIType* CGDebugInfo::getType (TypeDecl* Type) {
    if (llvm::DIType* T = TypeCache[Type])
        return T;

    if (llvm::isa<PervasiveTypeDecl> (Type))
        return TypeCache[Type] = getType (Type);
    if (auto* AliasTy = llvm::dyn_cast<AliasTypeDecl> (Type))
        return TypeCache[Type] = getType (AliasTy);
    if (auto* ArrayTy = llvm::dyn_cast<ArrayTypeDecl> (Type))
        return TypeCache[Type] = getType (ArrayTy);
    if (auto* RecordTy = llvm ::dyn_cast<RecordTypeDecl> (Type))
        return TypeCache[Type] = getType (RecordTy);


    llvm::report_fatal_error ("Unsupported type");
    return nullptr;
}

llvm::DISubroutineType* CGDebugInfo::getType (ProcedureDecl* P) {
    llvm::SmallVector<llvm::Metadata*, 4> Types; // capture llvm::DIType's
    const llvm::DataLayout& DL = CGM.getModule ()->getDataLayout ();

    // Return Value
    if (P->getRetType ())
        Types.push_back (getType (P->getRetType ()));
    else
        Types.push_back (Builder.createUnspecifiedType ("void"));

    // Params
    for (auto* FP : P->getFormalParams ()) {
        llvm::DIType* PT = getType (FP->getType ());
        if (FP->isVar ()) {
            llvm::Type* PTy = CGM.convertType (FP->getType ());
            PT = Builder.createReferenceType (llvm::dwarf::DW_TAG_reference_type, PT,
            DL.getTypeSizeInBits (PTy) * 8, 1 << Log2 (DL.getABITypeAlign (PTy)));
        }
        Types.push_back (PT);
    }

    return Builder.createSubroutineType (Builder.getOrCreateTypeArray (Types));
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGDebugInfo (Emissions)
/////////////////////////////////////////////////////////////////////////////

// Global Variables are straightforward as compared to Local Vars (explained below)
void CGDebugInfo::emit (VariableDecl* Decl, llvm::GlobalVariable* V) {
    // Create Debug Global expression and add the info to the global var
    llvm::DIGlobalVariableExpression* GV = Builder.createGlobalVariableExpression (
    getScope (), Decl->getName (), V->getName (), CU->getFile (),
    getLineNumber (Decl->getLocation ()), getType (Decl->getType ()), false);
    V->addDebugInfo (GV);
}

// Procedure has an emit() and emitEnd()
void CGDebugInfo::emit (ProcedureDecl* Decl, llvm::Function* Fn) {
    llvm::DISubroutineType* SubT = getType (Decl);
    llvm::DISubprogram* Sub = Builder.createFunction (getScope (), Decl->getName (),
    Fn->getName (), CU->getFile (), getLineNumber (Decl->getLocation ()), SubT,
    getLineNumber (Decl->getLocation ()), llvm::DINode::FlagPrototyped,
    llvm::DISubprogram::SPFlagDefinition);

    // Don't forget to open scope and set the subprogram to be later cleaned in the end()
    openScope (Sub);
    Fn->setSubprogram (Sub);
}
void CGDebugInfo::emitEnd (ProcedureDecl* Decl, llvm::Function* Fn) {
    // Close Scope + finalize the added subprogram in emit()
    if (Fn && Fn->getSubprogram ())
        Builder.finalizeSubprogram (Fn->getSubprogram ());
    closeScope ();
}

// Local Variables require llvm.dbg.declare + llvm.dbg.define intrinciscs
llvm::DILocalVariable*
CGDebugInfo::emit (FormalParameterDecl* FP, size_t Idx, llvm::Value* Val, llvm::BasicBlock* BB) {
    llvm::DILocalVariable* Var =
    Builder.createParameterVariable (getScope (), FP->getName (), Idx,
    CU->getFile (), getLineNumber (FP->getLocation ()), getType (FP->getType ()));

    // Insert a new llvm.dbg.value intrinsic call
    Builder.insertDbgValueIntrinsic (
    Val, Var, Builder.createExpression (), getDebugLoc (FP->getLocation ()), BB);
    return Var;
}

// Local variables require llvm intrinsics (dbg.declare + define)
void CGDebugInfo::emit (llvm::Value* Val,
llvm::DILocalVariable* Var,
llvm::SMLoc Loc,
llvm::BasicBlock* BB) {
    llvm::DebugLoc DLoc = getDebugLoc (Loc);

    // Insert a new llvm.dbg.value intrinsic call
    auto Instr =
    Builder
    .insertDbgValueIntrinsic (Val, Var, Builder.createExpression (), DLoc, BB)
    .get<llvm::Instruction*> (); // Per recent changes need to cast from the PointerUnion

    Instr->setDebugLoc (DLoc);
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGDebugInfo (Utils)
/////////////////////////////////////////////////////////////////////////////

llvm::DIScope* CGDebugInfo::getScope () {
    return ScopeStack.back ();
}

void CGDebugInfo::openScope (llvm::DIScope* Scope) {
    ScopeStack.push_back (Scope);
}

void CGDebugInfo::closeScope () {
    ScopeStack.pop_back ();
}

unsigned CGDebugInfo::getLineNumber (llvm::SMLoc Loc) {
    return CGM.getASTCtx ().getSourceMgr ().FindLineNumber (Loc);
}

llvm::DebugLoc CGDebugInfo::getDebugLoc (llvm::SMLoc Loc) {
    auto LineAndCol = CGM.getASTCtx ().getSourceMgr ().getLineAndColumn (Loc);
    auto* DILoc     = llvm::DILocation::get (
    CGM.getLLVMCtx (), LineAndCol.first, LineAndCol.second, getScope ());
    return llvm::DebugLoc (DILoc);
}

void CGDebugInfo::finalize () {
    Builder.finalize ();
}

} // namespace amanlang
