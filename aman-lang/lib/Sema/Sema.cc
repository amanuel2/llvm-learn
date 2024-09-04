#include "amanlang/Sema/Sema.h"

using namespace amanlang;

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Initalization
/////////////////////////////////////////////////////////////////////////////

void Sema::enterScope (Decl* D) {
    CurScope = new Scope (CurScope);
    CurDecl  = D;
}

void Sema::leaveScope () {
    assert (CurScope && "Can't leave non-existing scope");
    Scope* Parent = CurScope->getParent ();
    delete CurScope;
    CurScope = Parent ?: nullptr;
    CurDecl  = CurDecl->getEnclosingDecl ();
}

bool Sema::isOperatorForType (tok::TokenKind Op, TypeDecl* Ty) {
    switch (Op) {
    case tok::plus:
    case tok::minus:
    case tok::star:
    case tok::kw_DIV:
    case tok::kw_MOD: return Ty == IntegerType;
    case tok::slash: return false; // REAL not implemented
    case tok::kw_AND:
    case tok::kw_OR:
    case tok::kw_NOT: return Ty == BooleanType;
    default: llvm_unreachable ("Unknown operator");
    }
}

void Sema::initalize () {
    IntegerType = new TypeDecl (CurDecl, llvm::SMLoc (), "Integer");
    BooleanType = new TypeDecl (CurDecl, llvm::SMLoc (), "Boolean");

    TrueLiteral  = new BooleanLiteral (true, BooleanType);
    FalseLiteral = new BooleanLiteral (false, BooleanType);

    TrueConst = new ConstantDecl (CurDecl, llvm::SMLoc (), "TrueConst", TrueLiteral);
    FalseConst = new ConstantDecl (CurDecl, llvm::SMLoc (), "FalseConst", FalseLiteral);

    CurScope->insert (IntegerType);
    CurScope->insert (BooleanType);
    CurScope->insert (TrueConst);
    CurScope->insert (FalseConst);
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Action (Declaration)
/////////////////////////////////////////////////////////////////////////////

ModuleDecl* Sema::actOnModuleDeclaration (llvm::SMLoc Loc, llvm::StringRef Name) {
    return new ModuleDecl (CurDecl, Loc, Name);
}

/**
 * Handles the declaration of a module, including its contents.
 *
 * This function is responsible for processing a module declaration, including
 * the list of declarations and statements that make up the module's contents.
 * It performs validation to ensure the module's identifier matches the declared
 * name, and then associates the provided declarations and statements with the
 * module.
 *
 * @param ModDecl The module declaration object to be populated.
 * @param Loc The source location of the module declaration.
 * @param Name The name of the module being declared.
 * @param Decls The list of declarations that make up the module's contents.
 * @param Stmts The list of statements that make up the module's contents.
 * @example `module foo;`
 */
void Sema::actOnModuleDeclaration (ModuleDecl* ModDecl,
llvm::SMLoc Loc,
llvm::StringRef Name,
DeclList& Decls,
StmtList& Stmts) {
    if (Name != ModDecl->getName ()) {
        Diag.report (Loc, diag::err_module_identifier_not_equal);
        Diag.report (ModDecl->getLocation (), diag::note_module_identifier_declaration);
    }
    ModDecl->setDecls (Decls);
    ModDecl->setStmts (Stmts);
}

void Sema::actOnImport (llvm::StringRef ModuleName, IdentList& Ids) {
    Diag.report (llvm::SMLoc (), diag::err_not_yet_implemented);
}

/**
 * Handles the declaration of constant values in the current scope.
 *
 * This function is responsible for creating new constant declarations and
 * adding them to the current scope. It takes the location, name, and
 * expression for the new constant, creates a `ConstantDecl` object, and
 * inserts it into the current scope. If the constant name is already
 * declared in the current scope, an error is reported.
 *
 * @param Decls The list of declarations to add the new constant declaration to.
 * @param Loc The source location of the constant declaration.
 * @param Name The name of the constant being declared.
 * @param E The expression representing the value of the constant.
 */
void Sema::actOnConstantDeclaration (DeclList& Decls, llvm::SMLoc Loc, llvm::StringRef Name, Expr* E) {
    assert (CurScope && "CurrentScope not set");
    ConstantDecl* Decl = new ConstantDecl (CurDecl, Loc, Name, E);
    if (CurScope->insert (Decl))
        Decls.push_back (Decl);
    else
        Diag.report (Loc, diag::err_symbold_declared, Name);
}

/**
 * Handles the declaration of variables in the current scope.
 *
 * This function is responsible for creating new variable declarations and
 * adding them to the current scope. It iterates over the provided list of
 * identifiers and creates a new `VariableDecl` for each one, using the
 * provided type declaration.
 *
 * @param Decls The list of declarations to add the new variable declarations to.
 * @param Ids The list of identifiers to create new variable declarations for.
 * @param D The type declaration to use for the new variable declarations.
 * @example `var x, y: Integer;`
 */
void Sema::actOnVariableDeclaration (DeclList& Decls, IdentList& Ids, Decl* D) {
    assert (CurScope && "CurrentScope not set");
    // or can use isa<>
    if (TypeDecl* Ty = llvm::dyn_cast<TypeDecl> (D)) {
        // Types match (class of)
        for (auto& [Loc, Name] : Ids) {
            VariableDecl* Decl = new VariableDecl (CurDecl, llvm::SMLoc (), Name, Ty);
            if (CurScope->insert (Decl))
                Decls.push_back (Decl);
            else
                Diag.report (Loc, diag::err_symbold_declared, Name);
        }
    } else if (!Ids.empty ()) {
        llvm::SMLoc Loc = Ids.front ().Loc;
        Diag.report (Loc, diag::err_vardecl_requires_type);
    }
}

/**
 * Handles the declaration of formal parameters for a function or method.
 *
 * This function is responsible for creating new formal parameter declarations and
 * adding them to the provided list of parameters. It iterates over the provided list of
 * identifiers and creates a new `FormalParamDecl` for each one, using the
 * provided type declaration.
 *
 * @param Params The list of formal parameter declarations to add the new declarations to.
 * @param Ids The list of identifiers to create new formal parameter declarations for.
 * @param D The type declaration to use for the new formal parameter declarations.
 * @param IsVar Whether the formal parameter is a variable parameter (vs a value parameter).
 * @example `procedure foo (var x: Integer; y: Integer);`
 */
void Sema::actOnFormalParameterDeclaration (FormalParamList& Params,
IdentList& Ids,
Decl* D,
bool IsVar) {
    assert (CurScope && "CurrentScope not set");
    if (TypeDecl* Ty = llvm::dyn_cast<TypeDecl> (D)) {
        for (auto& [Loc, Name] : Ids) {
            FormalParameterDecl* Decl =
            new FormalParameterDecl (CurDecl, Loc, Name, Ty, IsVar);
            if (CurScope->insert (Decl))
                Params.push_back (Decl);
            else
                Diag.report (Loc, diag::err_symbold_declared, Name);
        }
    } else if (!Params.empty ()) {
        llvm::SMLoc Loc = Params.front ()->getLocation ();
        Diag.report (Loc, diag::err_vardecl_requires_type);
    }
}

/**
 * Creates a new procedure declaration and adds it to the current scope.
 *
 * This function is responsible for creating a new `ProcedureDecl` instance and
 * inserting it into the current scope. If a declaration with the same name
 * already exists in the current scope, an error is reported.
 *
 * @param Loc The source location of the procedure declaration.
 * @param Name The name of the procedure.
 * @return The newly created `ProcedureDecl` instance.
 * @example `procedure foo;`
 */
ProcedureDecl* Sema::actOnProcedureDeclaration (llvm::SMLoc Loc, llvm::StringRef Name) {
    ProcedureDecl* P = new ProcedureDecl (CurDecl, Loc, Name);
    if (!CurScope->insert (P))
        Diag.report (Loc, diag::err_symbold_declared, Name);
    return P;
}

/**
 * Creates a new procedure declaration and adds it to the current scope.
 *
 * This function is responsible for creating a new `ProcedureDecl` instance and
 * inserting it into the current scope. If a declaration with the same name
 * already exists in the current scope, an error is reported.
 *
 * @param ProcDecl The new procedure declaration to add to the current scope.
 * @param Loc The source location of the procedure declaration.
 * @param Name The name of the procedure.
 * @param Decls The list of declarations within the procedure.
 * @param Stmts The list of statements within the procedure.
 * @example `procedure foo;`
 */
void Sema::actOnProcedureDeclaration (ProcedureDecl* ProcDecl,
llvm::SMLoc Loc,
llvm::StringRef Name,
DeclList& Decls,
StmtList& Stmts) {
    if (Name != ProcDecl->getName ()) {
        Diag.report (Loc, diag::err_proc_identifier_not_equal);
        Diag.report (ProcDecl->getLocation (), diag::note_proc_identifier_declaration);
    }
    ProcDecl->setDecls (Decls);
    ProcDecl->setStmts (Stmts);
}

/**
 * This function is responsible for creating a new `ProcedureDecl` instance and
 * setting its formal parameters and return type.
 *
 * @param ProcDecl The new procedure declaration to configure.
 * @param Params The list of formal parameters for the procedure.
 * @param RetType The return type declaration for the procedure.
 */
void Sema::actOnProcedureHeading (ProcedureDecl* ProcDecl, FormalParamList& Params, Decl* RetType) {
    ProcDecl->setFormalParams (Params);
    auto Type = llvm::dyn_cast_or_null<TypeDecl> (RetType);
    if (!Type && RetType) {
        Diag.report (RetType->getLocation (), diag::err_returntype_must_be_type);
        return;
    }
    ProcDecl->setRetType (Type);
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Action (Statements)
/////////////////////////////////////////////////////////////////////////////

/**
 * Handles the assignment of an expression to a variable declaration.
 *
 * This function is responsible for creating a new `AssignmentStatement` and
 * adding it to the provided statement list. If the types of the variable
 * declaration and the expression are not compatible, an error is reported.
 *
 * @param Stmts The list of statements to add the assignment to.
 * @param Loc The source location of the assignment.
 * @param D The variable declaration to assign to.
 * @param E The expression to assign to the variable.
 */
void Sema::actOnAssignment (StmtList& Stmts, llvm::SMLoc Loc, Decl* D, Expr* E) {
    if (auto Var = llvm::dyn_cast<VariableDecl> (D)) {
        if (Var->getType () != E->getType ()) {
            Diag.report (Loc, diag::err_types_for_operator_not_compatible,
            tok::getPunctuatorSpelling (tok::colonequal));
        }
        Stmts.push_back (new AssignmentStatement (Var, E));
    } else if (!Stmts.empty ()) {
        llvm::SMLoc Loc = llvm::SMLoc ();
        Diag.report (Loc, diag::err_expected);
    }
}

/**
 * Checks that the formal parameters of a procedure declaration match the actual arguments
 * provided when calling the procedure.
 *
 * This function compares the number of formal parameters to the number of actual arguments,
 * and ensures that the types of the formal parameters and actual arguments are compatible.
 * If any mismatches are found, appropriate error diagnostics are reported.
 *
 * @param Loc The source location of the procedure call.
 * @param Formals The list of formal parameters for the procedure.
 * @param Actuals The list of actual arguments provided in the procedure call.
 */
void Sema::checkFormalAndActualParameters (llvm::SMLoc Loc,
const FormalParamList& Formals,
const ExprList& Actuals) {
    // argument vs param mismatch
    if (Formals.size () != Actuals.size ()) {
        Diag.report (Loc, diag::err_wrong_number_of_parameters);
        return;
    }

    auto A = Actuals.begin ();
    for (auto it = Formals.begin (); it != Formals.end (); ++it, ++A) {
        FormalParameterDecl* F = *it;
        Expr* Arg              = *A;
        if (F->getType () != Arg->getType ())
            Diag.report (Loc, diag::err_type_of_formal_and_actual_parameter_not_compatible);
        if (F->isVar () && llvm::isa<VariableAccess> (Arg))
            Diag.report (Loc, diag::err_var_parameter_requires_var);
    }
}

/**
 * Handles the action of calling a procedure.
 *
 * This function checks that the formal parameters of the procedure declaration
 * match the actual arguments provided in the procedure call. If any mismatches
 * are found, appropriate error diagnostics are reported. If the checks pass, a
 * new ProcedureCallStatement is added to the list of statements.
 *
 * @param Stmts The list of statements to add the procedure call to.
 * @param Loc The source location of the procedure call.
 * @param D The declaration of the procedure being called.
 * @param Params The list of actual arguments provided in the procedure call.
 */
void Sema::actOnProcCall (StmtList& Stmts, llvm::SMLoc Loc, Decl* D, ExprList& Params) {
    if (auto Proc = llvm::dyn_cast<ProcedureDecl> (D)) {
        checkFormalAndActualParameters (Loc, Proc->getFormalParams (), Params);
        Stmts.push_back (new ProcedureCallStatement (Proc, Params));
    } else {
        Diag.report (Loc, diag::err_expected);
    }
}


/**
 * Handles the action of an if statement.
 *
 * This function takes the condition expression, the list of statements for the
 * if block, and the list of statements for the else block, and adds a new IfStatement to the list of statements.
 *
 * @param Stmts The list of statements to add the if statement to.
 * @param Loc The source location of the if statement.
 * @param Cond The condition expression for the if statement.
 * @param IfStmts The list of statements for the if block.
 * @param ElseStmts The list of statements for the else block.
 */
void Sema::actOnIfStatement (StmtList& Stmts,
llvm::SMLoc Loc,
Expr* Cond,
StmtList& IfStmts,
StmtList& ElseStmts) {
    if (!Cond)
        Cond = FalseLiteral;

    //     if (auto Proc = llvm::dyn_cast<BooleanLiteral> (Cond)) {
    if (Cond->getType () != BooleanType)
        Diag.report (Loc, diag::err_if_expr_must_be_bool);
    Stmts.push_back (new IfStatement (Cond, IfStmts, ElseStmts));
}

/**
 * Handles the action of a while statement.
 *
 * This function takes the condition expression and the list of statements for
 * the while block, and adds a new WhileStatement to the list of statements.
 *
 * @param Stmts The list of statements to add the while statement to.
 * @param Loc The source location of the while statement.
 * @param Cond The condition expression for the while statement.
 * @param WhileStmts The list of statements for the while block.
 */
void Sema::actOnWhileStatement (StmtList& Stmts, llvm::SMLoc Loc, Expr* Cond, StmtList& WhileStmts) {
    if (!Cond)
        Cond = FalseLiteral;

    //     if (auto Proc = llvm::dyn_cast<BooleanLiteral> (Cond)) {
    if (Cond->getType () != BooleanType)
        Diag.report (Loc, diag::err_if_expr_must_be_bool);
    Stmts.push_back (new WhileStatement (Cond, WhileStmts));
}

/**
 * Handles the action of a return statement.
 *
 * This function takes the return value expression and adds a new
 * ReturnStatement to the list of statements. It performs various checks to
 * ensure the return value is compatible with the function's return type.
 *
 * @param Stmts The list of statements to add the return statement to.
 * @param Loc The source location of the return statement.
 * @param RetVal The return value expression.
 */
void Sema::actOnReturnStatement (StmtList& Stmts, llvm::SMLoc Loc, Expr* RetVal) {
    auto Cur = llvm::cast<ProcedureDecl> (CurDecl);

    if (Cur->getRetType () && !RetVal)
        Diag.report (Loc, diag::err_function_requires_return);
    if (!Cur->getRetType () && RetVal)
        Diag.report (Loc, diag::err_procedure_requires_empty_return);
    if ((Cur->getRetType () && RetVal) && Cur->getRetType () != RetVal->getType())
        Diag.report (Loc, diag::err_function_and_return_type);

    Stmts.push_back (new ReturnStatement (RetVal));
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Action (Expressions)
/////////////////////////////////////////////////////////////////////////////

/**
 * Combines two expressions using the given operator and returns the resulting expression.
 *
 * This function takes two expressions and an operator information object, and creates a new
 * InfixExpression that represents the combination of the two expressions using the given
 * operator. It performs type checking to ensure the operands are compatible, and sets the
 * resulting expression's type and constness appropriately.
 *
 * @param Left The left-hand expression.
 * @param Right The right-hand expression.
 * @param Op The operator information object.
 * @return The resulting expression.
 * @example `x + y`, `x * y`, `x and y`, etc.
 */
Expr* Sema::actOnExpression (Expr* Left, Expr* Right, const OperatorInfo& Op) {
    // Relation
    if (!Left || !Right)
        return Left ?: Right;

    if (Left->getType () != Right->getType ()) {
        Diag.report (Op.getLocation (), diag::err_types_for_operator_not_compatible,
        tok::getPunctuatorSpelling (Op.getKind ()));
    }

    return new InfixExpression (
    Left, Right, Op, BooleanType, Left->isConst () && Right->isConst ());
}

/**
 * Handles the action of a simple expression.
 *
 * This function takes two expressions and an operator, and creates a new
 * InfixExpression representing the result of applying the operator to the
 * two expressions. It performs various checks to ensure the operands are
 * compatible with the operator.
 *
 * @param Left The left-hand operand expression.
 * @param Right The right-hand operand expression.
 * @param Op The operator to apply to the operands.
 * @return A new InfixExpression representing the result of the operation.
 */
Expr* Sema::actOnSimpleExpression (Expr* Left, Expr* Right, const OperatorInfo& Op) {
    if (!Left || !Right)
        return Left ?: Right;

    if (Left->getType () != Right->getType ()) {
        Diag.report (Op.getLocation (), diag::err_types_for_operator_not_compatible,
        tok::getPunctuatorSpelling (Op.getKind ()));
    }

    bool IsConst = Left->isConst () && Right->isConst ();
    if (IsConst && Op.getKind () == tok::kw_OR) {
        BooleanLiteral* L = llvm::dyn_cast<BooleanLiteral> (Left);
        BooleanLiteral* R = llvm::dyn_cast<BooleanLiteral> (Right);
        return L->getValue () || R->getValue () ? TrueLiteral : FalseLiteral;
    }

    return new InfixExpression (
    Left, Right, Op, BooleanType, Left->isConst () && Right->isConst ());
}

/**
 * Handles the action of a term expression.
 *
 * This function takes two expressions and an operator, and creates a new
 * InfixExpression representing the result of applying the operator to the
 * two expressions. It performs various checks to ensure the operands are
 * compatible with the operator.
 *
 * @param Left The left-hand operand expression.
 * @param Right The right-hand operand expression.
 * @param Op The operator to apply to the operands.
 * @return A new InfixExpression representing the result of the operation.
 */
Expr* Sema::actOnTerm (Expr* Left, Expr* Right, const OperatorInfo& Op) {
    if (!Left || !Right)
        return Left ?: Right;

    if (Left->getType () != Right->getType ()) {
        Diag.report (Op.getLocation (), diag::err_types_for_operator_not_compatible,
        tok::getPunctuatorSpelling (Op.getKind ()));
    }

    bool IsConst = Left->isConst () && Right->isConst ();
    if (IsConst && Op.getKind () == tok::kw_AND) {
        BooleanLiteral* L = llvm::dyn_cast<BooleanLiteral> (Left);
        BooleanLiteral* R = llvm::dyn_cast<BooleanLiteral> (Right);
        return L->getValue () || R->getValue () ? TrueLiteral : FalseLiteral;
    }

    return new InfixExpression (
    Left, Right, Op, BooleanType, Left->isConst () && Right->isConst ());
}

/**
 * Handles the action of a prefix expression.
 *
 * This function takes an expression and an operator, and creates a new
 * expression representing the result of applying the operator to the
 * expression. It performs various checks to ensure the operand is
 * compatible with the operator.
 *
 * @param E The operand expression.
 * @param Op The operator to apply to the operand.
 * @return A new expression representing the result of the operation, or
 * nullptr if the operation is invalid.
 * @code
        MODULE NegationExamples;

        VAR
        a, b, c: INTEGER;

        BEGIN
        (* Unambiguous cases - no warning *)
        a := -5;  (* Negation of an integer literal *)
        b := -a;  (* Negation of a variable *)

        (* Unambiguous cases - no warning *)
        c := -(a * b);  (* Negation of a multiplication *)
        c := -(a / b);  (* Negation of a division *)

        (* Ambiguous cases - will generate a warning *)
        c := -a + b;  (* Is this -(a + b) or (-a) + b? *)
        c := -a - b;  (* Is this -(a - b) or (-a) - b? *)

        (* To avoid ambiguity, use parentheses *)
        c := -(a + b);  (* Clearly negating the sum *)
        c := (-a) + b;  (* Clearly adding the negation of a to b *)
        END NegationExamples.
 *  @endcode
 */
Expr* Sema::actOnPrefixExpression (Expr* E, const OperatorInfo& Op) {
    if (!E)
        return nullptr;
    if (E->isConst ())
        return llvm::dyn_cast<BooleanLiteral> (E)->getValue () ? TrueLiteral : FalseLiteral;

    if (Op.getKind () == tok::TokenKind::minus) {
        bool Ambigious = true;
        if (llvm::isa<VariableAccess> (E) || llvm::isa<ConstantAccess> (E) ||
        llvm::isa<IntegerLiteral> (E))
            Ambigious = false;
        if (auto Infix = llvm::dyn_cast<InfixExpression> (E)) {
            auto OpKind = Infix->getOperatorInfo().getKind();
            if (OpKind == tok::TokenKind::star || OpKind == tok::TokenKind::slash)
                Ambigious = false;
        }
        if (Ambigious)
            Diag.report (Op.getLocation (), diag::warn_ambigous_negation);
    }

    return new PrefixExpression (E, Op, E->getType (), E->isConst ());
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Action (Literal and Identifiers)
/////////////////////////////////////////////////////////////////////////////

/**
 * Parses an integer literal and returns an `IntegerLiteral` expression.
 *
 * This function handles integer literals in decimal, hexadecimal, and other
 * radixes. It supports 64-bit integer values.
 *
 * @param Loc The source location of the integer literal.
 * @param Literal The string representation of the integer literal.
 * @return An `IntegerLiteral` expression representing the parsed integer value.
 */
Expr* Sema::actOnIntegerLiteral (llvm::SMLoc Loc, llvm::StringRef Literal) {
    uint8_t Radix = 10;
    // Hex
    if (Literal.ends_with ("H")) {
        Literal = Literal.drop_back ();
        Radix   = 16;
    } else if (Literal.starts_with ("0x")) {
        Literal = Literal.drop_front (2);
        Radix   = 16;
    }

    llvm::APInt Value (64, Literal, Radix);
    return new IntegerLiteral (Loc, llvm::APSInt (Value), IntegerType);
}

/**
 * Resolves a variable declaration to an expression representing the variable access.
 *
 * This function takes a declaration for a variable, parameter, or constant, and
 * returns an expression that represents accessing that variable or constant.
 * For boolean constants, it returns the appropriate `TrueLiteral` or `FalseLiteral`
 * expression.
 *
 * @param D The declaration to resolve to an expression.
 * @return An expression representing the variable or constant access, or `nullptr`
 *         if the declaration is not a valid variable, parameter, or constant.
 */
Expr* Sema::actOnVariable (Decl* D) {
    if (!D)
        return nullptr;
    if (auto* V = llvm::dyn_cast<VariableDecl> (D))
        return new VariableAccess (V);
    else if (auto* P = llvm::dyn_cast<FormalParameterDecl> (D))
        return new VariableAccess (P);
    else if (auto* C = llvm::dyn_cast<ConstantDecl> (D)) {
        if (C == TrueConst || C == FalseConst)
            return (C == TrueConst) ? TrueLiteral : FalseLiteral;

        return new ConstantAccess (C);
    }
    return nullptr;
}

/**
 * Handles a function call expression.
 *
 * This function takes a declaration for a procedure and a list of argument expressions,
 * and returns a function call expression. It first checks that the number and types of
 * the actual parameters match the formal parameters of the procedure. If the declaration
 * is not for a valid procedure, it reports an error.
 *
 * @param D The declaration of the procedure being called.
 * @param Params The list of argument expressions for the function call.
 * @return A function call expression representing the call, or nullptr if the declaration
 *         is not for a valid procedure.
 *
 * @par
 * Key Differences:

    Return Value:

    Functions return a value
    Procedures do not return a value


    Usage in Expressions:

    Function calls can be used within expressions
    Procedure calls are standalone statements


    Error Handling:

    Function calls produce an error if called on a non-function
    Procedure calls produce an error if called on a non-procedure


    Implementation:

    Function calls create a FunctionCallExpr object
    Procedure calls create a ProcedureCallStatement object


    Type Checking:

    Functions check for a return type
    Procedures don't require a return type check
 * @endparblock
 */
Expr* Sema::actOnFunctionCall (Decl* D, ExprList& Params) {
    if (!D)
        return nullptr;
    if (auto* P = llvm::dyn_cast<ProcedureDecl> (D)) {
        checkFormalAndActualParameters (D->getLocation (), P->getFormalParams (), Params);
        if (!P->getRetType ())
            Diag.report (D->getLocation (), diag::err_function_call_on_nonfunction);
        return new FunctionCallExpr (P, Params);
    }
    Diag.report (D->getLocation (), diag::err_function_call_on_nonfunction);
    return nullptr;
}

/**
 * @brief Handles a qualified identifier part during name lookup.
 *
 * This function is responsible for resolving a single part of a qualified
 * identifier during the name lookup process. It takes the previous declaration
 * (if any), the location of the identifier, and the name of the identifier, and
 * returns the resulting declaration.
 *
 * @param Prev The previous declaration in the qualified identifier, or nullptr
 * if this is the first part.
 * @param Loc The source location of the identifier.
 * @param Name The name of the identifier.
 * @return The declaration corresponding to the qualified identifier part, or
 * nullptr if the name could not be resolved.
 * @example `foo.bar`, `foo.bar.baz`, etc.
 */
Decl* Sema::actOnQualIdentPart (Decl* Prev, llvm::SMLoc Loc, llvm::StringRef Name) {
    if (!Prev) {
        if (Decl* D = CurScope->lookup (Name))
            return D;
    } else if (auto* Mod = llvm::dyn_cast<ModuleDecl> (Prev)) {
        auto Decls = Mod->getDecls ();
        for (auto it = Decls.begin (); it != Decls.end (); it++)
            if ((*it)->getName () == Name)
                return *it;
    } else {
        llvm_unreachable ("actOnQualIdentPart only callable "
                          "with module declarations");
    }

    Diag.report (Loc, diag::err_undeclared_name, Name);
    return nullptr;
}
