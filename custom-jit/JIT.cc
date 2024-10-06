
#include "JIT.h"
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

llvm::Expected<std::unique_ptr<JIT>> JIT::create() {
  /// String pool for symbol names used by the JIT.
  auto SSP = std::make_shared<llvm::orc::SymbolStringPool>();

  /// ExecutorProcessControl supports interaction with a JIT target process.
  /// A ExecutorProcessControl implementation targeting the current process.
  /// (SelfTarget)
  // class SelfTarget : (ExecutorProcessControl, InProcessMemoryAccess)
  auto EPC = llvm::orc::SelfExecutorProcessControl::Create(SSP);
  assert(EPC);

  // Target Triple: <arch><sub_arch>-<vendor>-<sys>-<env>
  // Ex:           13.0.0-x86_64-apple-darwin.tar.gz

  llvm::orc::JITTargetMachineBuilder JTMB((*EPC)->getTargetTriple());
  auto DL = JTMB.getDefaultDataLayoutForTarget();
  assert(DL);

  /// An ExecutionSession represents a running JIT program.
  auto ES = std::make_unique<llvm::orc::ExecutionSession>(std::move(*EPC));
  return std::make_unique<JIT>(std::move(*EPC), std::move(ES), std::move(*DL), std::move(JTMB));
}

JIT::JIT(std::unique_ptr<llvm::orc::ExecutorProcessControl> EPCtrl, std::unique_ptr<llvm::orc::ExecutionSession> ExeS,
         llvm::DataLayout DataL, llvm::orc::JITTargetMachineBuilder JTMB)

    : EPC(std::move(EPCtrl)), ES(std::move(ExeS)), DL(std::move(DataL)), Mangle(*ES, DL),

      ObjectLinkingLayer(std::move(createObjectLinkingLayer(*ES, JTMB))),

      CompileLayer(std::move(createCompileLayer(*ES, *ObjectLinkingLayer, std::move(JTMB)))),

      OptIRLayer(std::move(createOptIRLayer(*ES, *CompileLayer))),

      MainJITDylib(ES->createBareJITDylib("<main>")) {

  // add a generator to search the current process for symbols.
  MainJITDylib.addGenerator(llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(DL.getGlobalPrefix())));
}

std::unique_ptr<llvm::orc::RTDyldObjectLinkingLayer> JIT::createObjectLinkingLayer(llvm::orc::ExecutionSession &ES,
                                                                                   llvm::orc::JITTargetMachineBuilder &JTMB) {
  /// This memory manager allocates all section memory as read-write.  The
  /// RuntimeDyld will copy JITed section memory into these allocated blocks
  /// and perform any necessary linking and relocations.
  auto GetMemoryManager = []() { return std::make_unique<llvm::SectionMemoryManager>(); };
  auto OLLayer = std::make_unique<llvm::orc::RTDyldObjectLinkingLayer>(ES, GetMemoryManager);
  if (JTMB.getTargetTriple().isOSBinFormatCOFF()) {
    OLLayer->setOverrideObjectFlagsWithResponsibilityFlags(true);
    OLLayer->setAutoClaimResponsibilityForObjectSymbols(true);
  }
  return OLLayer;
}

std::unique_ptr<llvm::orc::IRCompileLayer> JIT::createCompileLayer(llvm::orc::ExecutionSession &ES,
                                                                   llvm::orc::RTDyldObjectLinkingLayer &OLLayer,
                                                                   llvm::orc::JITTargetMachineBuilder JTMB) {
  auto IRCompiler = std::make_unique<llvm::orc::ConcurrentIRCompiler>(std::move(JTMB));
  auto IRCLayer = std::make_unique<llvm::orc::IRCompileLayer>(ES, OLLayer, std::move(IRCompiler));
  return IRCLayer;
}

std::unique_ptr<llvm::orc::IRTransformLayer> JIT::createOptIRLayer(llvm::orc::ExecutionSession &ES,
                                                                   llvm::orc::IRCompileLayer &CompileLayer) {
  auto OptIRLayer = std::make_unique<llvm::orc::IRTransformLayer>(ES, CompileLayer, optimizeModule);
  return OptIRLayer;
}

llvm::Expected<llvm::orc::ThreadSafeModule> JIT::optimizeModule(llvm::orc::ThreadSafeModule TSM,
                                                                const llvm::orc::MaterializationResponsibility &R) {
  TSM.withModuleDo([](llvm::Module &M) {
    // define a couple of analysis managers and register them
    llvm::PassBuilder PB;
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    FAM.registerPass([&] { return PB.buildDefaultAAPipeline(); });

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    // Optimize simply with an 02 Pipeline
    llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    MPM.run(M, MAM);
  });

  return TSM;
}
