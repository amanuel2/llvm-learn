#include "amanlang/CodeGen/CGProcedure.h"
#include "amanlang/AST/AST.h"

namespace amanlang {

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Init)
/////////////////////////////////////////////////////////////////////////////

void CGProcedure::run (ProcedureDecl* Proc) {
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Emit - Expr)
/////////////////////////////////////////////////////////////////////////////

llvm::Value* CGProcedure::operator() (InfixExpression* E) {
    llvm::Value* Left   = this->operator() (E->getLeft ());
    llvm::Value* Right  = this->operator() (E->getRight ());
    llvm::Value* Result = nullptr;

    // Creates operator from UserOperator
    // And inserts it into IRBuilder
    switch (E->getOperatorInfo ().getKind ()) {
    case tok::plus: Result = Builder.CreateNSWAdd (Left, Right); break;
    case tok::minus: Result = Builder.CreateNSWSub (Left, Right); break;
    case tok::star: Result = Builder.CreateNSWMul (Left, Right); break;
    case tok::kw_DIV: Result = Builder.CreateSDiv (Left, Right); break;
    case tok::kw_MOD: Result = Builder.CreateSRem (Left, Right); break;
    case tok::equal: Result = Builder.CreateICmpEQ (Left, Right); break;
    case tok::hash: Result = Builder.CreateICmpNE (Left, Right); break;
    case tok::less: Result = Builder.CreateICmpSLT (Left, Right); break;
    case tok::lessequal: Result = Builder.CreateICmpSLE (Left, Right); break;
    case tok::greater: Result = Builder.CreateICmpSGT (Left, Right); break;
    case tok::greaterequal: Result = Builder.CreateICmpSGE (Left, Right); break;
    case tok::kw_AND: Result = Builder.CreateAnd (Left, Right); break;
    case tok::kw_OR: Result = Builder.CreateOr (Left, Right); break;
    case tok::slash:
        // Divide by real numbers not supported.
        [[fallthrough]];
    default: llvm_unreachable ("Wrong operator");
    }
}

llvm::Value* CGProcedure::operator() (PrefixExpression* E) {
    llvm::Value* Result = this->operator() (E->getExpr ());
    switch (E->getOperatorInfo ().getKind ()) {
    case tok::plus: break;
    case tok::minus: Result = Builder.CreateNeg (Result); break;
    case tok::kw_NOT: Result = Builder.CreateNot (Result); break;
    default: llvm_unreachable ("Wrong operator used for prefix");
    }
    return Result;
}

llvm::Value* CGProcedure::operator() (Expr* E) {
    if (auto* Infix = llvm::dyn_cast<InfixExpression> (E)) {
        return (*this) (Infix);
    } else if (auto* Prefix = llvm::dyn_cast<PrefixExpression> (E)) {
        return (*this) (Prefix);
    } else if (auto* Var = llvm::dyn_cast<VariableAccess> (E)) {
        return (*this) (Var);
    } else if (auto* Const = llvm::dyn_cast<ConstantAccess> (E)) {
        return (*this) (Const);
    } else if (auto* IntLit = llvm::dyn_cast<IntegerLiteral> (E)) {
        return (*this) (IntLit);
    } else if (auto* BoolLit = llvm::dyn_cast<BooleanLiteral> (E)) {
        return (*this) (BoolLit);
    } else {
        llvm_unreachable ("Unknown expression type");
    }
}


/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Emit - Stmt)
/////////////////////////////////////////////////////////////////////////////

llvm::Value* CGProcedure::operator() (const StmtList& Stmts) {
    for (auto* S : Stmts) {
        if (auto* Stmt = llvm::dyn_cast<AssignmentStatement> (S))
            this->operator() (Stmt);
        else if (auto* Stmt = llvm::dyn_cast<ProcedureCallStatement> (S))
            this->operator() (Stmt);
        else if (auto* Stmt = llvm::dyn_cast<IfStatement> (S))
            this->operator() (Stmt);
        else if (auto* Stmt = llvm::dyn_cast<WhileStatement> (S))
            this->operator() (Stmt);
        else if (auto* Stmt = llvm::dyn_cast<ReturnStatement> (S))
            this->operator() (Stmt);
        else
            llvm_unreachable ("Unknown statement");
    }
}

