#pragma once

#include "AST.h"
#include "Lexer.h"
#include "llvm/Support/raw_ostream.h"

class Parser {
    Lexer& Lex;
    Token Tok;
    bool Err;

    void error () {
        llvm::errs () << "Unexpected: " << Tok.getText () << "\n";
        Err = true;
    }

    bool advance () {
        Lex.next (Tok);
        return true;
    }

    bool expect (Token::TokenKind Kind) {
        if (Tok.is (Kind)) {
            return true;
        }
        error ();
        return false;
    }

    bool consume (Token::TokenKind Kind) {
        return expect (Kind) ? true : (advance () && false);
    };

    public:
    explicit Parser (Lexer& Lex) : Lex (Lex), Err (false) {
        advance ();
    };
    AST* parse ();
    bool hasErr () {
        return Err;
    };

    // For each non-terminal of the grammar, a method to parse the rule is declared
    // Parse the Calc Compiler
    AST* parseCalc ();
    Expr* parseExpr ();
    Expr* parseTerm ();
    Expr* parseFactor ();
};