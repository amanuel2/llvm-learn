#pragma once
#include "amanlang/AST/AST.h"
#include "amanlang/Basic/Diagnostic.h"
#include "amanlang/Basic/TokenKinds.h"
#include "amanlang/Lexer/Lexer.h"
#include "amanlang/Sema/Sema.h"

namespace amanlang {
class Parser {
    public:
    explicit Parser (Lexer& Lex, Sema& Sema) : Lex (Lex), Actions(Sema) {
        advance ();
    };

    // Overall SPI
    ModuleDecl* parse ();

    private:
    Lexer& Lex;
    Sema& Actions;
    Token Tok;

    // Parser Module
    bool parseCompilationUnit (ModuleDecl*& D);
    bool parseImport ();
    bool parseBlock (DeclList& Decls, StmtList& Stmts);

    // Parser Declarations
    bool parseDeclaration (DeclList& Decls);
    bool parseConstantDeclaration (DeclList& Decls);
    bool parseVariableDeclaration (DeclList& Decls);
    bool parseProcedureDeclaration (DeclList& ParentDecls);
    bool parseFormalParameters (FormalParamList& Params, Decl*& RetType);
    bool parseFormalParameterList (FormalParamList& Params);
    bool parseFormalParameter (FormalParamList& Params);
    // ch.5
    bool parseTypeDeclaration(DeclList &Decls);
    bool parseSelectors(Expr *&E);
    bool parseFieldList(FieldList &Fields);
    bool parseField(FieldList &Fields);


    // Parser Statements
    bool parseStatementSequence (StmtList& Stmts);
    bool parseStatement (StmtList& Stmts);
    bool parseIfStatement (StmtList& Stmts);
    bool parseWhileStatement (StmtList& Stmts);
    bool parseReturnStatement (StmtList& Stmts);

    // Parser Expressions
    bool parseExprList (ExprList& Exprs);
    bool parseExpression (Expr*& E);
    bool parseRelation (OperatorInfo& Op);
    bool parseSimpleExpression (Expr*& E);

    // Parser Operators
    bool parseAddOperator (OperatorInfo& Op);
    bool parseMulOperator (OperatorInfo& Op);

    // Parser Terms
    bool parseTerm (Expr*& E);
    bool parseFactor (Expr*& E);
    bool parseQualident (Decl*& D);
    bool parseIdentList (IdentList& Ids);


    DiagnosticEngine getDiag () LLVM_READNONE {
        return Lex.getDiagnostics ();
    }

    bool expect (tok::TokenKind Kind) {
        if (Tok.is (Kind))
            return true;
        const char* Expected = tok::getPunctuatorSpelling (Kind);
        if (!Expected)
            Expected = tok::getKeywordSpelling (Kind);

        llvm::StringRef Str (Tok.getLocation ().getPointer (), Tok.getLength ());
        getDiag ().report (Tok.getLocation (), diag::err_expected, Expected, Str);
        return false;
    }

    bool consume (tok::TokenKind Expected) LLVM_READNONE {
        llvm::outs() << "Consuming: " << tok::getTokenName (Expected) << "\n";
        if (Tok.is (Expected)) {
            llvm::outs () << "[AMAN] Consumed: " << tok::getTokenName (Expected) << "\n";
            advance();
            return true;
        }
        
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

    void advance () {
        Lex.next (Tok);
    }
};

} // namespace amanlang