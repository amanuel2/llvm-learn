#pragma once
#include "amanlang/Basic/Diagnostic.h"
#include "amanlang/Lexer/Lexer.h"
#include "amanlang/AST/AST.h"

namespace amanlang {
class Parser {
    public:
    explicit Parser(Lexer &Lex);
    ModuleDecl *parseModule();
    ModuleDecl *parse();


    bool parseCompilationUnit(ModuleDecl *&D);
    bool parseImport();
    bool parseBlock(DeclList &Decls, StmtList &Stmts);
    bool parseDeclaration(DeclList &Decls);
    bool parseConstantDeclaration(DeclList &Decls);
    bool parseVariableDeclaration(DeclList &Decls);
    bool parseProcedureDeclaration(DeclList &ParentDecls);
    bool parseFormalParameters(FormalParamList &Params,
                                Decl *&RetType);
    bool parseFormalParameterList(FormalParamList &Params);
    bool parseFormalParameter(FormalParamList &Params);
    bool parseStatementSequence(StmtList &Stmts);
    bool parseStatement(StmtList &Stmts);
    bool parseIfStatement(StmtList &Stmts);
    bool parseWhileStatement(StmtList &Stmts);
    bool parseReturnStatement(StmtList &Stmts);
    bool parseExpList(ExprList &Exprs);
    bool parseExpression(Expr *&E);
    bool parseRelation(OperatorInfo &Op);
    bool parseSimpleExpression(Expr *&E);
    bool parseAddOperator(OperatorInfo &Op);
    bool parseTerm(Expr *&E);
    bool parseMulOperator(OperatorInfo &Op);
    bool parseFactor(Expr *&E);
    bool parseQualident(Decl *&D);
    bool parseIdentList(IdentList &Ids);
    
    private:
    Lexer& Lex;
    // Sema &Actions;
    Token Tok;
    DiagnosticEngine getDiag () LLVM_READNONE {
        return Lex.getDiagnostics ();
    }

    bool expect (tok::TokenKind Kind) {
        if (Tok.is (Kind))
            return false;
        const char* Expected = tok::getPunctuatorSpelling (Kind);
        if (!Expected)
            Expected = tok::getKeywordSpelling (Kind);

        llvm::StringRef Str (Tok.getLocation ().getPointer (), Tok.getLength ());
        getDiag ().report (Tok.getLocation (), diag::err_expected, Expected, Str);
        return true;
    }

    bool consume (tok::TokenKind Expected) LLVM_READNONE {
        if (Tok.is (Expected)) {
            Lex.next (Tok);
            return true;
        } else
            return false;
    }

    template <typename... TokenKinds>
    bool skipUntil (TokenKinds&&... TKs) LLVM_READNONE {
        while (true) {
            if ((... || Tok.is (TKs)))
                return false;
            if (Tok.is (tok::eof))
                return true;
            Lex.next (Tok);
        }
    }

    void next () {
        Lex.next (Tok);
    }
    void advance() {
        next ();
    } 
};

} // namespace amanlang