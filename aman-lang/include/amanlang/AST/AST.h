#pragma once
#include "amanlang/Lexer/Token.h"
#include "llvm/ADT/APSInt.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/SourceMgr.h"
#include <string>
#include <vector>

/**
 * The AST (Abstract Syntax Tree) namespace contains classes and types that represent the
 * abstract syntax tree of an Aman programming language program.
 *
 * The main classes are:
 * - `Decl`: Represents a declaration in the program, such as a module, constant, type, variable, parameter, or procedure.
 * - `Expr`: Represents an expression in the program.
 * - `Stmt`: Represents a statement in the program.
 * - `FormalParameterDecl`: Represents a formal parameter declaration.
 *
 * The namespace also defines several type aliases for collections of these AST elements, such as `DeclList`, `FormalParamList`, `ExprList`, and `StmtList`.
 *
 * The `Ident` struct represents an identifier, containing its location and name.
 */
namespace amanlang {
class Decl;
class Expr;
class Stmt;
class FormalParameterDecl;

using DeclList        = std::vector<Decl*>;
using FormalParamList = std::vector<FormalParameterDecl*>;
using ExprList        = std::vector<Expr*>;
using StmtList        = std::vector<Stmt*>;


/**
 * Represents an identifier, containing its location and name.
 */
typedef struct {
    llvm::SMLoc& Loc;
    llvm::StringRef Name;
} Ident;

typedef std::vector<Ident> IdentList;

////////////////////////////////////////////////////////////
#pragma mark - Declarations
////////////////////////////////////////////////////////////

/**
 * Represents a declaration in the Aman programming language, such as a module,
 * constant, type, variable, parameter, or procedure.
 *
 * The `Decl` class has a `DeclKind` enum to indicate the kind of declaration,
 * and stores the enclosing declaration, location, and name of the declaration.
 * It provides getter methods to access these properties.
 */
class Decl {

    public:
    enum DeclKind { DK_Module, DK_Const, DK_Type, DK_Var, DK_Param, DK_Proc };

    Decl (DeclKind Kind, Decl* EnclosingDecl, llvm::SMLoc& Loc, llvm::StringRef Name)
    : Kind (Kind), EnclosingDecl (EnclosingDecl), Loc (Loc), Name (Name) {
    }

    // getters and setters
    DeclKind getKind () const {
        return Kind;
    }
    llvm::SMLoc getLocation () {
        return Loc;
    }
    llvm::StringRef getName () {
        return Name;
    }
    Decl* getEnclosingDecl () {
        return EnclosingDecl;
    }

    private:
    const DeclKind Kind;

    protected:
    Decl* EnclosingDecl;
    llvm::SMLoc& Loc;
    llvm::StringRef Name;
};

/**
 * Represents a module declaration in the Aman programming language.
 *
 * A module declaration contains a list of declarations and a list of statements.
 * The `ModuleDecl` class provides methods to access and modify these lists.
 */
class ModuleDecl : public Decl {
    public:
    ModuleDecl (Decl* EnclosingDecl, llvm::SMLoc Loc, llvm::StringRef Name)
    : Decl (DK_Module, EnclosingDecl, Loc, Name) {
    }

    ModuleDecl (Decl* EnclosingDecl, llvm::SMLoc Loc, llvm::StringRef Name, DeclList& Decls, StmtList& Stmts)
    : Decl (DK_Module, EnclosingDecl, Loc, Name), Decls (Decls), Stmts (Stmts) {
    }

    const DeclList& getDecls () {
        return Decls;
    }
    void setDecls (DeclList& D) {
        Decls = D;
    }
    const StmtList& getStmts () {
        return Stmts;
    }
    void setStmts (StmtList& L) {
        Stmts = L;
    }

    static bool classof (const Decl* D) {
        return D->getKind () == DK_Module;
    }

    private:
    DeclList Decls;
    StmtList Stmts;
};

/**
 * Represents a constant declaration in the Aman programming language.
 *
 * The `ConstantDecl` class represents a constant declaration, which binds a name
 * to a constant expression. The `getExpr()` method can be used to retrieve the expression that defines the constant value.
 */
class ConstantDecl : public Decl {
    public:
    Expr* getExpr () {
        return E;
    }

