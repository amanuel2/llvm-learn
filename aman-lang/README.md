# Aman-Lang


Example:

```Fortran
MODULE Gcd;
PROCEDURE GCD(a, b: INTEGER) : INTEGER;
VAR t: INTEGER;
BEGIN
  IF b = 0 THEN
    RETURN a;
  END;
  WHILE b # 0 DO
    t := a MOD b;
    a := b;
    b := t;
  END;
  RETURN a;
END GCD;
END Gcd.
```

## EBNF

```
compilationUnit  : "MODULE" identifier ";" ( import )* block identifier "." ;
Import : ( "FROM" identifier )? "IMPORT" identList ";" ;
Block  : ( declaration )* ( "BEGIN" statementSequence )? "END" ;
declaration
  : "CONST" ( constantDeclaration ";" )*
  | "VAR" ( variableDeclaration ";" )*
  | procedureDeclaration ";" ;


constantDeclaration : identifier "=" expression ;
variableDeclaration : identList ":" qualident ;
qualident : identifier ( "." identifier )* ;
identList : identifier ( "," identifier)* ; 


procedureDeclaration
  : "PROCEDURE" identifier ( formalParameters )? ";"
    block identifier ;
formalParameters    : "(" ( formalParameterList )? ")" ( ":" qualident )? ;
formalParameterList : formalParameter (";" formalParameter )* ;
formalParameter     : ( "VAR" )? identList ":" qualident ;


statementSequence   : statement ( ";" statement )* ;
statement
  : qualident ( ":=" expression | ( "(" ( expList )? ")" )? )
  | ifStatement | whileStatement | "RETURN" ( expression )? ;

ifStatement
  : "IF" expression "THEN" statementSequence
   ( "ELSE" statementSequence )? "END" ;

whileStatement
  : "WHILE" expression "DO" statementSequence "END" ;



expList
  : expression ( "," expression )* ;
expression
  : simpleExpression ( relation simpleExpression )? ;
relation
  : "=" | "#" | "<" | "<=" | ">" | ">=" ;
simpleExpression
  : ( "+" | "-" )? term ( addOperator term )* ;
addOperator
  : "+" | "-" | "OR" ;
term
  : factor ( mulOperator factor )* ;
mulOperator
  : "*" | "/" | "DIV" | "MOD" | "AND" ;
factor
  : integer_literal | "(" expression ")" | "NOT" factor
  | qualident ( "(" ( expList )? ")" )? ;

```


## Notes:

- More on qualified ids: https://stackoverflow.com/a/7257601/5768335
- Also note in this lang ":=" is what signifies a stmt
