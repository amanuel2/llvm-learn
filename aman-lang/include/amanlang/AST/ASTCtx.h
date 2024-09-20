#pragma once
#include "llvm/Support/SourceMgr.h"

namespace amanlang {

class ASTContext {

    public:
    ASTContext (llvm::SourceMgr& SrcMgr, llvm::StringRef Filename)
    : SrcMgr (SrcMgr), Filename (Filename) {
    }

    llvm::StringRef getFilename () {
        return Filename;
    }

    const llvm::SourceMgr& getSourceMgr () const {
        return SrcMgr;
    }

    private:
    llvm::SourceMgr& SrcMgr;
    llvm::StringRef Filename;
};

} // namespace amanlang