    static bool classof (const Decl* D) {
        return D->getKind () == DK_Const;
    }

    ConstantDecl (Decl* EnclosingDecl, llvm::SMLoc Loc, llvm::StringRef Name, Expr* E)
    : Decl (DK_Const, EnclosingDecl, Loc, Name), E (E) {
    }

    private:
    Expr* E;
};

/**
 * Represents a type declaration in the Aman programming language.
 *
 * The `TypeDeclaration` class represents a type declaration, which binds a name
 * to a type. This class is used to define new types in the Aman language.
 */
class TypeDecl : public Decl {
    public:
    TypeDecl (Decl* EnclosingDecl, llvm::SMLoc Loc, llvm::StringRef Name)
    : Decl (DK_Type, EnclosingDecl, Loc, Name) {
    }

    static bool classof (const Decl* D) {
        return D->getKind () == DK_Type;
    }
};

/**
 * Represents a variable declaration in the Aman programming language.
 *
 * The `VariableDeclaration` class represents a variable declaration, which binds a name
 * to a type. This class is used to declare variables in the Aman language.
 */
class VariableDecl : public Decl {
    public:
    VariableDecl (Decl* EnclosingDecl, llvm::SMLoc Loc, llvm::StringRef Name, TypeDecl* Ty)
    : Decl (DK_Var, EnclosingDecl, Loc, Name), Ty (Ty) {
    }

    TypeDecl* getType () {
        return Ty;
    }

    static bool classof (const Decl* D) {
        return D->getKind () == DK_Var;
    }

    private:
    TypeDecl* Ty;
};


/**
 * Represents a formal parameter declaration in the Aman programming language.
 *
 * The `FormalParameterDecl` class represents a formal parameter
 * declaration, which binds a name to a type and indicates whether the parameter
 * is a variable parameter. This class is used to declare formal parameters in
 * function declarations in the Aman language.
 * @example NonVar => `procedure foo (x: integer) { ... }`
 * @example Var => `procedure foo (var x: integer) { ... }`
 * Difference  between Var and NonVar: Var allows the procedure to modify the value of the parameter.
 */
class FormalParameterDecl : public Decl {
    public:
    FormalParameterDecl (Decl* EnclosingDecl, llvm::SMLoc Loc, llvm::StringRef Name, TypeDecl* Ty, bool IsVar)
    : Decl (DK_Param, EnclosingDecl, Loc, Name), Ty (Ty), IsVar (IsVar) {
    }

    TypeDecl* getType () {
        return Ty;
    }
    bool isVar () {
        return IsVar;
    }

    static bool classof (const Decl* D) {
        return D->getKind () == DK_Param;
    }

    private:
    TypeDecl* Ty;
    bool IsVar;
};

/**
 * Represents a procedure declaration in the Aman programming language.
 *
 * The `ProcedureDecl` class represents a procedure declaration, which includes
 * a name, a list of formal parameters, a return type, a list of local
 * declarations, and a list of statements that define the procedure's
 * implementation. This class is used to declare procedures in the Aman
 * language.
 */
class ProcedureDecl : public Decl {
    public:
    ProcedureDecl (Decl* EnclosingDecl, llvm::SMLoc Loc, llvm::StringRef Name)
    : Decl (DK_Proc, EnclosingDecl, Loc, Name) {
    }

    ProcedureDecl (Decl* EnclosingDecl,
    llvm::SMLoc Loc,
    llvm::StringRef Name,
    FormalParamList& Params,
    TypeDecl* RetType,
    DeclList& Decls,
    StmtList& Stmts)
    : Decl (DK_Proc, EnclosingDecl, Loc, Name), Params (Params),
      RetType (RetType), Decls (Decls), Stmts (Stmts) {
    }

    const FormalParamList& getFormalParams () {
        return Params;
    }
    void setFormalParams (FormalParamList& FP) {
        Params = FP;
    }
    TypeDecl* getRetType () {
        return RetType;
    }
    void setRetType (TypeDecl* Ty) {
        RetType = Ty;
    }

