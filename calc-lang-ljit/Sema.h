#pragma once

#include "AST.h"
#include "Lexer.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/raw_ostream.h"

using llvm::StringMap;

///
/// The semantic analyzer walks the AST and checks various semantic
/// rules of the language, e.g. a variable must be declared before use or
/// types of variables must be compatible in an expression.
///

class Sema {
    public:
    bool sema (AST* Tree, StringMap<size_t>& JITFns);
};


class DeclCheck : public ASTVisitor {
    public:
    DeclCheck (StringMap<size_t>& JITFns) : Err (false), JITFns (JITFns) {};
    bool hasErr () {
        return Err;
    }

    // Override visitors (impl)
    virtual void visit (Factor& Node) override;
    virtual void visit (BinaryOp& Node) override;
    virtual void visit (With& Node) override;
    virtual void visit (DefDecl& Node) override;
    virtual void visit (FuncCallFromDef& Node) override;

    private:
    llvm::StringSet<> Scope;
    bool Err;

    // JIT
    StringMap<size_t>& JITFns;

    enum ErrType { Twice, Not };

    void err (ErrType ET, llvm::StringRef V) {
        llvm::errs () << "Variable " << V << " " << (ET == Twice ? "already" : "not") << " declared\n";
        Err = true;
    }


    // Typename == With || Def (Can't specify like rs since cpp sucks and Rust rocks)
    template <typename T> void helper_visit (T& Node) {
        for (auto it = Node.begin (); it != Node.end (); it++) {
            if (!Scope.insert (*it).second)
                err (Twice, *it);
        }

        if (Node.getExpr ())
            Node.getExpr ()->accept (*this);
        else
            Err = true;
    }
};