#include "amanlang/CodeGen/CGProcedure.h"
#include "amanlang/AST/AST.h"

namespace amanlang {

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Init)
/////////////////////////////////////////////////////////////////////////////

void CGProcedure::run (ProcedureDecl* Proc) {
    FunType  = createFunctionType (Proc);
    Function = createFunction (Proc, FunType);

    // create first BB
    llvm::BasicBlock* BB = llvm::BasicBlock::Create (CGM.getLLVMCtx (), "entry", Function);
    setInsertion (BB);

    // We must step through all formal parameters. To handle VAR parameters correctly
    // In contrast to local variables,
    // formal parameters have a value in the first basic block, so we must make these values known
    for (auto [Idx, Arg] : llvm::enumerate (Function->args ())) {
        FormalParameterDecl* FP = Proc->getFormalParams ()[Idx];
        FormalParams[FP]        = &Arg;
        writeLocalVariable (CurrBlk, FP, &Arg);
    }

    auto Block = Proc->getStmts ();
    this->operator() (Block); // call emit on statements

    // The last block after generating the IR code may not be sealed yet, so we must call sealBlock()
    if (!CurrBlk->getTerminator ()) {
        Builder.CreateRetVoid (); // we may have an implicit return
    }
    sealBlock (CurrBlk);
}

