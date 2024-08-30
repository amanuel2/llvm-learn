#include "Lexer.h"
#include "Parser.h"
#include "Sema.h"
#include "CodeGen.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/raw_ostream.h"

namespace {
static llvm::cl::opt<std::string> Input(llvm::cl::Positional,
                                        llvm::cl::desc("<input>"),
                                        llvm::cl::init("Init"));
}

int main(int argc, const char **argv) {
    llvm::InitLLVM X(argc, argv);
    assert(llvm::cl::ParseCommandLineOptions(argc, argv));

    // Lex & Build the tree
    Lexer Lex(Input);
    Parser Parser(Lex);
    AST* Tree = Parser.parse();

    assert(Tree!=nullptr);
    if (Parser.hasErr()) {
        llvm::errs() << "Parser error\n";
        return 1;
    }

    // Check any semantic errors
    Sema sema;
    assert(!sema.sema(Tree));

    // CodeGen to LLVM IR
    CodeGen codegen;
    assert(codegen.compile(Tree));

    return 0;
}