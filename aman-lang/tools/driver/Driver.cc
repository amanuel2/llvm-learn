#include "amanlang/Basic/Diagnostic.h"
#include "amanlang/Lexer/Lexer.h"
#include "amanlang/Parser/Parser.h"
#include "amanlang/Sema/Sema.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/raw_ostream.h"

#include <iostream>


int main (int argc, const char** _argv) {
    // llvm::InitLLVM X(argc, argv);
    // llvm::outs() << "Hello, World!\n";
    llvm::SmallVector<const char*, 256> argv (_argv + 1, _argv + argc);

    for (const char* Filename : argv) {
        auto File = llvm::MemoryBuffer::getFile (Filename);
        // if (auto code = File.getError()) {
        //     llvm::errs() << "Error retrieving file with code " <<
        //     code.message() << "\n"; return 1;
        // }
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
        Parser.parse ();
        return 0;
    }
}