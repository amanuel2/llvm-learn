#pragma once

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/CompileUtils.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/IRCompileLayer.h"
#include "llvm/ExecutionEngine/Orc/IRTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"

class JIT {

public:
  explicit JIT(std::unique_ptr<llvm::orc::ExecutorProcessControl> EPCtrl, std::unique_ptr<llvm::orc::ExecutionSession> ExeS,
               llvm::DataLayout DataL, llvm::orc::JITTargetMachineBuilder JTMB);

  // Generic Create
  static llvm::Expected<std::unique_ptr<JIT>> create();

  // Create Object Link (Executable) LAYER
  static std::unique_ptr<llvm::orc::RTDyldObjectLinkingLayer> createObjectLinkingLayer(
      llvm::orc::ExecutionSession &ES,
      llvm::orc::JITTargetMachineBuilder &JTMB); // link in-memory objects and turning them into executable code.

  // Create Compile (IR -> Obj) LAYER
  static std::unique_ptr<llvm::orc::IRCompileLayer> createCompileLayer(llvm::orc::ExecutionSession &ES,
                                                                       llvm::orc::RTDyldObjectLinkingLayer &OLLayer,
                                                                       llvm::orc::JITTargetMachineBuilder JTMB);
  // Optimize IR and generate more IR lol
  static std::unique_ptr<llvm::orc::IRTransformLayer> createOptIRLayer(llvm::orc::ExecutionSession &ES,
                                                                       llvm::orc::IRCompileLayer &CompileLayer);
  static llvm::Expected<llvm::orc::ThreadSafeModule> optimizeModule(llvm::orc::ThreadSafeModule TSM,
                                                                    const llvm::orc::MaterializationResponsibility &R);

  // Helpers //

  llvm::Expected<llvm::orc::ExecutorSymbolDef> lookup(llvm::StringRef Name) {
    return ES->lookup({&MainJITDylib}, Mangle(Name.str()));
  }

  llvm::Error addIRModule(llvm::orc::ThreadSafeModule TSM, llvm::orc::ResourceTrackerSP RT = nullptr) {
    if (!RT)
      RT = MainJITDylib.getDefaultResourceTracker();
    return OptIRLayer->add(RT, std::move(TSM));
  }

private:
  // Structure

  /// An ExecutionSession represents a running JIT program.
  /// ExecutorProcessControl supports interaction with a JIT target process.
  std::unique_ptr<llvm::orc::ExecutionSession> ES;
  std::unique_ptr<llvm::orc::ExecutorProcessControl> EPC;

  llvm::DataLayout DL;                 // Coincide with what target we are compiling for
  llvm::orc::MangleAndInterner Mangle; // Same as DL coincides
  std::unique_ptr<llvm::orc::RTDyldObjectLinkingLayer> ObjectLinkingLayer;
  std::unique_ptr<llvm::orc::IRCompileLayer> CompileLayer;
  std::unique_ptr<llvm::orc::IRTransformLayer> OptIRLayer;

  /// This class aims to mimic the behavior of a regular dylib or shared object,
  /// but without requiring the contained program representations to be compiled
  /// up-front.
  llvm::orc::JITDylib &MainJITDylib;

  // Actual Optimization
};
