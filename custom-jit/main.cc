#include "JIT.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"

using namespace llvm;
namespace {
static llvm::cl::opt<std::string> InputFile(cl::Positional, cl::Required, cl::desc("<input-file>"));

Error jitmain(std::unique_ptr<Module> M, std::unique_ptr<LLVMContext> Ctx, int argc, char *argv[]) {
  auto JIT = JIT::create();
  assert(JIT);

  if (auto Err = (*JIT)->addIRModule(orc::ThreadSafeModule(std::move(M), std::move(Ctx))))
    return Err;

  auto MainSym = (*JIT)->lookup("main");
  assert(MainSym);

  llvm::orc::ExecutorAddr MainExecutorAddr = MainSym->getAddress();
  auto *Main = MainExecutorAddr.toPtr<int(int, char **)>();

  (void)Main(argc, argv);
  return Error::success();
}
} // namespace

int main(int argc, char *argv[]) {
  // Initalization
  InitLLVM X(argc, argv);
  llvm::SMDiagnostic Err;

  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();

  // Parse CLI args
  cl::ParseCommandLineOptions(argc, argv, "aman-jit\n");

  auto Ctx = std::make_unique<LLVMContext>();
  auto Mod = llvm::parseIRFile(argv[0], Err, *Ctx);
  assert(Mod);

  ExitOnError EOE;
  EOE(jitmain(std::move(Mod), std::move(Ctx), argc, argv));

  return 0;
}