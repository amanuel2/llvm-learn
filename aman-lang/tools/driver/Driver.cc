#include "amanlang/Basic/Diagnostic.h"
#include "amanlang/Lexer/Lexer.h"
#include "amanlang/Parser/Parser.h"
#include "amanlang/Sema/Sema.h"
#include "llvm/Support/InitLLVM.h"
#include "amanlang/CodeGen/CodeGen.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/WithColor.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ToolOutputFile.h>

#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Pass.h"

#include "llvm/Support/TargetSelect.h" // init for MC,Arch,etc...


#include <system_error>

////////////////////////////////////////////////////////////////////////////////
#pragma mark - Command Line Options
////////////////////////////////////////////////////////////////////////////////

// Option for Target Override
static llvm::cl::opt<std::string>
MTriple ("mtriple", llvm::cl::desc ("Override target triple for module"));

// Option for Emiting IR (default:false)
static llvm::cl::opt<bool>
EmitIR ("emitir", llvm::cl::desc ("emit"), llvm::cl::init (false));

// Input file
static llvm::cl::opt<std::string> InputFile (
llvm::cl::Positional, // Position == no '-' required so can do ./amanlang test.lang
llvm::cl::desc ("<input-files>"),
llvm::cl::init ("-"));


// Option for output file name
static llvm::cl::opt<std::string>
OutputName ("o", llvm::cl::desc ("Output file name"), llvm::cl::init ("a.out"));

void printVersion (llvm::raw_ostream& OS) {
    OS << "  Default target: " << llvm::sys::getDefaultTargetTriple () << "\n";
    std::string CPU (llvm::sys::getHostCPUName ());
    OS << "  Host CPU: " << CPU << "\n";
    OS << "\n";
    OS.flush ();
    llvm::TargetRegistry::printRegisteredTargetsForVersion (OS);
    exit (EXIT_SUCCESS);
}

static llvm::codegen::RegisterCodeGenFlags RCG;

////////////////////////////////////////////////////////////////////////////////
#pragma mark - Target Machine
////////////////////////////////////////////////////////////////////////////////

// With the target machine instance,
// We can generate IR code that targets a CPU architecture of our choice.
llvm::TargetMachine* createTarget () {
    llvm::Triple Triple =
    llvm::Triple (!MTriple.empty () ? llvm::Triple::normalize (MTriple) :
                                      llvm::sys::getDefaultTargetTriple ());

    auto targetOptions = llvm::codegen::InitTargetOptionsFromCodeGenFlags (Triple);

    std::string err;
    auto* Target =
    llvm::TargetRegistry::lookupTarget (llvm::codegen::getMArch (), Triple, err);

    if (!Target) {
        llvm::WithColor::error (llvm::outs ()) << err;
        return nullptr;
    }

    llvm::TargetMachine* TM = Target->createTargetMachine (Triple.getTriple (),
    llvm::codegen::getCPUStr (), llvm::codegen::getFeaturesStr (), targetOptions,
    std::optional<llvm::Reloc::Model> (llvm::codegen::getRelocModel ()));
    return TM;
}

