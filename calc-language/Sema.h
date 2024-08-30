#pragma once

#include "AST.h"
#include "Lexer.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/raw_ostream.h"

///
/// The semantic analyzer walks the AST and checks various semantic 
/// rules of the language, e.g. a variable must be declared before use or 
/// types of variables must be compatible in an expression.
///

class Sema {
    public:
        bool sema(AST *Tree);
};


class DeclCheck : public ASTVisitor {
    public:
        DeclCheck() : Err(false) {};
        bool hasErr() { return Err; }

        // Override visitors (impl)
        virtual void visit(Factor &Node) override;
        virtual void visit(BinaryOp &Node) override;
        virtual void visit(With &Node) override;

    private:
        llvm::StringSet<> Scope;
        bool Err;

        enum ErrType { Twice, Not };

    void err(ErrType ET, llvm::StringRef V) {
        llvm::errs() << "Variable " << V << " "
                    << (ET == Twice ? "already" : "not")
                    << " declared\n";
        Err = true;
    }

};