#pragma once

// #include "tinylang/Basic/LLVM.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

namespace amanlang {
namespace diag {
enum {
#define DIAG(ID, Level, Msg) ID,
#include "amanlang/Basic/Diagnostic.def"
#undef DIAG
};
} // namespace diag
} // namespace amanlang

class DiagnosticEngine {
    public:
    explicit DiagnosticEngine (llvm::SourceMgr& SrcMgr) : SrcMgr (SrcMgr) {};
    unsigned numErrors () {
        return ErrCnt;
    }

    template <typename... Args>
    void report (llvm::SMLoc Loc, unsigned ID, Args&&... As) {
        // SmallString<8> S = formatv("{0} {1}", 1234.412, "test").sstr<8>();
        std::string Msg = llvm::formatv(getDiagnosticText(ID), std::forward<Args>(As)...).str();
        llvm::SourceMgr::DiagKind Kind = getDiagnosticKind(ID);
        SrcMgr.PrintMessage(Loc, Kind, Msg);
        ErrCnt += (Kind == llvm::SourceMgr::DiagKind::DK_Error);
    }

    private:
    static const char* getDiagnosticText (unsigned ID);
    static llvm::SourceMgr::DiagKind getDiagnosticKind (unsigned ID);

    llvm::SourceMgr& SrcMgr;
    unsigned ErrCnt;
};