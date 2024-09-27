#pragma once

#include "amanlang/AST/AST.h"
#include "amanlang/CodeGen/CGModule.h"
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/GlobalVariable.h>

namespace amanlang {

class CGDebugInfo {
    public:
    explicit CGDebugInfo (CGModule& CGM);

    // Emissions
    void emit (VariableDecl* Decl, llvm::GlobalVariable* V);
    void emit (ProcedureDecl* Decl, llvm::Function* Fn);
    void emitEnd (ProcedureDecl* Decl, llvm::Function* Fn); // emitProcedureEnd

    llvm::DILocalVariable*
    emit (FormalParameterDecl* FP, size_t Idx, llvm::Value* Val, llvm::BasicBlock* BB);
    void emit (llvm::Value* Val, llvm::DILocalVariable* Var, llvm::SMLoc Loc, llvm::BasicBlock* BB);

    private:
    CGModule& CGM;

    llvm::DIBuilder Builder;
    llvm::DICompileUnit* CU;

    llvm::DenseMap<TypeDecl*, llvm::DIType*> TypeCache;
    llvm::SmallVector<llvm::DIScope*, 4> ScopeStack;


    // Utils
    llvm::DIScope* getScope ();
    void openScope (llvm::DIScope*);
    void closeScope ();
    unsigned getLineNumber (llvm::SMLoc Loc);
    llvm::DebugLoc getDebugLoc (llvm::SMLoc Loc);
    void finalize ();


    // Type Retrieval
    llvm::DIType* getType (PervasiveTypeDecl* Ty); // PervasiveType
    llvm::DIType* getType (AliasTypeDecl* Ty);     // AliasType
    llvm::DIType* getType (ArrayTypeDecl* Ty);     // ArrayType
    llvm::DIType* getType (RecordTypeDecl* Ty);    // RecordType
    llvm::DIType* getType (TypeDecl* Type);

    llvm::DISubroutineType* getType (ProcedureDecl* P);
};
} // namespace amanlang