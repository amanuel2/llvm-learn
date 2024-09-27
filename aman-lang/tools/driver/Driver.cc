#include "amanlang/AST/ASTCtx.h"
#include "amanlang/Basic/Diagnostic.h"
#include "amanlang/CodeGen/CodeGen.h"
#include "amanlang/Lexer/Lexer.h"
#include "amanlang/Parser/Parser.h"
#include "amanlang/Sema/Sema.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/WithColor.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"

// #include "llvm-c/Core.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"

#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/Support/TargetSelect.h" // init for MC,Arch,etc...


// Ch.6 (Optimizing IR)
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"


using namespace llvm;

// CodeGenFlags
// static llvm::codegen::RegisterCodeGenFlags CGF;

////////////////////////////////////////////////////////////////////////////////
#pragma mark - Command Line Options
////////////////////////////////////////////////////////////////////////////////

// Option for Target Override
static llvm::cl::opt<std::string> MTriple ("mtriple", llvm::cl::desc ("Override target triple for module"));

// Option for Emiting IR (default:false)
static llvm::cl::opt<bool> EmitIR ("emitir", llvm::cl::desc ("emit"), llvm::cl::init (false));

// Input file
static llvm::cl::opt<std::string> InputFile (
llvm::cl::Positional, // Position == no '-' required so can do ./amanlang test.lang
llvm::cl::desc ("<input-files>"),
llvm::cl::init ("-"));

// Option for output file name
static llvm::cl::opt<std::string>
OutputName ("o", llvm::cl::desc ("Output file name"), llvm::cl::init ("a.out"));

static cl::opt<std::string>
PipelineStartEPPipeline ("passes-ep-pipeline-start", cl::desc ("Pipeline start extension point"));

// Ch.6 CL Opts
static cl::opt<bool> DebugPM ("debug-pass-manager", cl::Hidden, cl::desc ("Print PM debugging information"));
static cl::opt<std::string> PassPipeline ("passes", cl::desc ("A description of the pass pipeline"));
static cl::list<std::string> PassPlugins ("load-pass-plugin", cl::desc ("Load passes from plugin library"));
static cl::opt<signed char> Optimization (cl::desc ("Setting the optimization level:"),
cl::ZeroOrMore, // Zero or more occurrences allowed
cl::values (clEnumValN (3, "O", "Equivalent to -O3"),
clEnumValN (0, "O0", "Optimization level 0"),
clEnumValN (1, "O1", "Optimization level 1"),
clEnumValN (2, "O2", "Optimization level 2"),
clEnumValN (3, "O3", "Optimization level 3"),
clEnumValN (-1,
"Os",
"Like -O2 with extra optimizations "
"for size"),
clEnumValN (-2, "Oz", "Like -Os but reduces code size further")),
cl::init (0));


// The plugin mechanism of LLVM supports a plugin registry for statically linked plugins
// getPProfilerPluginInfo()
#define HANDLE_EXTENSION(Ext) llvm::PassPluginLibraryInfo get##Ext##PluginInfo ();
#include "llvm/Support/Extension.def"


void printVersion (llvm::raw_ostream& OS) {
    OS << "  Default target: " << llvm::sys::getDefaultTargetTriple () << "\n";
    std::string CPU (llvm::sys::getHostCPUName ());
    OS << "  Host CPU: " << CPU << "\n";
    OS << "\n";
    OS.flush ();
    llvm::TargetRegistry::printRegisteredTargetsForVersion (OS);
    exit (EXIT_SUCCESS);
}

std::string outputFilename (StringRef InputFilename) {
    CodeGenFileType FileType = codegen::getFileType ();
    std::string OutputFilename;
    if (InputFilename == "-") {
        OutputFilename = "-";
    } else {
        if (InputFilename.ends_with (".mod") || InputFilename.ends_with (".mod"))
            OutputFilename = InputFilename.drop_back (4).str ();
        else
            OutputFilename = InputFilename.str ();
        switch (FileType) {
        case llvm::CodeGenFileType::AssemblyFile: OutputName.append (EmitIR ? ".ll" : ".s"); break;
        case llvm::CodeGenFileType::ObjectFile: OutputName.append (".o"); break;
        case llvm::CodeGenFileType::Null: OutputName.append (".null"); break;
        }
    }
    return OutputFilename;
}


////////////////////////////////////////////////////////////////////////////////
#pragma mark - Target Machine
////////////////////////////////////////////////////////////////////////////////

