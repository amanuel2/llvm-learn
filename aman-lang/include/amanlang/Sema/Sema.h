#pragma once

#include "amanlang/AST/AST.h"
#include "amanlang/Basic/Diagnostic.h"
#include "amanlang/Basic/TokenKinds.h"
#include "amanlang/Sema/Scope.h"

namespace amanlang {
class Sema {

    friend class EnterDecl;

    public:
    Sema (DiagnosticEngine& Diag)
    : CurScope (new Scope ()), CurDecl (nullptr), Diag (Diag) {
        initalize ();
    };

    void initalize ();

    ModuleDecl* actOnModuleDeclaration (llvm::SMLoc Loc, llvm::StringRef Name);
    void actOnModuleDeclaration (ModuleDecl* ModDecl,
    llvm::SMLoc Loc,
    llvm::StringRef Name,
    DeclList& Decls,
    StmtList& Stmts);
    void actOnImport (llvm::StringRef ModuleName, IdentList& Ids);

    // Decls
    void actOnConstantDeclaration (DeclList& Decls, llvm::SMLoc Loc, llvm::StringRef Name, Expr* E);
    void actOnVariableDeclaration (DeclList& Decls, IdentList& Ids, Decl* D);
    void actOnFormalParameterDeclaration (FormalParamList& Params, IdentList& Ids, Decl* D, bool IsVar);
    ProcedureDecl* actOnProcedureDeclaration (llvm::SMLoc Loc, llvm::StringRef Name);
    void actOnProcedureDeclaration (ProcedureDecl* ProcDecl,
    llvm::SMLoc Loc,
    llvm::StringRef Name,
    DeclList& Decls,
    StmtList& Stmts);
    void actOnProcedureHeading (ProcedureDecl* ProcDecl, FormalParamList& Params, Decl* RetType);

    void actOnAliasTypeDeclaration (DeclList& Decls, llvm::SMLoc Loc, llvm::StringRef Name, Decl* D); // ch.5
    void actOnArrayTypeDeclaration (DeclList& Decls, llvm::SMLoc Loc, llvm::StringRef Name, Expr* E, Decl* D); // ch.5
    void actOnPointerTypeDeclaration (DeclList& Decls, llvm::SMLoc Loc, llvm::StringRef Name, Decl* D); // ch.5
    void actOnFieldDeclaration (FieldList& Fields, IdentList& Ids, Decl* D); // ch.5
    void actOnRecordTypeDeclaration (DeclList& Decls, llvm::SMLoc Loc, llvm::StringRef Name, const FieldList& Fields); // ch.5

    // Stmt
    void actOnAssignment (StmtList& Stmts, llvm::SMLoc Loc, Expr* D, Expr* E);
    void actOnProcCall (StmtList& Stmts, llvm::SMLoc Loc, Decl* D, ExprList& Params);
    void actOnIfStatement (StmtList& Stmts, llvm::SMLoc Loc, Expr* Cond, StmtList& IfStmts, StmtList& ElseStmts);
    void actOnWhileStatement (StmtList& Stmts, llvm::SMLoc Loc, Expr* Cond, StmtList& WhileStmts);
    void actOnReturnStatement (StmtList& Stmts, llvm::SMLoc Loc, Expr* RetVal);

    // Expr
    Expr* actOnExpression (Expr* Left, Expr* Right, const OperatorInfo& Op);
    Expr* actOnSimpleExpression (Expr* Left, Expr* Right, const OperatorInfo& Op);
    Expr* actOnTerm (Expr* Left, Expr* Right, const OperatorInfo& Op);
    Expr* actOnPrefixExpression (Expr* E, const OperatorInfo& Op);

    // Literals and Identifiers
    Expr* actOnIntegerLiteral (llvm::SMLoc Loc, llvm::StringRef Literal);
    Expr* actOnFunctionCall (Decl* D, ExprList& Params);
    Decl* actOnQualIdentPart (Decl* Prev, llvm::SMLoc Loc, llvm::StringRef Name);

    Expr* actOnDesignator (Decl* D); // ch.5
    void actOnIndexSelector(Expr *Desig, llvm::SMLoc Loc, Expr *E); // ch.5
    void actOnFieldSelector(Expr *Desig, llvm::SMLoc Loc, llvm::StringRef Name); // ch.5
    void actOnDereferenceSelector(Expr *Desig, llvm::SMLoc Loc); // ch.5

    private:
    Scope* CurScope;
    Decl* CurDecl;
    DiagnosticEngine& Diag;


    /* Types  */
    TypeDecl* IntegerType;
    TypeDecl* BooleanType;
    BooleanLiteral* TrueLiteral;
    BooleanLiteral* FalseLiteral;
    ConstantDecl* TrueConst;
    ConstantDecl* FalseConst;

    void enterScope (Decl*);
    void leaveScope ();

    bool isOperatorForType (tok::TokenKind Op, TypeDecl* Ty);

    void checkFormalAndActualParameters (llvm::SMLoc Loc,
    const FormalParamList& Formals,
    const ExprList& Actuals);
};


class EnterDecl {
    public:
    EnterDecl (Sema& Sema, Decl* Decl) : Sema (Sema) {
        Sema.enterScope (Decl);
    };
    ~EnterDecl () {
        Sema.leaveScope ();
    };

    private:
    Sema& Sema;
};

} // namespace amanlang