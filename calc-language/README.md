# Calc Langauge


EBNF:

```
calc : ("with" ident ("," ident)* ":")? expr ;
expr : term (( "+" | "-" ) term)* ;
term : factor (( "*" | "/") factor)* ;
factor : ident | number | "(" expr ")" ;
ident : ([a-zAZ])+ ;
number : ([0-9])+ ;
```


LLVM IR CodeGen:

```llvm
declare i32 @calc_read(ptr)
declare void @calc_write(int)

@a.str = private constant [2 x i8] c"a\00"
@b.str = private constant [2 x i8] c"b\00"
@c.str = private constant [2 x i8] c"c\00"

// int main(int argc, char *argv[])
define i32 @main(i32, ptr) {
    entry:
        %2 = call i32 @calc_read(ptr @a.str)
        %3 = mul nsw i32 3, %2
        call void @calc_write(i32 %3)
        ret i32 0
}
```