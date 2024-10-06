# Calc-Language JIT

Generate IR from user defined functions:

```sh
$ ./calc

JIT calc > def f(x) = x*2
define i32 @f(i32 %x) {
entry:
  %0 = mul nsw i32 %x, 2
  ret i32 %0
}

JIT calc > f(20)

Attempting to evaluate expression:
define i32 @calc_expr_func() {
entry:
  %calc_expr_res = call i32 @f(i32 20)
  ret i32 %calc_expr_res
}
declare i32 @f(i32)
User defined function evaluated to: 40

JIT calc > def g(x,y) = x*y+100
define i32 @g(i32 %x, i32 %y) {
entry:
  %0 = mul nsw i32 %x, %y
  %1 = add nsw i32 %0, 100
  ret i32 %1
}

JIT calc > g(8,9)
Attempting to evaluate expression:
define i32 @calc_expr_func() {
entry:
  %calc_expr_res = call i32 @g(i32 8, i32 9)
  ret i32 %calc_expr_res
}
declare i32 @g(i32, i32)
User defined function evaluated to: 172
```