    const DeclList& getDecls () {
        return Decls;
    }
    void setDecls (DeclList& D) {
        Decls = D;
    }
    const StmtList& getStmts () {
        return Stmts;
    }
    void setStmts (StmtList& L) {
        Stmts = L;
    }

    static bool classof (const Decl* D) {
        return D->getKind () == DK_Proc;
    }

    private:
    FormalParamList Params;
    TypeDecl* RetType;
    DeclList Decls;
    StmtList Stmts;
};

/// Represents information about an operator, including its location, kind, and whether it is unspecified.
///
/// The `OperatorInfo` class provides a way to store and access information about
/// an operator in the abstract syntax tree (AST) of a programming language. It
/// includes the location of the operator in the source code, the kind of operator
/// (e.g. `+`, `-`, `*`, `/`), and a flag indicating whether the operator is unspecified (e.g. an overloaded operator).
///
/// This class is likely used throughout the AST implementation to represent operators and their associated metadata.
class OperatorInfo {

    public:
    OperatorInfo () : Loc (), Kind (tok::unknown), IsUnspecified (true) {
    }
    OperatorInfo (llvm::SMLoc Loc, tok::TokenKind Kind, bool IsUnspecified = false)
    : Loc (Loc), Kind (Kind), IsUnspecified (IsUnspecified) {
    }

    llvm::SMLoc getLocation () const {
        return Loc;
    }
    tok::TokenKind getKind () const {
        return static_cast<tok::TokenKind> (Kind);
    }
    bool isUnspecified () const {
        return IsUnspecified;
    }

    private:
    llvm::SMLoc Loc;
    uint16_t Kind;
    bool IsUnspecified;
};

////////////////////////////////////////////////////////////
#pragma mark - Expressions
////////////////////////////////////////////////////////////

/// The `Expr` class represents an expression in the abstract syntax tree (AST)
/// of the programming language. It serves as the base class for various types
/// of expressions, such as infix expressions, prefix expressions, integer
/// literals, boolean literals, variable references, constant references, and
/// function calls.
///
/// The `Expr` class has the following properties:
/// - `Kind`: The kind of expression, represented by the `ExprKind` enum.
/// - `Ty`: The type declaration of the expression.
/// - `IsConstant`: A flag indicating whether the expression is a constant.
///
/// The `Expr` class provides methods to get and set the type of the expression,
/// as well as a method to check whether the expression is a constant.
class Expr {
    public:
    enum ExprKind {
        EK_Infix,
        EK_Prefix,
        EK_Int,
        EK_Bool,
        EK_Var,
        EK_Const,
        EK_Func,
    };

    private:
    const ExprKind Kind;
    TypeDecl* Ty;
    bool IsConstant;

    protected:
    Expr (ExprKind Kind, TypeDecl* Ty, bool IsConst)
    : Kind (Kind), Ty (Ty), IsConstant (IsConst) {
    }

    public:
    ExprKind getKind () const {
        return Kind;
    }
    TypeDecl* getType () {
        return Ty;
    }
    void setType (TypeDecl* T) {
        Ty = T;
    }
    bool isConst () {
        return IsConstant;
    }
};

class InfixExpression : public Expr {
    public:
    InfixExpression (Expr* Left, Expr* Right, OperatorInfo Op, TypeDecl* Ty, bool IsConst)
    : Expr (EK_Infix, Ty, IsConst), Left (Left), Right (Right), Op (Op) {
    }

    Expr* getLeft () {
        return Left;
    }
    Expr* getRight () {
        return Right;
    }
    const OperatorInfo& getOperatorInfo () {
        return Op;
    }

    static bool classof (const Expr* E) {
        return E->getKind () == EK_Infix;
    }

    private:
    Expr* Left;
    Expr* Right;
    const OperatorInfo Op;
};

/**
 * Represents a prefix expression in the abstract syntax tree (AST).
 * A prefix expression consists of an operator and an operand expression.
 */
class PrefixExpression : public Expr {
    public:
    PrefixExpression (Expr* E, OperatorInfo Op, TypeDecl* Ty, bool IsConst)
    : Expr (EK_Prefix, Ty, IsConst), E (E), Op (Op) {
    }