llvm::Value* CGProcedure::operator() (AssignmentStatement* Stmt) {
    auto* Val = this->operator() (Stmt->getExpr ());
    writeVariable (CurrBlk, Stmt->getVar (), Val);
}

llvm::Value* CGProcedure::operator() (ProcedureCallStatement* Stmt) {
    llvm::report_fatal_error ("not implemented");
}

llvm::Value* CGProcedure::operator() (IfStatement* Stmt) {
    bool HasElse = Stmt->getElseStmts ().size () > 0;
    // Create BB's
    llvm::BasicBlock* IfBB =
    llvm::BasicBlock::Create (CGM.getLLVMCtx (), "if.body", Function);
    llvm::BasicBlock* ElseBB = HasElse ?
    llvm::BasicBlock::Create (CGM.getLLVMCtx (), "else.body", Function) :
    nullptr;
    llvm::BasicBlock* AfterIfBB =
    llvm::BasicBlock::Create (CGM.getLLVMCtx (), "after.if", Function);

    // Cond Val + BranchInst
    auto* Cond                    = this->operator() (Stmt->getCond ());
    [[maybe_unused]] auto* CondBB = Builder.CreateCondBr (Cond, IfBB, ElseBB);

    sealBlock (CurrBlk);
    setInsertion (CurrBlk);

    // If Val + BranchInst
    auto* If_V = this->operator() (Stmt->getIfStmts ());
    if (!CurrBlk->getTerminator ())
        Builder.CreateBr (AfterIfBB);
    sealBlock (CurrBlk);

    // Else Val + BranchInst
    if (HasElse) {
        setInsertion (ElseBB);
        [[maybe_unused]] auto* Else_Bi = this->operator() (Stmt->getElseStmts ());
        if (!CurrBlk->getTerminator ())
            Builder.CreateBr (AfterIfBB);
        sealBlock (CurrBlk);
    }
    setInsertion (AfterIfBB);


    return If_V; // Doesn't matter for now
}

llvm::Value* CGProcedure::operator() (WhileStatement* Stmt) {
    // Condition Block + BranchInst
    llvm::BasicBlock* WhileCondBB =
    llvm::BasicBlock::Create (CGM.getLLVMCtx (), "while.cond", Fn);
    // The basic block for the while body.
    llvm::BasicBlock* WhileBodyBB =
    llvm::BasicBlock::Create (CGM.getLLVMCtx (), "while.body", Fn);
    // The basic block after the while statement.
    llvm::BasicBlock* AfterWhileBB =
    llvm::BasicBlock::Create (CGM.getLLVMCtx (), "after.while", Fn);

    [[maybe_unused]] auto* While_Cond_BR = Builder.CreateBr (WhileCondBB);
    sealBlock (CurrBlk);
    setInsertion (WhileCondBB);
    llvm::Value* Cond = this->operator() (Stmt->getCond ());
    Builder.CreateCondBr (Cond, WhileBodyBB, AfterWhileBB);

    setInsertion (WhileBodyBB);
    this->operator() (Stmt->getStmts ());
    [[maybe_unused]] auto* While_Block_BR = Builder.CreateBr (WhileCondBB);

    sealBlock (WhileCondBB);
    sealBlock (CurrBlk);
    setInsertion (AfterWhileBB);
}

llvm::Value* CGProcedure::operator() (ReturnStatement* Stmt) {
    if (Stmt->getExpr ()) {
        auto* V = this->operator() (Stmt->getExpr ());
        return Builder.CreateRet (V);
    }

    return Builder.CreateRetVoid ();
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Read/Write Vars)
/////////////////////////////////////////////////////////////////////////////

void CGProcedure::writeVariable (llvm::BasicBlock* BB, Decl* Decl, llvm::Value* Val) {
}

llvm::Value* CGProcedure::readVariable (llvm::BasicBlock* BB, Decl* Decl) {
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Read/Write Local Vars)
/////////////////////////////////////////////////////////////////////////////