// With the target machine instance,
// We can generate IR code that targets a CPU architecture of our choice.
llvm::TargetMachine* createTarget () {
    llvm::Triple Triple = llvm::Triple (
    !MTriple.empty () ? llvm::Triple::normalize (MTriple) : llvm::sys::getDefaultTargetTriple ());

    auto targetOptions = llvm::codegen::InitTargetOptionsFromCodeGenFlags (Triple);

    std::string err;
    auto* Target = llvm::TargetRegistry::lookupTarget (llvm::codegen::getMArch (), Triple, err);

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


    ////////////////////////////////// CH.6 IR-Optimization START //////////////////////////////////

    llvm::PassBuilder PB (TM);

    // loop through the list of plugin libraries given by the user
    // and try to load the opt plugin passed by ./amanlang --load-pass="pass1,pass2,..."
    for (auto& PluginFN : PassPlugins) {
        auto PassPlugin = PassPlugin::Load (PluginFN);
        if (!PassPlugin) {
            WithColor::error (errs (), Argv0)
            << "Failed to load passes from '" << PluginFN << "'. Request ignored.\n";
            continue;
        }
        PassPlugin->registerPassBuilderCallbacks (PB);
    }

/**
 ** Register plugin with PassBuilder instance. Remember for our in-tree built PProfiler plugin we had:
 ** @code
 ** llvm::PassPluginLibraryInfo getPProfilerPluginInfo() {
 **   return {LLVM_PLUGIN_API_VERSION, "PProfiler", "v0.1", &RegisterCBs};
 ** }
 ** @endcode
 ** Where RegisterCBs is a method that registeres via registerPipelineParsingCallback and the ExtensionPoint variant
 **/
#define HANDLE_EXTENSION(Ext) get##Ext##PluginInfo ().RegisterPassBuilderCallbacks (PB);
#include "llvm/Support/Extension.def"

    // Different analysis managers used (we specifically use Module variant for PProfiler)
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    // Register the AA manager first so that our version
    // is the one used.
    FAM.registerPass ([&] { return PB.buildDefaultAAPipeline (); });

    // We must also make sure that the function analysis manager uses the default alias-analysis
    // pipeline and that all analysis managers know about each other
    PB.registerModuleAnalyses (MAM);
    PB.registerCGSCCAnalyses (CGAM);
    PB.registerFunctionAnalyses (FAM);
    PB.registerLoopAnalyses (LAM);
    PB.crossRegisterProxies (LAM, FAM, CGAM, MAM);

    // Let this become the start of the pipeline
    PB.registerPipelineStartEPCallback ([] (ModulePassManager& MPM, OptimizationLevel Level) {
        llvm::outs () << "Starting Pipeline 🏁\n";
    });

    // Our MPM
    llvm::ModulePassManager MPM;

    // If user provided anything via cl ../amanlang --passes="pprofiler,etc..."
    if (!PassPipeline.empty ()) {
        if (auto Err = PB.parsePassPipeline (MPM, PassPipeline)) {
            WithColor::error (errs (), Argv0) << toString (std::move (Err)) << "\n";
            return false;
        }
    } else {
        StringRef DefaultPass;
        // User can pass in -01, -02, etc... but we default to -O0 in cl::init
        switch (Optimization) {
        case 0: DefaultPass = "default<O0>"; break;
        case 1: DefaultPass = "default<O1>"; break;
        case 2: DefaultPass = "default<O2>"; break;
        case 3: DefaultPass = "default<O3>"; break;
        case -1: DefaultPass = "default<Os>"; break;
        case -2: DefaultPass = "default<Oz>"; break;
        }
        if (auto Err = PB.parsePassPipeline (MPM, DefaultPass)) {
            WithColor::error (errs (), Argv0) << toString (std::move (Err)) << "\n";
            return false;
        }
    }

    ////////////////////////////////// CH.6 IR-Optimization END //////////////////////////////////

    // Open output file (For windows needs carriage..)
    std::error_code ec;
    llvm::sys::fs::OpenFlags OpenFlags = llvm::sys::fs::OF_None;
    if (FileType == llvm::CodeGenFileType::AssemblyFile)
        OpenFlags |= llvm::sys::fs::OF_Text;

    auto Out = std::make_unique<llvm::ToolOutputFile> (outputFilename (OutputName), ec, OpenFlags);
    if (ec) {
        llvm::WithColor::error (llvm::errs (), Argv0) << ec.message () << '\n';
        return false;
    }


    llvm::legacy::PassManager PM;
    //// Per LLVM:
    /// The core idea of the TargetIRAnalysis is to expose an interface through
    /// which LLVM targets can analyze and provide information about the middle
    /// end's target-independent IR. This supports use cases such as target-aware
    /// cost modeling of IR constructs.
    PM.add (createTargetTransformInfoWrapperPass (TM->getTargetIRAnalysis ()));
    if (FileType == llvm::CodeGenFileType::AssemblyFile && EmitIR) {
        PM.add (llvm::createPrintModulePass (Out->os ()));
        PM.add (llvm::createPrintModulePass (llvm::outs ())); // own testing
    }

    MPM.run (*M, MAM);
    PM.run (*M);  // Let the Pass Manager run
    Out->keep (); // dont delete file

    return true;
}

