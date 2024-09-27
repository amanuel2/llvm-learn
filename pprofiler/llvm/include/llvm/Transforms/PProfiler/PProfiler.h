#pragma once

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "pprofiler"

ALWAYS_ENABLED_STATISTIC(NumOfFunc, "Number of instrumented functions.");

using llvm::ArrayRef;
using llvm::StringRef;

/////////////////////////////////////////////////////////////////////////////
#pragma mark - PProfiler Pass
/////////////////////////////////////////////////////////////////////////////

namespace llvm {
class PProfilerIRPass : public llvm::PassInfoMixin<PProfilerIRPass> {
public:
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &AM);

private:
  void instrument(llvm::Function &F, llvm::Function *EnterFn,
                  llvm::Function *ExitFn);
};
} // namespace llvm