void CGProcedure::writeLocalVariable(llvm::BasicBlock* BB, Decl* Decl, llvm::Value* Val) {
    assert(BB && "Basic block is nullptr");
    assert(Val && "Value is nullptr");

    auto& blockDef = BlockDefs.at(*BB);
    blockDef.Defs[Decl] = llvm::TrackingVH<llvm::Value>(Val);
}


llvm::Value* CGProcedure::readLocalVariable (llvm::BasicBlock* BB, Decl* Decl) {
  assert(BB && "Basic block is nullptr");
  auto Val = BlockDefs[BB].Defs.find(Decl);
  if (Val != BlockDefs[BB].Defs.end())
    return Val->second;
  return readLocalVariableRecursive(BB, Decl);
}

llvm::Value* CGProcedure::readLocalVariableRecursive (llvm::BasicBlock* BB, Decl* Decl) {
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Phi Nodes)
/////////////////////////////////////////////////////////////////////////////

llvm::PHINode* CGProcedure::addEmptyPhi (llvm::BasicBlock* BB, Decl* Decl) {
    return BB->empty () ?
    llvm::PHINode::Create (mapType (Decl), 0, "", BB) :
    llvm::PHINode::Create (mapType (Decl), 0, "", &BB->front ());
}

llvm::Value*
CGProcedure::addPhiOperands (llvm::BasicBlock* BB, Decl* Decl, llvm::PHINode* Phi) {
    for (auto* PredBB : llvm::predecessors (BB))
        Phi->addIncoming (readLocalVariable (PredBB, Decl), PredBB);
    return optimizePhi (Phi);
}

/**
 * Optimizes a PHI node by replacing it with a single value if possible.
 *
 * This function analyzes the incoming values of the given PHI node and tries to
 * find a single non-PHI value. If such a value is found, it replaces all uses
 * of the PHI node with that value and recursively optimizes any other PHI nodes
 * that used the original PHI node.
 *
 * @param Phi The PHI node to optimize.
 * @return The single value that replaces the PHI node, or the PHI node itself
 *         if it cannot be optimized.
 */
llvm::Value* CGProcedure::optimizePhi (llvm::PHINode* Phi) {
    llvm::Value* Prev = nullptr;

    // Try to find a single non-Phi value
    for (auto* V : Phi->incoming_values ()) {
        if (V == Prev || V == Phi)
            continue;
        if (Prev && V != Prev)
            return Phi; // Multiple different values, can't optimize

        Prev = V;
    }

    if (Prev == nullptr)
        Prev = llvm::UndefValue::get (Phi->getType ());

    // Collect phi instructions using this one
    llvm::SmallVector<llvm::PHINode*, 8> CandidatePhis;
    // A Use represents the edge between a Value definition and its users
    for (llvm::Use& U : Phi->uses ()) {
        if (auto* P = llvm::dyn_cast<llvm::PHINode> (U.getUser ()))
            if (P != Phi)
                CandidatePhis.push_back (P);
    }

    // Replace all uses of this Phi with the single value
    Phi->replaceAllUsesWith (Prev);
    Phi->eraseFromParent ();

    // Recursively optimize Phis that used this one
    for (auto* P : CandidatePhis)
        optimizePhi (P);

    return Prev;
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Utils)
/////////////////////////////////////////////////////////////////////////////

llvm::Type* CGProcedure::mapType (Decl* Decl, bool HonorReference) {
}

/**
 * Seals a basic block by adding any incomplete PHI node operands and marking the block as sealed.
 *
 * When a basic block is created, any PHI nodes in the block may have incomplete operands. This function
 * finds any such incomplete PHI nodes, adds the missing operands, and marks the block as sealed so that
 * no further changes can be made to it.
 *
 * @param BB The basic block to seal.
 */
void CGProcedure::sealBlock (llvm::BasicBlock* BB) {

    auto it = BlockDefs.find (*BB);
    if (it != BlockDefs.end ()) {
        for (auto PhiDecl : it->second.IncompletePhis) {
            addPhiOperands (BB, PhiDecl.second, PhiDecl.first);
        }
        it->second.IncompletePhis.clear ();
        it->second.Sealed = true;
    }
}

} // namespace amanlang