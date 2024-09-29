#include "llvm/TableGen/Main.h"
#include "TableGenBackends.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/TableGen/Record.h"

enum ActionType {
  PrintRecords,
  DumpJSON,
  GenTokens,
};

namespace {
// Compile Opts

llvm::cl::opt<ActionType> Action(
    llvm::cl::desc("Action to perform:"),
    llvm::cl::values(clEnumValN(PrintRecords, "print-records",
                                "Print all records to stdout (default)"),
                     clEnumValN(DumpJSON, "dump-json",
                                "Dump all records as "
                                "machine-readable JSON"),
                     clEnumValN(GenTokens, "gen-tokens",
                                "Generate token kinds and keyword "
                                "filter")),
    llvm::cl::init(ActionType::PrintRecords)); // Default to printing to ostraem

bool Main(llvm::raw_ostream &os, llvm::RecordKeeper &Records) {
  switch (Action) {
  case PrintRecords:
    os << Records; // just dump to ostream
    break;
  case DumpJSON:
    llvm::EmitJSON(Records, os); // part of TableGen (not us)
    break;
  case GenTokens:
    EmitTokensAndKeywordFilter(Records, os);
    break;
  }

  return false;
}

} // namespace

int main(int argc, char **argv) {
  // When an error signal (such as SIGABRT or SIGSEGV) is delivered to the
  // process, print a stack trace and then exit.
  llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

  /// PrettyStackTraceProgram - This object prints a specified program arguments
  /// to the stream as the stack trace when a crash occurs.
  llvm::PrettyStackTraceProgram X(argc, argv);
  llvm::cl::ParseCommandLineOptions(argc, argv);

  // its destructor just calls llvm_shutdown when destroyed
  // ~llvm_shutdown_obj() { llvm_shutdown(); }
  llvm::llvm_shutdown_obj Y;

  return TableGenMain(argv[0], &Main);
}

#ifdef __has_feature
#if __has_feature(address_sanitizer)
#include <sanitizer/lsan_interface.h>
// Disable LeakSanitizer for this binary as it has too many
// leaks that are not very interesting to fix. See
// compiler-rt/include/sanitizer/lsan_interface.h .
int __lsan_is_turned_off() { return 1; }
#endif // __has_feature(address_sanitizer)
#endif // defined(__has_feature)