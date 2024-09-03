grammar AmanLang;

compilationUnit
    : 'MODULE' identifier ';' import_* block identifier '.' EOF
    ;

import_
    : ('FROM' identifier)? 'IMPORT' identList ';'
    ;

block
    : declaration* ('BEGIN' statementSequence)? 'END'
    ;

declaration
    : 'CONST' (constantDecl ';')*
    | 'VAR' (variableDecl ';')*
    | procedureDecl ';'
    ;

constantDecl
    : identifier '=' expression
    ;

variableDecl
    : identList ':' qualident
    ;

procedureDecl
    : 'PROCEDURE' identifier formalParameters? ';'
      block identifier
    ;

formalParameters
    : '(' formalParameterList? ')' (':' qualident)?
    ;

formalParameterList
    : formalParameter (';' formalParameter)*
    ;

formalParameter
    : ('VAR')? identList ':' qualident
    ;

statementSequence
    : statement (';' statement)*
    ;

statement
    : qualident ((':=' expression) | ('(' expList? ')')?)?
    | ifStatement
    | whileStatement
    | 'RETURN' expression?
    ;

ifStatement
    : 'IF' expression 'THEN' statementSequence
      ('ELSE' statementSequence)? 'END'
    ;

whileStatement
    : 'WHILE' expression 'DO' statementSequence 'END'
    ;

expList
    : expression (',' expression)*
    ;

expression
    : simpleExpression (relation simpleExpression)?
    ;

relation
    : '=' | '#' | '<' | '<=' | '>' | '>='
    ;

simpleExpression
    : ('+' | '-')? term (addOperator term)*
    ;

addOperator
    : '+' | '-' | 'OR'
    ;

term
    : factor (mulOperator factor)*
    ;

mulOperator
    : '*' | '/' | 'DIV' | 'MOD' | 'AND'
    ;

factor
    : INTEGER_LITERAL
    | '(' expression ')'
    | 'NOT' factor
    | qualident ('(' expList? ')')?
    ;

qualident
    : identifier ('.' identifier)*
    ;

identList
    : identifier (',' identifier)*
    ;

identifier
    : IDENT
    ;

// Lexer Rules
IDENT: [a-zA-Z][a-zA-Z0-9]*;
INTEGER_LITERAL: [0-9]+;
WS: [ \t\r\n]+ -> skip;