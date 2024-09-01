#pragma once

#include "llvm/Support/Compiler.h"

namespace amanlang {
namespace tok {
enum TokenKind : unsigned short {
    #define TOK(ID) ID,
    #include "TokenKinds.def"
    NUM_TOKENS
};

/// Identifiers
const char* getTokenName (TokenKind Kind) LLVM_READNONE;
const char* getPunctuatorSpelling (TokenKind Kind) LLVM_READNONE;
const char* getKeywordSpelling (TokenKind Kind) LLVM_READNONE;

} // namespace tok
} // namespace amanlang