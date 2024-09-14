#pragma once


#include "amanlang/AST/AST.h"
#include "amanlang/CodeGen/CGModule.h"
#include <llvm/IR/IRBuilder.h>
#include <variant>

namespace llvm {

template <> struct DenseMapInfo<BasicBlock*> {
    static inline BasicBlock* getEmptyKey () {
        return reinterpret_cast<BasicBlock*> (0);
    }
    static inline BasicBlock* getTombstoneKey () {
        return reinterpret_cast<BasicBlock*> (-1);
    }
    static unsigned getHashValue (const BasicBlock* BB) {
        return reinterpret_cast<uintptr_t> (BB);
    }
    static bool isEqual (const BasicBlock* LHS, const BasicBlock* RHS) {
        return LHS == RHS;
    }
};

} // end namespace llvm

namespace amanlang {

class CGProcedure {

    struct BasicBlock {
        llvm::DenseMap<Decl*, llvm::TrackingVH<llvm::Value>> Defs; // Map Var -> defintion
        llvm::DenseMap<llvm::PHINode*, Decl*> IncompletePhis; // Map Phi -> Decl

        unsigned Sealed : 1; // Block is sealed => no more predesscors
        BasicBlock () : Sealed (0) {
        }
    };

    using ExprVariant =
    std::variant<InfixExpression*, PrefixExpression*, VariableAccess*, ConstantAccess*, IntegerLiteral*, BooleanLiteral*>;

    using StmtVariant =
    std::variant<AssignmentStatement*, ProcedureCallStatement*, IfStatement*, WhileStatement*, ReturnStatement*, StmtList&>;

    public:
    explicit CGProcedure (CGModule& CGM)
    : CGM (CGM), Builder (CGM.getLLVMCtx ()), CurrBlk (nullptr) {};
    void run (ProcedureDecl* Proc);

    protected:
    LLVM_ATTRIBUTE_ALWAYS_INLINE void setInsertion (llvm::BasicBlock* BB) {
        CurrBlk = BB;
        Builder.SetInsertPoint (BB);
    }

    // Expr => Value
    llvm::Value* operator() (Expr* expr);
    llvm::Value* operator() (InfixExpression* expr);
    llvm::Value* operator() (PrefixExpression* expr);
    llvm::Value* operator() (VariableAccess* expr) {
        auto* D = expr->getDecl ();
        return readVariable (CurrBlk, D);
    }
    llvm::Value* operator() (ConstantAccess* expr) {
        return this->operator() (expr->geDecl ()->getExpr ());
    }
    LLVM_ATTRIBUTE_ALWAYS_INLINE llvm::Value* operator() (IntegerLiteral* expr) {
        return llvm::ConstantInt::get (CGM.Int64Ty, expr->getValue ());
    }
    LLVM_ATTRIBUTE_ALWAYS_INLINE llvm::Value* operator() (BooleanLiteral* expr) {
        return llvm::ConstantInt::get (CGM.Int1Ty, expr->getValue ());
    }

    // Stmt => Value
    llvm::Value* operator() (const StmtList& Stmts);
    llvm::Value* operator() (AssignmentStatement* Stmt);
    llvm::Value* operator() (ProcedureCallStatement* Stmt);
    llvm::Value* operator() (IfStatement* Stmt);
    llvm::Value* operator() (WhileStatement* Stmt);
    llvm::Value* operator() (ReturnStatement* Stmt);

    private:
    CGModule& CGM;
    ProcedureDecl* ProcDecl;

    llvm::IRBuilder<> Builder;
    llvm::BasicBlock* CurrBlk;

    llvm::Function* Function;
    llvm::FunctionType* FunType;

    llvm::DenseMap<llvm::BasicBlock*, BasicBlock> BlockDefs;
    llvm::DenseMap<FormalParameterDecl*, llvm::Argument*> FormalParams;

    // Read/Write Vars
    void writeVariable (llvm::BasicBlock* BB, Decl* Decl, llvm::Value* Val);
    llvm::Value* readVariable (llvm::BasicBlock* BB, Decl* Decl);

    // Read/Write Local Vars
    void writeLocalVariable (llvm::BasicBlock* BB, Decl* Decl, llvm::Value* Val);
    llvm::Value* readLocalVariable (llvm::BasicBlock* BB, Decl* Decl);
    llvm::Value* readLocalVariableRecursive (llvm::BasicBlock* BB, Decl* Decl);

    // PHINode Manupulation
    llvm::PHINode* addEmptyPhi (llvm::BasicBlock* BB, Decl* Decl);
    llvm::Value* addPhiOperands (llvm::BasicBlock* BB, Decl* Decl, llvm::PHINode* Phi);
    llvm::Value* optimizePhi (llvm::PHINode* Phi);

    // Create Function from our AST's ProcedureDecl
    llvm::FunctionType* createFunctionType (ProcedureDecl* Proc);
    llvm::Function* createFunction (ProcedureDecl* Proc, llvm::FunctionType* FTy);

    // Utils
    llvm::Type* mapType (Decl* Decl, bool HonorReference = true);
    void sealBlock (llvm::BasicBlock* BB);
};

} // namespace amanlang