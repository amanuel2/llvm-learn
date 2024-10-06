#include "Parser.h"

AST* Parser::parse () {
    AST* Res = parseCalc ();
    expect (Token::eoi);
    return Res;
}

AST* Parser::parseCalc () {
    Expr* E;
    llvm::SmallVector<llvm::StringRef, 8> Vars;
    llvm::StringRef DefFnName;

    if (Tok.is (Token::KW_with)) {
        advance ();
        if (expect (Token::ident))
            goto _err;

        Vars.push_back (Tok.getText ());
        advance ();
        while (Tok.is (Token::comma)) {
            advance ();
            if (expect (Token::ident))
                goto _err;
            Vars.push_back (Tok.getText ());
            advance ();
        }
        if (consume (Token::colon))
            goto _err;
    }

    // CH.8 IR
    // TOK TYPE DEF
    else if (Tok.is (Token::KW_def)) {
        advance ();
        if (!expect (Token::ident))
            goto _err;
        DefFnName = Tok.getText ();
        advance ();
        if (!consume (Token::l_paren))
            goto _err;
        if (!expect (Token::ident))
            goto _err;
        Vars.push_back (Tok.getText ());
        advance ();
        while (Tok.is (Token::comma)) {
            advance ();
            if (!expect (Token::ident))
                goto _err;
            Vars.push_back (Tok.getText ());
            advance ();
        }
        if (!consume (Token::r_paren))
            goto _err;
        if (!consume (Token::equals))
            goto _err;
        E = parseExpr ();
        if (!expect (Token::eoi))
            goto _err;
        if (Vars.empty ())
            return E;
        else
            return new DefDecl (DefFnName, Vars, E);
    }

    // TOK TYPE IDENTIFIER
    else if (Tok.is (Token::ident)) {
        DefFnName = Tok.getText ();
        advance ();
        if (!consume (Token::l_paren))
            goto _err;
        if (expect (Token::number))
            goto _err;
        Vars.push_back (Tok.getText ());
        advance ();
        while (Tok.is (Token::comma)) {
            advance ();
            if (expect (Token::number))
                goto _err;
            Vars.push_back (Tok.getText ());
            advance ();
        }
        if (!consume (Token::r_paren))
            goto _err;
        if (expect (Token::eoi))
            goto _err;
        return new FuncCallFromDef (DefFnName, Vars);
    }

    // END

    E = parseExpr ();
    if (expect (Token::eoi))
        goto _err;
    if (Vars.empty ())
        return E;
    else
        return new With (Vars, E);
_err:
    while (Tok.getKind () != Token::eoi)
        advance ();
    return nullptr;
}

Expr* Parser::parseExpr () {
    Expr* Left = parseTerm ();
    while (Tok.isOneOf (Token::plus, Token::minus)) {
        BinaryOp::Operator Op = Tok.is (Token::plus) ? BinaryOp::Plus : BinaryOp::Minus;
        advance ();
        Expr* Right = parseTerm ();
        Left        = new BinaryOp (Op, Left, Right);
    }
    return Left;
}

Expr* Parser::parseTerm () {
    Expr* Left = parseFactor ();
    while (Tok.isOneOf (Token::star, Token::slash)) {
        BinaryOp::Operator Op = Tok.is (Token::star) ? BinaryOp::Mul : BinaryOp::Div;
        advance ();
        Expr* Right = parseFactor ();
        Left        = new BinaryOp (Op, Left, Right);
    }
    return Left;
}

Expr* Parser::parseFactor () {
    Expr* Res = nullptr;
    switch (Tok.getKind ()) {
    case Token::number:
        Res = new Factor (Factor::Number, Tok.getText ());
        advance ();
        break;
    case Token::ident:
        Res = new Factor (Factor::Ident, Tok.getText ());
        advance ();
        break;
    case Token::l_paren:
        advance ();
        Res = parseExpr ();
        if (!consume (Token::r_paren))
            break;
    case Token::r_paren: advance (); break;
    default:
        if (!Res)
            error ();
        while (!Tok.isOneOf (Token::r_paren, Token::star, Token::plus, Token::minus, Token::slash, Token::eoi))
            advance ();
    }
    return Res;
}