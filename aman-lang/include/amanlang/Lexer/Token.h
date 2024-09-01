#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SMLoc.h"
#include "amanlang/Basic/TokenKinds.h"

namespace amanlang {
    class Lexer;

    class Token {
        friend class Lexer;

        public: 
            constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE tok::TokenKind getKind() LLVM_READNONE { return Kind; } 
            constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE void setKind(tok::TokenKind Kind) { this->Kind = Kind; }

            constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE bool is(tok::TokenKind Kind) { return this->Kind == Kind; }
            constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE bool isNot(tok::TokenKind Kind) { return this->Kind != Kind; }
            template <typename... Tokens> constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE bool isOneOf(Tokens&... Toks) { return (... || is(Toks)); }

            constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE size_t getLength() LLVM_READNONE { return Len; }
            constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE llvm::SMLoc getLocation() LLVM_READNONE { return llvm::SMLoc::getFromPointer(Ptr); }
            constexpr const char* getName() LLVM_READNONE { return tok::getTokenName(Kind); }

            constexpr llvm::StringRef getIdentifier() LLVM_READNONE { return llvm::StringRef(Ptr, Len); }
        private:
            const char *Ptr;
            size_t Len;
            tok::TokenKind Kind;
            
    };
}