// default cpu to host target
void default_cpu () {
    auto atrs = llvm::codegen::getMAttrs ();
    if (llvm::codegen::getMCPU () == "help" ||
    std::any_of (atrs.begin (), atrs.end (), [] (const std::string& a) { return a == "help"; })) {
        auto Triple = llvm::Triple (LLVM_DEFAULT_TARGET_TRIPLE);

        std::string ErrMsg;
        if (auto Target = llvm::TargetRegistry::lookupTarget (Triple.getTriple (), ErrMsg)) {
            llvm::outs () << "Targeting " << Target->getName () << ". ";
            // This prints the available CPUs and features of the
            // target
            auto* info = Target->createMCSubtargetInfo (
            Triple.getTriple (), llvm::codegen::getCPUStr (), llvm::codegen::getFeaturesStr ());
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

static llvm::codegen::RegisterCodeGenFlags CGF ();
static cl::opt<CodeGenFileType> FileType ("filetype",
cl::init (CodeGenFileType::AssemblyFile),
cl::desc ("Choose a file type (not all types are supported by all targets):"),
cl::values (clEnumValN (CodeGenFileType::AssemblyFile, "asm", "Emit an assembly ('.s') file"),
clEnumValN (CodeGenFileType::ObjectFile, "obj", "Emit a native object ('.o') file"),
clEnumValN (CodeGenFileType::Null, "null", "Emit nothing, for performance testing")));

int main (int argc, const char** _argv) {
    llvm::InitLLVM X (argc, _argv);

    // Initialize targets first, so that --version shows registered targets.
    llvm::InitializeAllTargets ();
    llvm::InitializeAllTargetMCs ();
    llvm::InitializeAllAsmPrinters ();
    llvm::InitializeAllAsmParsers ();


    // Initialize codegen and IR passes used by llc so that the -print-after,
    // -print-before, and -stop-after options work.
    llvm::PassRegistry* Registry = llvm::PassRegistry::getPassRegistry ();
    llvm::initializeCore (*Registry);
    llvm::initializeCodeGen (*Registry);


    llvm::outs () << "Begin before\n";
    // Register the Target and CPU printer for --version.
    llvm::cl::AddExtraVersionPrinter (llvm::sys::printDefaultTargetAndDetectedCPU);
    // Register the target printer for --version.
    llvm::cl::AddExtraVersionPrinter (llvm::TargetRegistry::printRegisteredTargetsForVersion);

    //   cl::ParseCommandLineOptions(argc, argv, "llvm system compiler\n");
    // llvm::cl::SetVersionPrinter (&printVersion);
    llvm::cl::ParseCommandLineOptions (argc, _argv, "AmanLang");

    llvm::outs () << "INIT DONE\n";

    default_cpu ();

    llvm::TargetMachine* TM = createTarget ();
    if (!TM)
        exit (EXIT_FAILURE);


    llvm::SmallVector<const char*, 256> argv (_argv + 1, _argv + argc);

    for (const char* Filename : argv) {
        auto File = llvm::MemoryBuffer::getFile (Filename);
        if (std::error_code BufferError = File.getError ()) {
            auto msg = BufferError.message ();
            llvm::errs () << "Error reading " << Filename << ": " << BufferError.message () << "\n";
            continue;
        }

        llvm::outs () << "Going in " << Filename << '\n';
        llvm::SourceMgr SrcMgr;
        DiagnosticEngine Diag (SrcMgr);

        SrcMgr.AddNewSourceBuffer (std::move (File.get ()), llvm::SMLoc ());

        amanlang::Lexer Lex (SrcMgr, Diag);
        auto ASTCtx = amanlang::ASTContext (SrcMgr, Filename);
        amanlang::Sema Sema (Diag);
        amanlang::Parser Parser (Lex, Sema);

        llvm::outs () << "Test\n";
        // Mod
        auto* Mod = Parser.parse ();
        if (Mod && !Diag.numErrors ()) {
            llvm::LLVMContext Ctx;
            if (amanlang::CodeGen* CG = amanlang::CodeGen::create (Ctx, TM, ASTCtx)) {
                std::unique_ptr<llvm::Module> M = CG->run (Mod, InputFile);
                if (!emit (_argv[0], M.get (), TM, InputFile)) {
                    llvm::WithColor::error (llvm::errs (), +_argv[0]) << "Error writing output\n";
                }
                delete CG;
            }
        }
        return 0;
    }
}