    Expr* getExpr () {
        return E;
    }
    const OperatorInfo& getOperatorInfo () {
        return Op;
    }

    static bool classof (const Expr* E) {
        return E->getKind () == EK_Prefix;
    };

    private:
    Expr* E;
    const OperatorInfo Op;
};

/**
 * Represents an integer literal expression in the abstract syntax tree (AST).
 * An integer literal holds a signed integer value and its location in the source code.
 */
class IntegerLiteral : public Expr {
    public:
    IntegerLiteral (llvm::SMLoc Loc, const llvm::APSInt& Value, TypeDecl* Ty)
    : Expr (EK_Int, Ty, true), Loc (Loc), Value (Value) {
    }
    llvm::APSInt& getValue () {
        return Value;
    }

    static bool classof (const Expr* E) {
        return E->getKind () == EK_Int;
    }

    private:
    llvm::SMLoc Loc;
    llvm::APSInt Value;
};

/**
 * Represents a boolean literal expression in the abstract syntax tree (AST).
 * A boolean literal holds a boolean value and its location in the source code.
 */
class BooleanLiteral : public Expr {
    public:
    BooleanLiteral (bool Value, TypeDecl* Ty)
    : Expr (EK_Bool, Ty, true), Value (Value) {
    }
    bool getValue () {
        return Value;
    }

    static bool classof (const Expr* E) {
        return E->getKind () == EK_Bool;
    }

    private:
    bool Value;
};

/**
 * Represents a variable access expression in the abstract syntax tree (AST).
 * A variable access expression refers to a variable declaration, which can be either a
 * VariableDecl or a FormalParameterDecl. The expression holds a reference to the
 * variable declaration and its type.
 *
 * FormalParameterDecl Example: `procedure foo (x: integer) { x }` where `x` is a FormalParameterDecl.
 * VariableDecl Example: `var x: integer = 42` where `x` is a VariableDecl.
 */
class VariableAccess : public Expr {
    public:
    VariableAccess (VariableDecl* Var)
    : Expr (EK_Var, Var->getType (), false), Var (Var) {
    }
    VariableAccess (FormalParameterDecl* Param)
    : Expr (EK_Var, Param->getType (), false), Var (Param) {
    }

    Decl* getDecl () {
        return Var;
    }

    static bool classof (const Expr* E) {
        return E->getKind () == EK_Var;
    }

    private:
    Decl* Var;
};

/**
 * Represents a constant access expression in the abstract syntax tree (AST).
 * A constant access expression refers to a constant declaration, which holds a constant value.
 * The expression holds a reference to the constant declaration and its type.
 */
class ConstantAccess : public Expr {
    public:
    ConstantAccess (ConstantDecl* Const)
    : Expr (EK_Const, Const->getExpr ()->getType (), true), Const (Const) {
    }

    ConstantDecl* geDecl () {
        return Const;
    }

    static bool classof (const Expr* E) {
        return E->getKind () == EK_Const;
    }

    private:
    ConstantDecl* Const;
};

/**
 * Represents a function call expression in the abstract syntax tree (AST).
 * A function call expression refers to a procedure declaration, which holds the function's return type and parameters.
 * The expression holds a reference to the procedure declaration and the list of argument expressions passed to the function.
 *
 * Example: `foo (x, y)` where `foo` is a procedure declaration and `x` and `y` are expression parameters.
 */
class FunctionCallExpr : public Expr {
    public:
    FunctionCallExpr (ProcedureDecl* Proc, ExprList Params)
    : Expr (EK_Func, Proc->getRetType (), false), Proc (Proc), Params (Params) {
    }

    ProcedureDecl* geDecl () {
        return Proc;
    }
    const ExprList& getParams () {
        return Params;
    }

    static bool classof (const Expr* E) {
        return E->getKind () == EK_Func;
    }

    private:
    ProcedureDecl* Proc;
    ExprList Params;
};

/**
 * Represents a statement in the abstract syntax tree (AST).
 * Statements can be of various kinds, such as assignment, procedure call, if, while, and return.
 * The statement kind is stored in the `Kind` member variable.
 */
class Stmt {
    public:
    enum StmtKind { SK_Assign, SK_ProcCall, SK_If, SK_While, SK_Return };

