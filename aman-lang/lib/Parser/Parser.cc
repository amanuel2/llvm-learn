#include "amanlang/Parser/Parser.h"
#include "amanlang/AST/AST.h"
#include "amanlang/Basic/TokenKinds.h"
#include "amanlang/Sema/Sema.h"

namespace amanlang {
OperatorInfo fromTok (Token Tok) {
    return OperatorInfo (Tok.getLocation (), Tok.getKind ());
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Parser (SPI)
/////////////////////////////////////////////////////////////////////////////


ModuleDecl* Parser::parse () {
    ModuleDecl* ModDecl = nullptr;
    parseCompilationUnit (ModDecl);
    return ModDecl;
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Parser (Module)
/////////////////////////////////////////////////////////////////////////////

/**
 * Parses the compilation unit of an Aman language module.
 *
 * This function parses the compilation unit of an Aman language module, which
 * consists of a module declaration, zero or more import statements, and a block
 * of declarations and statements. The parsed module declaration is stored in the
 * provided `D` parameter, and the parsed declarations and statements are added
 * to the provided `Decls` and `Stmts` lists, respectively.
 *
 * @param D The module declaration to store the parsed module declaration in.
 * @return `true` if the compilation unit was parsed successfully, `false` otherwise.
 */
bool Parser::parseCompilationUnit (ModuleDecl*& D) {
    llvm::outs () << "parseCompilationUnit\n";
    auto handle_err = [this] {
        llvm::outs () << "handle_err\n";
        return skipUntil ();
    };
    if (!consume (tok::kw_MODULE) || !expect (tok::identifier)) {
        return handle_err ();
    }

    llvm::outs () << "parseCompilationUnit\n";
    EnterDecl enter (Actions, D);
    advance ();
    // parse headears
    while (Tok.isOneOf (tok::TokenKind::kw_IMPORT, tok::TokenKind::kw_MODULE)) {
        if (!this->parseImport ())
            return handle_err ();
    }

    DeclList Decls;
    StmtList Stmts;
    llvm::outs () << "parseBlock\n";
    if (!parseBlock (Decls, Stmts) || !expect (tok::identifier))
        return handle_err ();


    // Check Semantics
    Actions.actOnModuleDeclaration (D, Tok.getLocation (), Tok.getName (), Decls, Stmts);

    advance ();
    return (!consume (tok::period)) ? handle_err () : true;
}

/**
 * Parses an import statement.
 *
 * This function parses an import statement, which can be in one of two forms:
 * 1. `IMPORT <identifier_list>;`
 * 2. `FROM <module_name> IMPORT <identifier_list>;`
 *
 * The parsed identifiers are added to the `Ids` list, and the module name (if present)
 * is stored in `ModuleName`. The parsed import statement is then passed to the
 * `actOnImport` semantic action.
 *
 * @param Ids The list of identifiers to add the parsed identifiers to.
 * @param ModuleName The module name, if present in the import statement.
 * @return `true` if the import statement was parsed successfully, `false` otherwise.
 */
bool Parser::parseImport () {
    auto _errorhandler = [this] {
        return skipUntil (tok::kw_BEGIN, tok::kw_CONST, tok::kw_END, tok::kw_FROM,
        tok::kw_IMPORT, tok::kw_PROCEDURE, tok::kw_VAR, tok::kw_TYPE);
    };
    IdentList Ids;
    llvm::StringRef ModuleName;
    if (Tok.is (tok::kw_FROM)) {
        advance ();
        if (!expect (tok::identifier))
            return _errorhandler ();
        ModuleName = Tok.getIdentifier ();
        advance ();
    }
    if (!consume (tok::kw_IMPORT) || !parseIdentList (Ids) || !expect (tok::semi))
        return _errorhandler ();

    // Check Semantics
    Actions.actOnImport (ModuleName, Ids);

    advance ();
    return true;
}

/**
 * Parses a block of declarations and statements.
 *
 * This function parses a block of declarations (constants, procedures, and
 * variables) followed by an optional block of statements. The parsed
 * declarations are added to the provided `Decls` list, and the parsed
 * statements are added to the provided `Stmts` list.
 *
 * @param Decls The list of declarations to add the parsed declarations to.
 * @param Stmts The list of statements to add the parsed statements to.
 * @return `true` if the block was parsed successfully, `false` otherwise.
 */
bool Parser::parseBlock (DeclList& Decls, StmtList& Stmts) {
    auto handle_err = [this] () { return skipUntil (tok::identifier); };
    llvm::outs () << "parseBlock\n";

    while (Tok.isOneOf (tok::kw_CONST, tok::kw_PROCEDURE, tok::kw_VAR, tok::kw_TYPE))
        if (!parseDeclaration (Decls))
            return handle_err ();

    if (Tok.is (tok::kw_BEGIN)) {
        advance ();
        if (!parseStatement (Stmts))
            return handle_err ();
    } else if (!Tok.is (tok::kw_END)) {
        handle_err ();
    }

    return true;
}


/**
 * Parses a type declaration in the current scope.
 *
 * This function parses a type declaration, which can be one of the following:
 * - An alias type declaration
 * - A pointer type declaration
 * - An array type declaration
 * - A record type declaration
 *
 * The parsed type declaration is then added to the provided `Decls` list.
 *
 * @param Decls The list of declarations to add the parsed type declaration to.
 * @return `true` if the type declaration was parsed successfully, `false` otherwise.
 */
bool Parser::parseTypeDeclaration (DeclList& Decls) {
    auto _errorhandler = [this] { return skipUntil (tok::semi); };
    if (!expect (tok::identifier))
        return _errorhandler ();

    auto Loc  = Tok.getLocation ();
    auto Name = Tok.getIdentifier ();
    advance ();

    if (!consume (tok::equal))
        return _errorhandler ();

    if (Tok.is (tok::identifier)) {
        Decl* D;
        if (!parseQualident (D))
            return _errorhandler ();
        Actions.actOnAliasTypeDeclaration (Decls, Loc, Name, D);
    } else if (Tok.is (tok::kw_POINTER)) {
        advance ();
        if (!expect (tok::kw_TO))
            return _errorhandler ();
        Decl* D;
        advance ();
        if (!parseQualident (D))
            return _errorhandler ();
        Actions.actOnPointerTypeDeclaration (Decls, Loc, Name, D);
    } else if (Tok.is (tok::kw_ARRAY)) {
        advance ();
        if (!expect (tok::l_square))
            return _errorhandler ();
        Expr* E = nullptr;
        advance ();
        if (!parseExpression (E))
            return _errorhandler ();
        if (!consume (tok::r_square))
            return _errorhandler ();
        if (!expect (tok::kw_OF))
            return _errorhandler ();
        Decl* D;
        advance ();
        if (parseQualident (D))
            return _errorhandler ();
        Actions.actOnArrayTypeDeclaration (Decls, Loc, Name, E, D);
    } else if (Tok.is (tok::kw_RECORD)) {
        FieldList Fields;
        advance ();
        if (!parseFieldList (Fields))
            return _errorhandler ();
        if (!expect (tok::kw_END))
            return _errorhandler ();
        Actions.actOnRecordTypeDeclaration (Decls, Loc, Name, Fields);
        advance ();
    } else {
        /*ERROR*/
        return _errorhandler ();
    }

    return true;
}

bool Parser::parseFieldList (FieldList& Fields) {
    auto _errorhandler = [this] { return skipUntil (tok::kw_END); };
    if (!parseField (Fields))
        return _errorhandler ();
    while (Tok.is (tok::semi)) {
        advance ();
        if (!parseField (Fields))
            return _errorhandler ();
    }
    return true;
}

bool Parser::parseField (FieldList& Fields) {
    auto _errorhandler = [this] { return skipUntil (tok::semi, tok::kw_END); };
    Decl* D;
    IdentList Ids;
    if (!parseIdentList (Ids))
        return _errorhandler ();
    if (!consume (tok::colon))
        return _errorhandler ();
    if (!parseQualident (D))
        return _errorhandler ();
    Actions.actOnFieldDeclaration (Fields, Ids, D);
    return false;
}

bool Parser::parseSelectors (Expr*& E) {
    auto _errorhandler = [this] {
        return skipUntil (tok::hash, tok::r_paren, tok::star, tok::plus,
        tok::comma, tok::minus, tok::slash, tok::colonequal, tok::semi,
        tok::less, tok::lessequal, tok::equal, tok::greater, tok::greaterequal,
        tok::kw_AND, tok::kw_DIV, tok::kw_DO, tok::kw_ELSE, tok::kw_END,
        tok::kw_MOD, tok::kw_OR, tok::kw_THEN, tok::r_square);
    };
    while (Tok.isOneOf (tok::period, tok::l_square, tok::caret)) {
        if (Tok.is (tok::caret)) {
            Actions.actOnDereferenceSelector (E, Tok.getLocation ());
            advance ();
        } else if (Tok.is (tok::l_square)) {
            auto Loc    = Tok.getLocation ();
            Expr* IndexE = nullptr;
            advance ();
            if (!parseExpression (IndexE))
                return _errorhandler ();
            if (!expect (tok::r_square))
                return _errorhandler ();
            Actions.actOnIndexSelector (E, Loc, IndexE);
            advance ();
        } else if (Tok.is (tok::period)) {
            advance ();
            if (!expect (tok::identifier))
                return _errorhandler ();
            Actions.actOnFieldSelector (E, Tok.getLocation (), Tok.getIdentifier ());
            advance ();
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Parser (Declarations)
/////////////////////////////////////////////////////////////////////////////

/**
 * Parses a declaration in the current scope.
 *
 * This function parses a declaration, which can be one of the following:
 * - A constant declaration
 * - A variable declaration
 * - A procedure declaration
 *
 * The parsed declaration is then added to the provided `Decls` list.
 *
 * @param Decls The list of declarations to add the parsed declaration to.
 * @return `true` if the declaration was parsed successfully, `false` otherwise.
 */
bool Parser::parseDeclaration (DeclList& Decls) {
    auto handle_err = [this] () {
        return skipUntil (tok::kw_BEGIN, tok::kw_CONST, tok::kw_END,
        tok::kw_PROCEDURE, tok::kw_VAR);
    };

    switch (Tok.getKind ()) {
    case tok::identifier:
        advance ();
        while (Tok.is (tok::identifier))
            if (!parseConstantDeclaration (Decls) || !consume (tok::semi))
                return handle_err ();
        break;
    case tok::kw_VAR:
        advance ();
        while (Tok.is (tok::identifier))
            if (!parseVariableDeclaration (Decls) || !consume (tok::semi))
                return handle_err ();
        break;
    case tok::kw_PROCEDURE:
        return (!parseProcedureDeclaration (Decls) || !consume (tok::semi)) ?
        handle_err () :
        true;
    default: return handle_err ();
    }

    return true;
}

/**
 * Parses a constant declaration in the current scope.
 *
 * This function parses a constant declaration, which consists of an identifier
 * followed by an expression that defines the constant's value. The parsed
 * declaration is then added to the provided `Decls` list.
 *
 * @param Decls The list of declarations to add the parsed constant declaration to.
 * @return `true` if the constant declaration was parsed successfully, `false` otherwise.
 */
bool Parser::parseConstantDeclaration (DeclList& Decls) {
    auto handle_err = [this] () { return skipUntil (tok::semi); };
    if (!expect (tok::identifier))
        return handle_err ();

    llvm::SMLoc Loc      = Tok.getLocation ();
    llvm::StringRef Name = Tok.getIdentifier ();

    advance ();
    if (!expect (tok::equal))
        return handle_err ();


    advance ();
    Expr* E = nullptr;
    if (!parseExpression (E))
        return handle_err ();

    // Check Semantics
    Actions.actOnConstantDeclaration (Decls, Loc, Name, E);
    return true;
}

/**
 * Parses a variable declaration in the current scope.
 *
 * This function parses a variable declaration, which consists of an identifier
 * list followed by a type declaration. The parsed declaration is then added to
 * the provided `Decls` list.
 *
 * @param Decls The list of declarations to add the parsed variable declaration to.
 * @return `true` if the variable declaration was parsed successfully, `false` otherwise.
 */
bool Parser::parseVariableDeclaration (DeclList& Decls) {
    auto handle_err = [this] () { return skipUntil (tok::semi); };
    Decl* D;
    IdentList Ids;

    if (!parseIdentList (Ids) || !consume (tok::colon) || !parseQualident (D))
        return handle_err ();

    // Check Semantics
    Actions.actOnVariableDeclaration (Decls, Ids, D);
    return true;
}

/**
 * Parses a procedure declaration in the input stream.
 *
 * This function is responsible for parsing the syntax of a procedure
 * declaration, including the procedure name, formal parameters, and return
 * type. It also handles the parsing of the procedure body, including any local
 * declarations and statements.
 *
 * @param ParentDecls The list of declarations that the procedure declaration
 *                    should be added to.
 * @return `true` if the procedure declaration was parsed successfully, `false`
 *         otherwise.
 */
bool Parser::parseProcedureDeclaration (DeclList& ParentDecls) {
    auto handle_err = [this] () { return skipUntil (tok::semi); };
    if (!consume (tok::kw_PROCEDURE) || !expect (tok::identifier))
        return handle_err ();

    // Check Semantics + (Add to curr scope)
    ProcedureDecl* D =
    Actions.actOnProcedureDeclaration (Tok.getLocation (), Tok.getIdentifier ());

    EnterDecl S (Actions, D);
    FormalParamList Params;
    Decl* RetType = nullptr;
    advance ();

    // Parameters
    if (Tok.is (tok::l_paren))
        if (!parseFormalParameters (Params, RetType))
            return handle_err ();

    // Semantics + (Set Params + Ret Type)
    Actions.actOnProcedureHeading (D, Params, RetType);
    if (!expect (tok::semi))
        return handle_err ();


    DeclList Decls;
    StmtList Stmts;
    advance ();
    if (!parseBlock (Decls, Stmts) || !expect (tok::identifier))
        return handle_err ();

    // Semantics + (Sets StmtList & DeclList)
    Actions.actOnProcedureDeclaration (
    D, Tok.getLocation (), Tok.getIdentifier (), Decls, Stmts);

    // Finish
    ParentDecls.push_back (D);
    advance ();
    return true;
}

/**
 * Parses a list of formal parameters in the input stream.
 *
 * This function is responsible for parsing a list of formal parameters, which
 * can include multiple parameters separated by semicolons. Each parameter is
 * parsed using the `parseFormalParameter` function and added to the provided
 * `FormalParamList`.
 *
 * @param Params The formal parameter list to add the parsed parameters to.
 * @param RetType The return type of the procedure, if present.
 * @return `true` if the parameter list was parsed successfully, `false` otherwise.
 * @example formal parameter list: `(var x, y: integer)`
 */
bool Parser::parseFormalParameters (FormalParamList& Params, Decl*& RetType) {
    auto handle_err = [this] () { return skipUntil (tok::semi); };
    if (!consume (tok::l_paren))
        return handle_err ();

    if (Tok.isOneOf (tok::kw_VAR, tok::identifier))
        if (!parseFormalParameterList (Params))
            return handle_err ();

    if (!consume (tok::r_paren))
        return handle_err ();

    if (Tok.is (tok::colon)) {
        advance ();
        if (!parseQualident (RetType))
            return handle_err ();
    }

    return true;
}

/**
 * Parses a list of formal parameters in the parser.
 *
 * This function is responsible for parsing a list of formal parameters, which
 * can include multiple parameters separated by semicolons. Each parameter is
 * parsed using the `parseFormalParameter` function and added to the provided
 * `FormalParamList`.
 *
 * @param Params The formal parameter list to add the parsed parameters to.
 * @return `true` if the parameter list was parsed successfully, `false` otherwise.
 */
bool Parser::parseFormalParameterList (FormalParamList& Params) {
    auto handle_err = [this] () { return skipUntil (tok::r_paren); };
    if (!parseFormalParameter (Params))
        return handle_err ();
    while (Tok.is (tok::semi)) {
        advance ();
        if (!parseFormalParameter (Params))
            return handle_err ();
    }
    return true;
}

/**
 * Parses a formal parameter declaration in the parser.
 *
 * This function is responsible for parsing a formal parameter declaration, which
 * can include a variable parameter (prefixed with 'var') and a type
 * specification. The parsed parameter is added to the provided `FormalParamList`.
 *
 * @param Params The formal parameter list to add the parsed parameter to.
 * @return `true` if the parameter was parsed successfully, `false` otherwise.
 * @example formal parameter list: `var x, y: integer`
 */
bool Parser::parseFormalParameter (FormalParamList& Params) {
    auto handle_err = [this] () { return skipUntil (tok::r_paren, tok::semi); };
    IdentList Ids;
    Decl* D;

    bool IsVar = false;

    if (Tok.is (tok::kw_VAR)) {
        IsVar = true;
        advance ();
    }

    if (!parseIdentList (Ids) || !consume (tok::colon) || !parseQualident (D))
        return handle_err ();

    // Check Semantics + (Add to Params)
    Actions.actOnFormalParameterDeclaration (Params, Ids, D, IsVar);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Parser (Statements)
/////////////////////////////////////////////////////////////////////////////

/**
 * Parses a sequence of statements in the input stream.
 *
 * This function is responsible for parsing a sequence of statements in the input
 * stream. It calls the `parseStatement` function repeatedly to parse individual
 * statements, and adds them to the provided `StmtList`. The function continues
 * parsing statements until it reaches the end of the sequence or encounters an
 * error.
 *
 * @param Stmts The statement list to add the parsed statements to.
 * @return `true` if the statement sequence was parsed successfully, `false` otherwise.
 */
bool Parser::parseStatementSequence (StmtList& Stmts) {
    auto handle_err = [this] () { return skipUntil (tok::kw_ELSE, tok::kw_END); };
    if (!parseStatement (Stmts))
        return handle_err ();
    while (Tok.is (tok::semi)) {
        advance ();
        if (!parseStatement (Stmts))
            return handle_err ();
    }
    return true;
}

/**
 * Parses a statement in the input stream.
 *
 * This function is responsible for parsing a single statement in the input
 * stream. It handles various types of statements, including assignments,
 * procedure calls, if statements, and while statements. The parsed statement
 * is added to the provided `StmtList`.
 *
 * @param Stmts The statement list to add the parsed statement to.
 * @return `true` if the statement was parsed successfully, `false` otherwise.
 */
bool Parser::parseStatement (StmtList& Stmts) {
    auto handle_err = [this] () {
        return skipUntil (tok::semi, tok::kw_ELSE, tok::kw_END);
    };


    switch (Tok.getKind ()) {

    case tok::identifier: {
        Decl* D;
        Expr *E = nullptr, *Desig = nullptr;
        llvm::SMLoc Loc = Tok.getLocation ();
        if (!parseQualident (D))
            return handle_err ();
        if (!Tok.is (tok::l_paren)) {
            Desig = Actions.actOnDesignator (D);
            if (!parseSelectors (Desig))
                return handle_err ();
            if (!consume (tok::colonequal))
                return handle_err ();
            if (!parseExpression (E))
                return handle_err ();
            Actions.actOnAssignment (Stmts, Loc, Desig, E);
        } else if (Tok.is (tok::l_paren)) {
            ExprList Exprs;
            if (Tok.is (tok::l_paren)) {
                advance ();
                if (Tok.isOneOf (tok::l_paren, tok::plus, tok::minus,
                    tok::kw_NOT, tok::identifier, tok::integer_literal)) {
                    if (!parseExprList (Exprs))
                        return handle_err ();
                }
                if (consume (tok::r_paren))
                    return handle_err ();
            }
            Actions.actOnProcCall (Stmts, Loc, D, Exprs);
        }

        break;
    }

    case tok::kw_IF:
        if (!parseIfStatement (Stmts))
            return handle_err ();
        break;
    case tok::kw_WHILE:
        if (!parseWhileStatement (Stmts))
            return handle_err ();
        break;
    case tok::kw_RETURN:
        if (!parseReturnStatement (Stmts))
            return handle_err ();
        break;
    default: return handle_err ();
    }

    return true;
}

/**
 * Parses an if statement in the input stream.
 *
 * This function is responsible for parsing an if statement, which includes an
 * expression to evaluate, a sequence of statements to execute if the expression
 * is true, and an optional sequence of statements to execute if the expression
 * is false.
 *
 * @param Stmts The statement list to add the parsed if statement to.
 * @return `true` if the if statement was parsed successfully, `false` otherwise.
 */
bool Parser::parseIfStatement (StmtList& Stmts) {
    auto handle_err = [this] () {
        return skipUntil (tok::semi, tok::kw_ELSE, tok::kw_END);
    };
    Expr* E = nullptr;
    StmtList IfStmts, ElseStmts;
    llvm::SMLoc Loc = Tok.getLocation ();

    if (!consume (tok::kw_IF) || !parseExpression (E) || !parseStatementSequence (IfStmts))
        return handle_err ();

    if (Tok.is (tok::kw_ELSE)) {
        advance ();
        if (!parseStatementSequence (ElseStmts))
            return handle_err ();
    }

    if (!expect (tok::kw_END))
        return handle_err ();

    // Check Semantics + (Add to Stmts)
    Actions.actOnIfStatement (Stmts, Loc, E, IfStmts, ElseStmts);
    advance ();
    return false;
}

/**
 * Parses a while statement in the input stream.
 *
 * This function is responsible for parsing a while statement, which includes an
 * expression to evaluate and a sequence of statements to execute as long as the
 * expression is true.
 *
 * @param Stmts The statement list to add the parsed while statement to.
 * @return `true` if the while statement was parsed successfully, `false` otherwise.
 */
bool Parser::parseWhileStatement (StmtList& Stmts) {
    auto handle_err = [this] () {
        return skipUntil (tok::semi, tok::kw_ELSE, tok::kw_END);
    };
    Expr* E = nullptr;
    StmtList WhileStmts;
    llvm::SMLoc Loc = Tok.getLocation ();

    if (!consume (tok::kw_WHILE) || !parseExpression (E) ||
    !consume (tok::kw_DO) || !expect (tok::kw_END))
        return handle_err ();

    // Check Semantics + (Add to Stmts)
    Actions.actOnWhileStatement (Stmts, Loc, E, WhileStmts);
    advance ();
    return true;
}

bool Parser::parseReturnStatement (StmtList& Stmts) {
    auto handle_err = [this] () {
        return skipUntil (tok::semi, tok::kw_ELSE, tok::kw_END);
    };
    Expr* E         = nullptr;
    llvm::SMLoc Loc = Tok.getLocation ();

    if (!consume (tok::kw_RETURN))
        return handle_err ();
    if (Tok.isOneOf (tok::l_paren, tok::plus, tok::minus, tok::kw_NOT,
        tok::identifier, tok::integer_literal))
        if (!parseExpression (E))
            return handle_err ();

    // Check Semantics + (Add to Stmts)
    Actions.actOnReturnStatement (Stmts, Loc, E);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Parser (Expressions)
/////////////////////////////////////////////////////////////////////////////

/**
 * Parses a list of expressions in the input stream.
 *
 * This function is responsible for parsing a list of expressions, which can
 * include multiple expressions separated by commas. Each expression is parsed
 * using the `parseExpression` function and added to the provided `ExprList`.
 *
 * @param Exprs The expression list to add the parsed expressions to.
 * @return `true` if the expression list was parsed successfully, `false` otherwise.
 * @example expression list: `x, y, z`
 */
bool Parser::parseExprList (ExprList& Exprs) {
    auto handle_err = [this] () { return skipUntil (tok::r_paren); };
    Expr* E         = nullptr;
    if (!parseExpression (E))
        return handle_err ();
    if (E)
        Exprs.push_back (E);

    while (Tok.is (tok::comma)) {
        E = nullptr;
        advance ();
        if (!parseExpression (E))
            return handle_err ();
        if (E)
            Exprs.push_back (E);
    }
    return true;
}

/**
 * Parses an expression in the input stream.
 *
 * This function is responsible for parsing an expression, which can be a simple
 * expression or a relation. The parsed expression is then added to the provided
 * `E` parameter.
 *
 * @param E The expression to store the parsed expression in.
 * @return `true` if the expression was parsed successfully, `false` otherwise.
 * @example expression: `x + y`
 */
bool Parser::parseExpression (Expr*& E) {
    auto handle_err = [this] () {
        return skipUntil (tok::r_paren, tok::comma, tok::semi, tok::kw_DO,
        tok::kw_ELSE, tok::kw_END, tok::kw_THEN);
    };

    if (!parseSimpleExpression (E))
        return handle_err ();

    if (Tok.isOneOf (tok::hash, tok::less, tok::lessequal, tok::equal,
        tok::greater, tok::greaterequal)) {
        OperatorInfo Op;
        Expr* Right = nullptr;
        if (!parseRelation (Op))
            return handle_err ();
        if (parseSimpleExpression (Right))
            return handle_err ();
        E = Actions.actOnExpression (E, Right, Op);
    }
    return true;
}

/**
 * Parses a relation operator in the input stream.
 *
 * This function is responsible for parsing a relation operator, which can be
 * one of the following: `=`, `#`, `<`, `<=`, `>`, or `>=`. The parsed operator
 * is then stored in the provided `Op` parameter.
 *
 * @param Op The operator information to store the parsed relation operator in.
 * @return `true` if the relation operator was parsed successfully, `false`
 *         otherwise.
 * @example relation operator: `=`
 */
bool Parser::parseRelation (OperatorInfo& Op) {
    auto handle_err = [this] () {
        return skipUntil (tok::l_paren, tok::plus, tok::minus, tok::kw_NOT,
        tok::identifier, tok::integer_literal);
    };

    if (Tok.isOneOf (tok::equal, tok::hash, tok::less, tok::lessequal,
        tok::greater, tok::greaterequal)) {
        Op = fromTok (Tok);
        advance ();
        return true;
    }
    return handle_err ();
}

/**
 * Parses a simple expression in the input stream.
 *
 * This function is responsible for parsing a simple expression, which can
 * include prefix operators, terms, and additive operators. The parsed
 * expression is then added to the provided `E` parameter.
 *
 * @param E The expression to store the parsed simple expression in.
 * @return `true` if the simple expression was parsed successfully, `false`
 *         otherwise.
 * @example simple expression: `x + y`
 */
bool Parser::parseSimpleExpression (Expr*& E) {
    auto handle_err = [this] () { return skipUntil (tok::identifier); };

    OperatorInfo PrefixOp;
    if (Tok.isOneOf (tok::plus, tok::minus)) {
        PrefixOp = fromTok (Tok);
        advance ();
    }
    if (!parseTerm (E))
        return handle_err ();

    while (Tok.isOneOf (tok::plus, tok::minus, tok::kw_OR)) {
        OperatorInfo Op;
        Expr* Right = nullptr;
        if (!parseAddOperator (Op))
            return handle_err ();
        if (!parseTerm (Right))
            return handle_err ();

        // Check Semantics + (Add to Expr)
        E = Actions.actOnSimpleExpression (E, Right, Op);
    }

    if (!PrefixOp.isUnspecified ())
        E = Actions.actOnPrefixExpression (E, PrefixOp);
    return true;
}


/////////////////////////////////////////////////////////////////////////////
#pragma mark - Parser (Operators)
/////////////////////////////////////////////////////////////////////////////

/**
 * Parses an additive operator in the input stream.
 *
 * This function is responsible for parsing an additive operator, which can
 * include the plus, minus, or logical OR operators. The parsed operator is
 * stored in the provided `Op` parameter.
 *
 * @param Op The operator information to store the parsed operator in.
 * @return `true` if an additive operator was parsed successfully, `false`
 *         otherwise.
 * @example additive operator: `+`
 */
bool Parser::parseAddOperator (OperatorInfo& Op) {
    auto handle_err = [this] () {
        return skipUntil (tok::l_paren, tok::kw_NOT, tok::identifier, tok::integer_literal);
    };
    if (Tok.isOneOf (tok::plus, tok::minus, tok::kw_OR)) {
        Op = fromTok (Tok);
        advance ();
        return true;
    }
    return handle_err ();
}

/**
 * Parses a multiplicative operator in the input stream.
 *
 * This function is responsible for parsing a multiplicative operator, which can
 * include the multiplication, division, or modulo operators. The parsed
 * operator is stored in the provided `Op` parameter.
 *
 * @param Op The operator information to store the parsed operator in.
 * @return `true` if a multiplicative operator was parsed successfully, `false`
 *         otherwise.
 * @example multiplicative operator: `*`
 */
bool Parser::parseMulOperator (OperatorInfo& Op) {
    auto handle_err = [this] () {
        return skipUntil (tok::l_paren, tok::kw_NOT, tok::identifier, tok::integer_literal);
    };
    if (Tok.isOneOf (tok::star, tok::slash, tok::kw_DIV, tok::kw_MOD, tok::kw_AND)) {
        Op = fromTok (Tok);
        advance ();
        return true;
    }
    return handle_err ();
}

/////////////////////////////////////////////////////////////////////////////
#pragma mark - Parser (Terms)
/////////////////////////////////////////////////////////////////////////////

/**
 * Parses a factor in the input stream.
 *
 * This function is responsible for parsing a factor, which can consist of a
 * literal, identifier, or parenthesized expression. The parsed factor is
 * stored in the provided `E` parameter.
 *
 * @param E The expression to store the parsed factor in.
 * @return `true` if a factor was parsed successfully, `false` otherwise.
 * @example factor: `x`, `42`, `(a + b)`
 */
bool Parser::parseFactor (Expr*& E) {
    auto handle_err = [this] () {
        return skipUntil (tok::hash, tok::r_paren, tok::star, tok::plus, tok::comma,
        tok::minus, tok::slash, tok::semi, tok::less, tok::lessequal, tok::equal,
        tok::greater, tok::greaterequal, tok::kw_AND, tok::kw_DIV, tok::kw_DO,
        tok::kw_ELSE, tok::kw_END, tok::kw_MOD, tok::kw_OR, tok::kw_THEN);
    };
    switch (Tok.getKind ()) {

    case tok::identifier: {
        Decl* D;
        ExprList Exprs;
        if (!parseQualident (D))
            return handle_err ();

        if (Tok.is (tok::l_paren)) {
            advance ();
            if (Tok.isOneOf (tok::l_paren, tok::plus, tok::minus, tok::kw_NOT,
                tok::identifier, tok::integer_literal))
                if (!parseExprList (Exprs))
                    return handle_err ();

            if (!expect (tok::r_paren))
                return handle_err ();

            E = Actions.actOnFunctionCall (D, Exprs);
            advance ();
        } else if (Tok.isOneOf (tok::hash, tok::r_paren, tok::star, tok::plus,
                   tok::comma, tok::minus, tok::slash, tok::semi, tok::less,
                   tok::lessequal, tok::equal, tok::greater, tok::greaterequal,
                   tok::kw_AND, tok::kw_DIV, tok::kw_DO, tok::kw_ELSE,
                   tok::kw_END, tok::kw_MOD, tok::kw_OR, tok::kw_THEN)) {
            E = Actions.actOnDesignator (D);
        }
        break;
    }

    case tok::l_paren:
        advance ();
        if (!parseExpression (E))
            return handle_err ();
        if (!consume (tok::r_paren))
            return handle_err ();
        break;

    case tok::kw_NOT: {
        OperatorInfo Op = fromTok (Tok);
        advance ();
        if (!parseFactor (E))
            return handle_err ();
        E = Actions.actOnPrefixExpression (E, Op);
        break;
    }

    default: return handle_err ();
    }

    return true;
}


/**
 * Parses a term in the input stream.
 *
 * This function is responsible for parsing a term, which can consist of a
 * factor followed by zero or more multiplicative operators and factors. The
 * parsed term is stored in the provided `E` parameter.
 *
 * @param E The expression to store the parsed term in.
 * @return `true` if a term was parsed successfully, `false` otherwise.
 * @example term: `x * y`
 */
bool Parser::parseTerm (Expr*& E) {
    auto handle_err = [this] () {
        return skipUntil (tok::hash, tok::r_paren, tok::plus, tok::comma, tok::minus,
        tok::semi, tok::less, tok::lessequal, tok::equal, tok::greater, tok::greaterequal,
        tok::kw_DO, tok::kw_ELSE, tok::kw_END, tok::kw_OR, tok::kw_THEN);
    };

    if (!parseFactor (E))
        return handle_err ();

    while (Tok.isOneOf (tok::star, tok::slash, tok::kw_AND, tok::kw_DIV, tok::kw_MOD)) {
        OperatorInfo Op;
        Expr* Right = nullptr;
        if (!parseMulOperator (Op) || !parseFactor (Right))
            return handle_err ();
        E = Actions.actOnTerm (E, Right, Op); // Creates Infix Expr
    }
    return true;
}

/**
 * Parses a list of identifiers in the input stream.
 *
 * This function is responsible for parsing a list of identifiers, which can
 * include multiple identifiers separated by commas. Each identifier is parsed
 * using the `parseIdentList` function and added to the provided `Ids` list.
 *
 * @param Ids The list of identifiers to add the parsed identifiers to.
 * @return `true` if the identifier list was parsed successfully, `false` otherwise.
 * @example identifier list: `x, y, z`
 */
bool Parser::parseIdentList (IdentList& Ids) {
    auto handle_err = [this] () { return skipUntil (tok::colon, tok::semi); };

    if (!expect (tok::identifier))
        return handle_err ();

    auto Loc = Tok.getLocation ();
    Ids.push_back ({ Loc, Tok.getIdentifier () });
    advance ();
    while (Tok.is (tok::comma)) {
        advance ();
        if (!expect (tok::identifier))
            return handle_err ();
        Loc = Tok.getLocation ();
        Ids.push_back ({ Loc, Tok.getIdentifier () });
        advance ();
    }
    return true;
}

/**
 * Parses a qualified identifier.
 *
 * This function is responsible for parsing a qualified identifier, which can
 * consist of one or more identifiers separated by periods. The parsed
 * identifier is stored in the provided `D` parameter.
 *
 * @param D The declaration to store the parsed qualified identifier in.
 * @return `true` if a qualified identifier was parsed successfully, `false`
 * otherwise.
 * @example qualified identifier: `foo.bar.baz`
 */
bool Parser::parseQualident (Decl*& D) {
    auto handle_err = [this] () {
        return skipUntil (tok::hash, tok::l_paren, tok::r_paren, tok::star,
        tok::plus, tok::comma, tok::minus, tok::slash, tok::colonequal,
        tok::semi, tok::less, tok::lessequal, tok::equal, tok::greater,
        tok::greaterequal, tok::kw_AND, tok::kw_DIV, tok::kw_DO, tok::kw_ELSE,
        tok::kw_END, tok::kw_MOD, tok::kw_OR, tok::kw_THEN);
    };

    D = nullptr;
    if (!expect (tok::identifier))
        return handle_err ();

    // Check Semantics + (Get declaration of qualident)
    D = Actions.actOnQualIdentPart (D, Tok.getLocation (), Tok.getIdentifier ());
    advance ();

    // keep on parsing qualident parts
    while (Tok.is (tok::period) && (llvm::isa<ModuleDecl> (D))) {
        advance ();
        if (!expect (tok::identifier))
            return handle_err ();
        D = Actions.actOnQualIdentPart (D, Tok.getLocation (), Tok.getIdentifier ());
        advance ();
    }
    return true;
}

} // namespace amanlang