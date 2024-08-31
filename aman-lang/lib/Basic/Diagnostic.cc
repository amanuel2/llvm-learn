#include "amanlang/Basic/Diagnostic.h"


namespace {
const char* DiagnosticText[] = {
#define DIAG(ID, Level, Msg) Msg,
#include "amanlang/Basic/Diagnostic.def"
};

llvm::SourceMgr::DiagKind DiagnosticKinds[] = {
#define DIAG(ID, Level, Msg) llvm::SourceMgr::DK_##Level,
#include "amanlang/Basic/Diagnostic.def"
};
} // namespace

const char* DiagnosticEngine::getDiagnosticText (unsigned ID) {
    return DiagnosticText[ID];
}

llvm::SourceMgr::DiagKind DiagnosticEngine::getDiagnosticKind (unsigned ID) {
    return DiagnosticKinds[ID];
}