    private:
    const StmtKind Kind;

    protected:
    Stmt (StmtKind Kind) : Kind (Kind) {
    }

    public:
    StmtKind getKind () const {
        return Kind;
    }
};

/**
 * Represents an assignment statement in the abstract syntax tree (AST).
 * An assignment statement holds a variable declaration and an expression to be assigned to that variable.
 */
class AssignmentStatement : public Stmt {
    public:
    AssignmentStatement (VariableDecl* Var, Expr* E)
    : Stmt (SK_Assign), Var (Var), E (E) {
    }

    VariableDecl* getVar () {
        return Var;
    }
    Expr* getExpr () {
        return E;
    }

    static bool classof (const Stmt* S) {
        return S->getKind () == SK_Assign;
    }

    private:
    VariableDecl* Var;
    Expr* E;
};

/**
 * Represents a procedure call statement in the abstract syntax tree (AST).
 * A procedure call statement holds a reference to the procedure declaration and
 * a list of expression parameters to be passed to the procedure.
 * *
 * Example: `foo (x, y)` where `foo` is a procedure declaration and `x` and `y` are expression parameters.
 *
 * Difference between FunctionCallExpr and ProcedureCallStatement:
 * - FunctionCallExpr is an expression that returns a value, while ProcedureCallStatement is a statement that does not return a value.
 */
class ProcedureCallStatement : public Stmt {
    public:
    ProcedureCallStatement (ProcedureDecl* Proc, ExprList& Params)
    : Stmt (SK_ProcCall), Proc (Proc), Params (Params) {
    }

    ProcedureDecl* getProc () {
        return Proc;
    }
    const ExprList& getParams () {
        return Params;
    }

    static bool classof (const Stmt* S) {
        return S->getKind () == SK_ProcCall;
    }

    private:
    ProcedureDecl* Proc;
    ExprList Params;
};

/**
 * Represents an if-statement in the abstract syntax tree (AST).
 * An if-statement consists of a condition expression, a list of statements to
 * execute if the condition is true, and an optional list of statements to execute if the condition is false.
 *
 * Example: `if (x > 0) { ... } else { ... }`
 */
class IfStatement : public Stmt {
    public:
    IfStatement (Expr* Cond, StmtList& IfStmts, StmtList& ElseStmts)
    : Stmt (SK_If), Cond (Cond), IfStmts (IfStmts), ElseStmts (ElseStmts) {
    }

    Expr* getCond () {
        return Cond;
    }
    const StmtList& getIfStmts () {
        return IfStmts;
    }
    const StmtList& getElseStmts () {
        return ElseStmts;
    }

    static bool classof (const Stmt* S) {
        return S->getKind () == SK_If;
    }

    private:
    Expr* Cond;
    StmtList IfStmts;
    StmtList ElseStmts;
};

/**
 * Represents a while-loop statement in the abstract syntax tree (AST).
 * A while-loop statement consists of a condition expression and a list of statements to execute while the condition is true.
 *
 * Example: `while (x > 0) { ... }`
 */
class WhileStatement : public Stmt {
    public:
    WhileStatement (Expr* Cond, StmtList& Stmts)
    : Stmt (SK_While), Cond (Cond), Stmts (Stmts) {
    }

    Expr* getCond () {
        return Cond;
    }
    const StmtList& getStmts () {
        return Stmts;
    }

    static bool classof (const Stmt* S) {
        return S->getKind () == SK_While;
    }

    private:
    Expr* Cond;
    StmtList Stmts;
};

/**
 * Represents a return statement in the abstract syntax tree (AST).
 * A return statement holds an expression to be returned from a function.
 *
 * Example: `return x` where `x` is an expression to be returned.
 */
class ReturnStatement : public Stmt {
    public:
    ReturnStatement (Expr* E) : Stmt (SK_Return), E (E) {
    }

    Expr* getExpr () {
        return E;
    }

    static bool classof (const Stmt* S) {
        return S->getKind () == SK_Return;
    }

    private:
    Expr* E;
};

} // namespace amanlang
