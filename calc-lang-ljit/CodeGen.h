#pragma once

#include "AST.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

using llvm::Function;
using llvm::StringMap;

class CodeGen {
    public:
    // CH.8 JIT Changes
    // void run(AST *Tree);
    bool compileToIR (AST* Tree, llvm::Module* M, StringMap<size_t>& JITtedFunctions);
    void prepareCalculationCallFunc (AST* FuncCall, llvm::Module* M, llvm::StringRef FnName, StringMap<size_t>& JITtedFunctions);
};

/// @class LLVMContext
/// @brief Used down below when calling ->getContext() to initate the
/// llvm::Module This is an important class for using LLVM in a threaded
/// context.  It (opaquely) owns and manages the core "global" data of LLVM's
/// core infrastructure, including the type and constant uniquing tables.
/// LLVMContext itself provides no locking guarantees, so you should be careful
/// to have one context per thread.

class ToIRVisitor : public ASTVisitor {
    public:
    ToIRVisitor (llvm::Module* M, StringMap<size_t>& JITFns)
    : M (M), Builder (M->getContext ()), JITFns (JITFns) {
        VoidType  = llvm::Type::getVoidTy (M->getContext ());
        Int32Type = llvm::Type::getInt32Ty (M->getContext ());
        PtrType   = llvm::PointerType::getUnqual (M->getContext ()); // opaque ptr
        Int32Zero = llvm::ConstantInt::get (Int32Type, 0, true);
    }

    void run (AST* Tree);

    // Override visitors (impl)
    virtual void visit (Factor& Node) override;
    virtual void visit (BinaryOp& Node) override;
    virtual void visit (With& Node) override;
    virtual void visit (DefDecl& Node) override;
    virtual void visit (FuncCallFromDef& Node) override;

    //// IR
    // Generate FunctionEval from Def Node
    void genFuncEvaluationCall (AST* Tree) {
        Tree->accept (*this);
    }
    Function* genUserDefinedFunction (llvm::StringRef FnName);

    private:
    llvm::Module* M; // entire compilation stored in module
    llvm::IRBuilder<> Builder;

    // JIT
    StringMap<size_t>& JITFns;

    // Representing Types in LLVM IR
    // To avoid repeated lookups, we cache the needed type instances: VoidTy,
    // Int32Ty, PtrTy, and Int32Zero
    llvm::Type *VoidType, *Int32Type;
    llvm::PointerType* PtrType;

    llvm::Constant* Int32Zero;
    llvm::Value* V;
    llvm::StringMap<llvm::Value*> nameMap;
};