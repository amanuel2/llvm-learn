#pragma once
#include "llvm/TableGen/Record.h"

// namespace {
void EmitTokensAndKeywordFilter(llvm::RecordKeeper &RK, llvm::raw_ostream &OS);
// }