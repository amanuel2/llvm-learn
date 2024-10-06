#include "Sema.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/raw_ostream.h"

////////////////////////////////////////
#pragma mark - Visitors
////////////////////////////////////////

void DeclCheck::visit (Factor& Node) {
    if (Node.getKind () == Factor::Ident) {
        if (Scope.find (Node.getVal ()) == Scope.end ()) {
            err (Not, Node.getVal ());
        }
    }
}

void DeclCheck::visit (BinaryOp& Node) {
    if (Node.getLeft ())
        Node.getLeft ()->accept (*this);
    else {
        Err = true;
    }
    if (Node.getRight ())
        Node.getRight ()->accept (*this);
    else
        Err = true;
}

void DeclCheck::visit (With& Node) {
    helper_visit (Node);
}

/////////// JIT

void DeclCheck::visit (DefDecl& Node) {
    helper_visit (Node);
}

void DeclCheck::visit (FuncCallFromDef& Node) {
    auto FuncCallName                 = Node.getFnName ();
    unsigned NumArgs                  = Node.getArgs ().size ();
    unsigned NumOriginallyDefinedArgs = 0;
    // Check to ensure that the function specified by the user was
    // previously defined.
    auto LookUpFunctionCall = JITFns.find (FuncCallName);
    if (LookUpFunctionCall != JITFns.end ())
        NumOriginallyDefinedArgs = LookUpFunctionCall->second;
    else {
        llvm::errs () << "Specified function definition does not exist!\n";
        Err = true;
    }


    if (NumArgs != NumOriginallyDefinedArgs) {
        llvm::errs () << "Number of parameters specified to the function does not "
                      << "match it's definition!\n";
        Err = true;
    }
}

////////////////////////////////////////
#pragma mark - Sema
////////////////////////////////////////

bool Sema::sema (AST* Tree, StringMap<size_t>& JITFns) {
    DeclCheck DC (JITFns);
    Tree->accept (DC);
    return DC.hasErr ();
}