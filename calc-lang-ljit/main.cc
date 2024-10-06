#include "CodeGen.h"
#include "Lexer.h"
#include "Parser.h"
#include "Sema.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include <iostream>

namespace {
static llvm::cl::opt<std::string>
Input (llvm::cl::Positional, llvm::cl::desc ("<input>"), llvm::cl::init ("Init"));
}

llvm::ExitOnError ErrExit;

int main (int argc, const char** argv) {

    // initalization
    llvm::InitLLVM X (argc, argv);
    llvm::InitializeNativeTarget ();
    llvm::InitializeAllAsmPrinters ();
    llvm::InitializeAllAsmParsers ();

    assert (llvm::cl::ParseCommandLineOptions (argc, argv));

    auto JIT = ErrExit (llvm::orc::LLJITBuilder ().create ());
    llvm::StringMap<size_t> JITedFunctions;

    while (true) {
        llvm::outs () << "JIT calc > ";
        std::string calcExp;
        std::getline (std::cin, calcExp);

        // Create a new context and module.
        std::unique_ptr<llvm::LLVMContext> Ctx = std::make_unique<llvm::LLVMContext> ();
        std::unique_ptr<llvm::Module> M = std::make_unique<llvm::Module> ("JIT calc.expr", *Ctx);
        M->setDataLayout (JIT->getDataLayout ());

        CodeGen codegen;


        // Inits
        Lexer Lex (Input);
        Parser Parser (Lex);
        Sema sema;

        auto CalcTok = Lex.peek ();
        llvm::outs () << "Test " << Lex.peek () << "," << Input.ValueStr << "\n";
        switch (CalcTok) {
        case Token::KW_def: {
            AST* Tree = Parser.parse ();
            assert (Tree != nullptr && !Parser.hasErr ());
            assert (!sema.sema (Tree, JITedFunctions));
            assert (codegen.compileToIR (Tree, M.get (), JITedFunctions));
            ErrExit (JIT->addIRModule (llvm::orc::ThreadSafeModule (std::move (M), std::move (Ctx)))); /// An LLVM Module together with a shared ThreadSafeContext.
            break;
        }
        case Token::ident: {
            AST* Tree = Parser.parse ();
            assert (Tree != nullptr && !Parser.hasErr ());
            assert (!sema.sema (Tree, JITedFunctions));

            // Ch.8 JIT
            llvm::StringRef FuncCallName = Tree->getFnName ();

            // Prepare the function call to the JITted (previously defined) function.
            codegen.prepareCalculationCallFunc (Tree, M.get (), FuncCallName, JITedFunctions);

            /// First return a JITDylib representing the JIT'd main program.
            // A resource tracker will track the memory that is allocated to the
            // JITted function that calls the user defined function.
            auto RT  = JIT->getMainJITDylib ().createResourceTracker ();
            auto TSM = llvm::orc::ThreadSafeModule (std::move (M), std::move (Ctx));
            ErrExit (JIT->addIRModule (RT, std::move (TSM)));

            // Generate the function call by querying the JIT for the temporary
            // call function (that will call the function defined by the user
            // to evaluate the calculator expression).
            auto CalcExprCall = ErrExit (JIT->lookup ("calc_expr_func"));

            // Get the address of the symbol for "calc_expr_func" (the temporary call
            // function, and cast to the appropriate type. This function returns an
            // integer and takes no arguments. It simply produces a call to the
            // calculator function (previously) defined by the user.
            int (*UserFnCall) () = CalcExprCall.toPtr<int (*) ()> ();
            llvm::outs () << "User defined function evaluated to: " << UserFnCall () << "\n";

            // Free the memory that was previously allocated.
            ErrExit (RT->remove ());
        }
        }

        if (Input.find ("quit") != std::string::npos) {
            llvm::outs () << "Quitting the JIT calc program.\n";
            exit (1);
        }
    }

    return 0;
}