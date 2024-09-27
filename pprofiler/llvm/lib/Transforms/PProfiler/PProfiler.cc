#include "llvm/Passes/PassBuilder.h"
#include <llvm/Transforms/PProfiler/PProfiler.h>

namespace llvm {
/////////////////////////////////////////////////////////////////////////////
#pragma mark - PProfiler Pass
/////////////////////////////////////////////////////////////////////////////

llvm::PreservedAnalyses PProfilerIRPass::run(llvm::Module &M,
                                             llvm::ModuleAnalysisManager &AM) {
  // Do not instrument the runtime functions.
  if (M.getFunction("__ppp_enter") || M.getFunction("__ppp_exit")) {
    return llvm::PreservedAnalyses::all();
  }

  // Create the function type and functions.
  auto *VoidTy = llvm::Type::getVoidTy(M.getContext());
  auto *PtrTy = llvm::PointerType::getUnqual(M.getContext());
  auto *EnterExitFty = llvm::FunctionType::get(VoidTy, {PtrTy}, false);

  // Create Funcs
  auto *EnterFn = llvm::Function::Create(
      EnterExitFty, llvm::GlobalValue::ExternalLinkage, "__ppp_enter", M);
  auto *ExitFn = llvm::Function::Create(
      EnterExitFty, llvm::GlobalValue::ExternalLinkage, "__ppp_exit", M);

  // instrument enter/exit functions into each found function
  for (auto &F : M.functions()) {
    if (!F.isDeclaration() && F.hasName())
      instrument(F, EnterFn, ExitFn);
  }

  return llvm::PreservedAnalyses::none();
}

// To be called for each functoin to be instrumented (metrics)
void PProfilerIRPass::instrument(llvm::Function &F, llvm::Function *EnterFn,
                                 llvm::Function *ExitFn) {
  ++NumOfFunc;
  // Set the insertion point to begin of first block. (Setting insertion point
  // at entry)
  llvm::IRBuilder<> Builder(&*F.getEntryBlock().begin());

  // Create global constant for the function name.
  llvm::GlobalVariable *FnName = Builder.CreateGlobalString(F.getName());
  // Call the EnterFn at function entry.
  Builder.CreateCall(EnterFn->getFunctionType(), EnterFn, {FnName});

  // Find all Ret instructions, and call ExitFn before.
  for (llvm::BasicBlock &BB : F) {
    for (llvm::Instruction &Inst : BB) {
      if (Inst.getOpcode() == llvm::Instruction::Ret) {
        Builder.SetInsertPoint(&Inst);
        Builder.CreateCall(ExitFn->getFunctionType(), ExitFn, {FnName});
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - LLVM integration
/////////////////////////////////////////////////////////////////////////////

// Register call back with pipeline
void RegisterCBs(llvm::PassBuilder &PB) {
  // Add our pass into the pipeline (as a callback)
  PB.registerPipelineParsingCallback(
      [](StringRef Name, llvm::ModulePassManager &MPM,
         ArrayRef<llvm::PassBuilder::PipelineElement>) {
        if (Name == "PProfiler") {
          MPM.addPass(PProfilerIRPass());
          return true;
        }
        return false;
      });

  /// This extension point allows adding optimization once at the
  /// start of the opt pipeline.
  PB.registerPipelineStartEPCallback(
      [](llvm::ModulePassManager &PM, llvm::OptimizationLevel Level) {
        PM.addPass(PProfilerIRPass());
      });
}

llvm::PassPluginLibraryInfo getPProfilerPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "PProfiler", "v0.1", &RegisterCBs};
}

// llvmGetPassPluginInfo() == The public entry point for a pass plugin.
/// ```
/// extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
/// llvmGetPassPluginInfo() {
///   return {
///     LLVM_PLUGIN_API_VERSION, "MyPlugin", "v0.1", [](PassBuilder &PB) { ... }
///   };
/// }
/// ```
#ifndef LLVM_PPROFILER_LINK_INTO_TOOLS
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getPProfilerPluginInfo();
}
#endif
} // namespace llvm