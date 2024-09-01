#pragma once
#include "amanlang/Basic/Diagnostic.h"
#include "amanlang/Lexer/Lexer.h"
#include "amanlang/AST/AST.h"

namespace amanlang {
class Parser {
    public:
    explicit Parser(Lexer &Lex);
    
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
};

} // namespace amanlang