void CGProcedure::run() {}

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
    } else if (auto* Var = llvm::dyn_cast<Designator> (E)) {
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

// ch.5
// Examples with CreateInBoundsGEP function

// Value *i32zero = ConstantInt::get(contexet, APInt(32, 0));
// Value *indices[2] = {i32zero, i32zero};
// builder.CreateInBoundsGEP(var, ArrayRef<Value *>(indices, 2));

// Which will yield

// %1 = getelementptr inbounds [10 x i32], [10 x i32]* %0, i32 0, i32 0
llvm::Value* CGProcedure::_handleDesignator (Designator* Var) {
    auto* Decl       = Var->getDecl ();
    llvm::Value* Val = readVariable (CurrBlk, Decl);

    // add array and record support.
    auto& Selectors = Var->getSelectors ();
    for (auto it = Selectors.begin (); it != Selectors.end ();) {
        // Index Selector [3]
        if (auto* IdxSel = llvm::dyn_cast<IndexSelector> (*it)) {
            llvm::SmallVector<llvm::Value*, 4> IdxList;
            while (it != Selectors.end ()) {
                if (auto* Sel = llvm::dyn_cast<IndexSelector> (*it)) {
                    IdxList.push_back (this->operator() (Sel->getIndex ()));
                    ++it;
                } else
                    break;
            }
            Val = Builder.CreateInBoundsGEP (Val->getType (), Val, IdxList);
            Val = Builder.CreateLoad (Val->getType (), Val);
        }
        // Field Selector .xyz
        else if (auto* FieldSel = llvm::dyn_cast<FieldSelector> (*it)) {
            llvm::SmallVector<llvm::Value*, 4> IdxList;
            while (it != Selectors.end ()) {
                if (auto* Sel = llvm::dyn_cast<FieldSelector> (*it)) {
                    llvm::Value* V =
                    llvm::ConstantInt::get (CGM.Int64Ty, Sel->getIndex ());
                    IdxList.push_back (V);
                    ++it;
                } else
                    break;


                Val = Builder.CreateInBoundsGEP (Val->getType (), Val, IdxList);
                Val = Builder.CreateLoad (Val->getType (), Val);
            }
        }

        // Deref *it
        else if (auto* _ = llvm::dyn_cast<DerefSelector> (*it)) {
            Val = Builder.CreateLoad (Val->getType (), Val);
            ++it;
        }

        else {
            llvm::report_fatal_error ("Unsupported selector");
        }
    }

    return Val;
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

    // Write Statement out to variable
    // Desig = Decl + Sel_Lst
    Designator* Desig = Stmt->getVar ();
    auto& Selectors   = Desig->getSelectors ();
    if (Selectors.empty ()) {
        writeVariable (CurrBlk, Desig->getDecl (), Val);
        return Val;
    }

    llvm::SmallVector<llvm::Value*, 4> IdxList;
    // First index for GEP.
    IdxList.push_back (llvm::ConstantInt::get (CGM.Int32Ty, 0));
    auto* Base = readVariable (CurrBlk, Desig->getDecl (), false);
    for (auto I = Selectors.begin (), E = Selectors.end (); I != E; ++I) {
        if (auto* IdxSel = llvm::dyn_cast<IndexSelector> (*I)) {
            IdxList.push_back (this->operator() (IdxSel->getIndex ()));
        } else if (auto* FieldSel = llvm::dyn_cast<FieldSelector> (*I)) {
            llvm::Value* V = llvm::ConstantInt::get (CGM.Int32Ty, FieldSel->getIndex ());
            IdxList.push_back (V);
        } else {
            llvm::report_fatal_error ("not implemented");
        }
    }

    if (!IdxList.empty ()) {
        if (Base->getType ()->isPointerTy ()) {
            Base = Builder.CreateInBoundsGEP (mapType (Desig->getDecl ()), Base, IdxList);
            Builder.CreateStore (Val, Base);
        } else {
            llvm::report_fatal_error ("should not happen");
        }
    }

    return Val;
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
    llvm::BasicBlock::Create (CGM.getLLVMCtx (), "while.cond", Function);
    // The basic block for the while body.
    llvm::BasicBlock* WhileBodyBB =
    llvm::BasicBlock::Create (CGM.getLLVMCtx (), "while.body", Function);
    // The basic block after the while statement.
    llvm::BasicBlock* AfterWhileBB =
    llvm::BasicBlock::Create (CGM.getLLVMCtx (), "after.while", Function);

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
    if (auto* V = llvm::dyn_cast<VariableDecl> (Decl)) {
        if (V->getEnclosingDecl () == ProcDecl)
            writeLocalVariable (BB, Decl, Val);
        else // enclosingDecl => Module
            Builder.CreateStore (Val, CGM.getGlobal (Decl));
    } else if (auto* FP = llvm::dyn_cast<FormalParameterDecl> (Decl)) {
        if (FP->isVar ())
            Builder.CreateStore (Val, FormalParams[FP]);
        else
            writeLocalVariable (BB, Decl, Val);
    }
}

llvm::Value* CGProcedure::readVariable (llvm::BasicBlock* BB, Decl* Decl, bool LoadVal) {
    if (auto* V = llvm::dyn_cast<VariableDecl> (Decl)) {
        if (V->getEnclosingDecl () == ProcDecl)
            return readLocalVariable (BB, Decl);

        if (V->getEnclosingDecl () == CGM.getModuleDeclaration ()) { // enclosingDecl => Module
            auto* Global = CGM.getGlobal (Decl);
            if (LoadVal)
                Builder.CreateLoad (mapType (Decl), Global);
            return Global;
        }
    } else if (auto* FP = llvm::dyn_cast<FormalParameterDecl> (Decl)) {
        if (FP->isVar ()) {
            if (LoadVal)
                Builder.CreateLoad (mapType (Decl), FormalParams[FP]);
            return FormalParams[FP];
        } else
            return readLocalVariable (BB, Decl);
    }
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Read/Write Local Vars)
/////////////////////////////////////////////////////////////////////////////

void CGProcedure::writeLocalVariable (llvm::BasicBlock* BB, Decl* Decl, llvm::Value* Val) {
    assert (BB && "Basic block is nullptr");
    assert (Val && "Value is nullptr");

    BasicBlock blockDef = BlockDefs.at (BB);
    blockDef.Defs[Decl] = llvm::TrackingVH<llvm::Value> (Val);
}


llvm::Value* CGProcedure::readLocalVariable (llvm::BasicBlock* BB, Decl* Decl) {
    assert (BB && "Basic block is nullptr");
    BasicBlock blockDef = BlockDefs.at (BB);
    auto Val            = blockDef.Defs.find (Decl);
    if (Val != blockDef.Defs.end ())
        return Val->second;
    return readLocalVariableRecursive (BB, Decl);
}

llvm::Value* CGProcedure::readLocalVariableRecursive (llvm::BasicBlock* BB, Decl* Decl) {
    llvm::Value* Ret   = nullptr;
    BasicBlock currDef = BlockDefs.at (BB);
    if (!currDef.Sealed) {
        llvm::PHINode* Phi = addEmptyPhi (BB, Decl);
        currDef.IncompletePhis.insert ({ Phi, Decl }); // Add incomplete phi
        Ret = Phi;
    } else if (auto* PredBB = BB->getSinglePredecessor ()) {
        ///// HEADER FILE DOCS /////
        /// Return the predecessor of this block if it has a single
        /// predecessor block. Otherwise return a null pointer.
        //   const BasicBlock *getSinglePredecessor() const;

        /// Return the predecessor of this block if it has a unique predecessor
        /// block. Otherwise return a null pointer.
        ///
        /// Note that unique predecessor doesn't mean single edge, there can be
        /// multiple edges from the unique predecessor to this block (for example a
        /// switch statement with multiple cases having the same destination).
        //   const BasicBlock *getUniquePredecessor() const;
        Ret = readLocalVariable (PredBB, Decl);
    } else { // block is sealed and has multiple preds
        // Create empty phi instruction to break potential
        // cycles.
        llvm::PHINode* Phi = addEmptyPhi (BB, Decl);
        writeLocalVariable (BB, Decl, Phi); // Write empty phi to break cycle
        Ret = addPhiOperands (BB, Decl, Phi); // add pred values as operands to phi
    }

    writeLocalVariable (BB, Decl, Ret);
    return Ret;
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
    for (auto it = llvm::pred_begin (BB); it != llvm::pred_end (BB); it++)
        Phi->addIncoming (readLocalVariable (*it, Decl), *it);
    return optimizePhi (Phi);
}

/**
 * Optimizes a PHI node by replacing it with a single value if possible.
 *
 * This function analyzes the incoming values of the given PHI node and
 * tries to find a single non-PHI value. If such a value is found, it
 * replaces all uses of the PHI node with that value and recursively
 * optimizes any other PHI nodes that used the original PHI node.
 *
 * @param Phi The PHI node to optimize.
 * @return The single value that replaces the PHI node, or the PHI node
 * itself if it cannot be optimized.
 */
llvm::Value* CGProcedure::optimizePhi (llvm::PHINode* Phi) {
    llvm::Value* Prev = nullptr;

    // If the instruction has only one operand or all operands have the same value,
    // then we replace the instruction with this value. If the instruction has no operand,
    // then we replace the instruction with the special Undef value.
    // Only if the instruction has two or more distinct operands do we have to keep the instruction
    for (auto& V : Phi->incoming_values ()) {
        if (V == Prev || V == Phi)
            continue;
        if (Prev && V != Prev)
            return Phi; // Multiple different values, can't optimize

        Prev = V;
    }

    if (Prev == nullptr)
        Prev = llvm::UndefValue::get (Phi->getType ());

    // Removing a phi instruction may lead to optimization opportunities in
    // other phi instructions. Fortunately, LLVM keeps track of the users
    // and the use of values Collect phi instructions using this one
    llvm::SmallVector<llvm::PHINode*, 8> CandidatePhis;
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
#pragma mark - CGProcedure (Function)
/////////////////////////////////////////////////////////////////////////////

// To emit a function in LLVM IR, a function type is needed, which is similar to a prototype in C.
llvm::FunctionType* CGProcedure::createFunctionType (ProcedureDecl* Proc) {
    llvm::Type* retType =
    (!Proc->getRetType ()) ? CGM.VoidTy : mapType (Proc->getRetType ());
    auto params = Proc->getFormalParams ();

    llvm::SmallVector<llvm::Type*, 8> tmp{};
    for (auto param : params)
        tmp.push_back (mapType (param));

    return llvm::FunctionType::get (retType, tmp, false);
}

// Based on the function type, we also create the LLVM function.
// This associates the function type with the linkage and the mangled name:
llvm::Function* CGProcedure::createFunction (ProcedureDecl* Proc, llvm::FunctionType* FTy) {
    auto* func = llvm::Function::Create (
    FTy, llvm::GlobalValue::ExternalLinkage, "", CGM.getModule ());

    // enumerate params
    size_t idx = 0;
    for (auto* it = func->arg_begin (); it != func->arg_end (); it++, idx++) {
        llvm::Argument& Arg     = *it;
        FormalParameterDecl* FP = Proc->getFormalParams ()[idx];

        // Can Change
        if (FP->isVar ()) {
            llvm::AttrBuilder Attr (Function->getContext ());
            llvm::TypeSize Sz = CGM.getModule ()->getDataLayout ().getTypeStoreSize (
            CGM.convertType (FP->getType ()));
            Attr.addDereferenceableAttr (Sz);
            Attr.addAttribute (llvm::Attribute::NoCapture);
            Arg.addAttrs (Attr);
        }

        Arg.setName (FP->getName ());
    }

    return func;
}


/////////////////////////////////////////////////////////////////////////////
#pragma mark - CGProcedure (Utils)
/////////////////////////////////////////////////////////////////////////////

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

    auto it = BlockDefs.find (BB);
    if (it != BlockDefs.end ()) {
        for (auto PhiDecl : it->second.IncompletePhis) {
            addPhiOperands (BB, PhiDecl.second, PhiDecl.first);
        }
        it->second.IncompletePhis.clear ();
        it->second.Sealed = true;
    }
}


llvm::Type* CGProcedure::mapType (Decl* Decl) {
    if (auto* V = llvm::dyn_cast<VariableDecl> (Decl))
        return CGM.convertType (V->getType ());

    if (auto* FP = llvm::dyn_cast<FormalParameterDecl> (Decl)) {
        llvm::Type* Ty = CGM.convertType (FP->getType ());
        if (FP->isVar ())
            Ty = Ty->getPointerTo ();
        return Ty;
    }

    auto* ret = llvm::cast<TypeDecl> (Decl);
    return CGM.convertType (ret);
}

} // namespace amanlang