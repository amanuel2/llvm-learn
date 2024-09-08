#pragma once
#include "amanlang/Basic/Diagnostic.h"
#include "amanlang/Lexer/Token.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"

namespace amanlang {

class KeywordFilter {
    typedef struct {
        llvm::StringRef Name;
        tok::TokenKind Kind;
    } Kwargs;

    friend class Lexer;

    public:
    constexpr LLVM_ATTRIBUTE_ALWAYS_INLINE tok::TokenKind getKeyword (Kwargs kwargs,
    tok::TokenKind Default = tok::TokenKind::unknown) LLVM_READNONE {
        if (HashTable.find (kwargs.Name) == HashTable.end ())
            return HashTable.at (kwargs.Name);
        return Default;
    }

    private:
    llvm::StringMap<tok::TokenKind> HashTable;
    void addKeyword (Kwargs&& kwargs);
    void addKeywords ();
};

class Lexer {
    public:
    explicit Lexer (llvm::SourceMgr& SrcMgr, DiagnosticEngine& Diag)
    : SrcMgr (SrcMgr), Diag (Diag) {
        SrcMgrBuf = SrcMgr.getMainFileID ();
        Buf       = SrcMgr.getMemoryBuffer (SrcMgrBuf)->getBuffer ();
        Ptr       = Buf.begin ();
        KwFilter.addKeywords ();
    };

    constexpr DiagnosticEngine& getDiagnostics () LLVM_READNONE {
        return this->Diag;
    }

    /// Returns the next token from the input.
    void next (Token& Result);
    /// Gets source code buffer.
    constexpr llvm::StringRef getBuffer () LLVM_READNONE {
        return Buf;
    }


    private:
    LLVM_ATTRIBUTE_UNUSED llvm::SourceMgr& SrcMgr;
    DiagnosticEngine& Diag;

    const char* Ptr;
    llvm::StringRef Buf; // (MemoryBuf for File)
    unsigned SrcMgrBuf;  // (Curr-File Buf) managed by SrcMgr

    KeywordFilter KwFilter;

    // methods
    void identifier (Token& Result);
    void number (Token& Result);
    void string (Token& Result);
    void comment ();

    constexpr llvm::SMLoc getLoc () LLVM_READNONE {
        return llvm::SMLoc::getFromPointer (Ptr);
    }
    void formToken (Token& Result, const char* TokEnd, tok::TokenKind Kind);
};
} // namespace amanlang