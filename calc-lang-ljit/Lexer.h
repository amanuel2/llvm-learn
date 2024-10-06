#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"

class Lexer;

class Token {

    public:
    enum TokenKind : unsigned short {
        eoi,
        unknown,
        ident,
        number,
        comma,
        colon,
        equals,
        plus,
        minus,
        star,
        slash,
        l_paren,
        r_paren,
        KW_with,
        KW_def
    };
    friend class Lexer;

    bool is (TokenKind K) const {
        return Kind == K;
    }
    bool isOneOf (TokenKind K1, TokenKind K2) const {
        return is (K1) || is (K2);
    }

    template <typename... Ts> bool isOneOf (TokenKind K1, TokenKind K2, Ts... Ks) const {
        return is (K1) || isOneOf (K2, Ks...);
    }

    llvm::StringRef getText () const {
        return Lexeme;
    };
    TokenKind getKind () const {
        return Kind;
    }

    private:
    TokenKind Kind;
    llvm::StringRef Lexeme;
};

/// @class Lexer
class Lexer {
    const char* BufferStart;
    const char* BufferPtr;

    public:
    Lexer (const llvm::StringRef& Buffer) : BufferStart (Buffer.begin ()), BufferPtr (BufferStart) {
    }
    void next (Token& token);
    Token::TokenKind peek ();

    private:
    void formToken (Token& Result, const char* TokEnd, Token::TokenKind Kind);
};