bool emit (llvm::StringRef Argv0, llvm::Module* M, llvm::TargetMachine* TM, llvm::StringRef InputFilename) {
    llvm::CodeGenFileType FileType = llvm::codegen::getFileType ();

    if (OutputName.empty ()) {
        OutputName = "-";
    } else {
        if (InputFilename.endswith (".mod"))
            OutputName = InputFilename.drop_back (4).str ();
        else
            OutputName = InputFilename.str ();


        switch (FileType) {
        case llvm::CodeGenFileType::AssemblyFile:
            OutputName.append (EmitIR ? ".ll" : ".s");
            break;
        case llvm::CodeGenFileType::ObjectFile: OutputName.append (".o"); break;
        case llvm::CodeGenFileType::Null: OutputName.append (".null"); break;
        }
    }

    // Open output file (For windows needs carriage..)
    std::error_code ec;
    llvm::sys::fs::OpenFlags OpenFlags = llvm::sys::fs::OF_None;
    if (FileType == llvm::CodeGenFileType::AssemblyFile)
        OpenFlags |= llvm::sys::fs::OF_TextWithCRLF;

    auto Out = std::make_unique<llvm::ToolOutputFile> (OutputName, ec, OpenFlags);
    if (ec) {
        llvm::WithColor::error (llvm::errs (), Argv0) << ec.message () << '\n';
        return false;
    }


    llvm::legacy::PassManager PM;
    if (FileType == llvm::CodeGenFileType::AssemblyFile && EmitIR) {
        PM.add (llvm::createPrintModulePass (Out->os ()));
        PM.add (llvm::createPrintModulePass (llvm::outs ())); // own testing
    }

    PM.run (*M);  // Emit IR
    Out->keep (); // dont delete file
    return true;
}

// default cpu to host target
void default_cpu () {
    auto atrs = llvm::codegen::getMAttrs ();
    if (llvm::codegen::getMCPU () == "help" ||
    std::any_of (atrs.begin (), atrs.end (),
    [] (const std::string& a) { return a == "help"; })) {
        auto Triple = llvm::Triple (LLVM_DEFAULT_TARGET_TRIPLE);

        std::string ErrMsg;
        if (auto Target = llvm::TargetRegistry::lookupTarget (Triple.getTriple (), ErrMsg)) {
            llvm::outs () << "Targeting " << Target->getName () << ". ";
            // This prints the available CPUs and features of the
            // target
            auto* info = Target->createMCSubtargetInfo (Triple.getTriple (),
            llvm::codegen::getCPUStr (), llvm::codegen::getFeaturesStr ());
            llvm::outs () << info;
        } else {
            llvm::errs () << ErrMsg << "\n";
            exit (EXIT_FAILURE);
        }
        exit (EXIT_SUCCESS);
    }
}

////////////////////////////////////////////////////////////////////////////////
#pragma mark - Main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, const char** _argv) {

    // Initalization
    llvm::InitializeAllTargets ();
    llvm::InitializeAllTargetMCs ();
    llvm::InitializeAllAsmPrinters ();
    llvm::InitializeAllAsmParsers ();

    llvm::cl::SetVersionPrinter (&printVersion);
    llvm::cl::ParseCommandLineOptions (argc, _argv, "AmanLang");

    default_cpu ();

    llvm::TargetMachine* TM = createTarget ();
    if (!TM)
        exit (EXIT_FAILURE);


    llvm::SmallVector<const char*, 256> argv (_argv + 1, _argv + argc);

    for (const char* Filename : argv) {
        auto File = llvm::MemoryBuffer::getFile (Filename);
        if (std::error_code BufferError = File.getError ()) {
            auto msg = BufferError.message ();
            llvm::errs () << "Error reading " << Filename << ": "
                          << BufferError.message () << "\n";
            continue;
        }

        llvm::outs () << "Going in " << Filename << '\n';
        llvm::SourceMgr SrcMgr;
        DiagnosticEngine Diag (SrcMgr);

        SrcMgr.AddNewSourceBuffer (std::move (File.get ()), llvm::SMLoc ());

        amanlang::Lexer Lex (SrcMgr, Diag);
        amanlang::Sema Sema (Diag);
        amanlang::Parser Parser (Lex, Sema);

        //
        auto* Mod = Parser.parse ();
        if (Mod && !Diag.numErrors ()) {
            llvm::LLVMContext Ctx;
            if (amanlang::CodeGen* CG = amanlang::CodeGen::create (Ctx, TM)) {
                std::unique_ptr<llvm::Module> M = CG->run (Mod, InputFile);
                if (!emit (_argv[0], M.get (), TM, InputFile)) {
                    llvm::WithColor::error (llvm::errs (), +_argv[0])
                    << "Error writing output\n";
                }
                delete CG;
            }
        }
        return 0;
    }
}