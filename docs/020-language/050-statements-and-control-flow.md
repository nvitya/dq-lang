# Statements and Control Flow

Statements declare storage, assign values, call functions, and choose what runs
next. Conditions must be Boolean expressions. A block groups statements between
`:` and its matching closer, or between braces where supported.

```dq
use print

func *Main():
    var total : int = 0
    for i : int = 1 to 3:
        total += i
    endfor
    if total == 6:
        PrintLn("total = {}", [total])
    endif
endfunc
```

## Declaration and Assignment

`var`, `const`, `type`, and local `ref` declarations follow the rules in
[Variables, Constants, References, and Scope](030-variables-constants-references-and-scope.md).
Plain assignment uses `=`. Modify-assignment reads the existing value, performs
an operation, and stores the result. Thus its destination must already be
initialized and both readable and writable.

A property can be an assignment destination. Modify-assignment requires both a
getter and setter, because the old value must be read before the update.
Assignment does not produce a value.

## Conditional Statements

```dq
var count : int = 3
if count == 0:
    PrintLn("empty")
elif count < 10:
    PrintLn("small")
else:
    PrintLn("large")
endif
```

Branches are considered in order; at most one body runs. `elif` and `else` are
optional. Numeric values cannot serve directly as conditions.

## While Loops

A `while` loop tests its condition before each iteration. Its body may run zero
times:

```dq
var remaining : int = 3
while remaining > 0:
    PrintLn("{}", [remaining])
    remaining -= 1
endwhile
```

`break` exits the nearest enclosing loop. `continue` starts its next iteration;
a `while` loop then tests its condition again.

## Numeric For Loops

A loop can declare its control variable or use an existing compatible variable.
The principal forms are:

| Form | Meaning |
| --- | --- |
| `for i : int = first to last` | Increase from `first` through inclusive `last`. |
| `for i : int = first downto last` | Decrease through inclusive `last`. |
| `for i : int = first count n` | Run `n` iterations, increasing the control value. |
| `for i : int = first downcount n` | Run `n` iterations, decreasing the value. |
| `for i : int = first while condition` | Continue while the Boolean condition is true. |

Append `step amount` before the colon to change the default step of one.
`to`, `downto`, `count`, and `downcount` require a positive step magnitude;
the loop form supplies the direction. A `while` form can use a negative step.

```dq
for i : int = 0 to 6 step 2:
    PrintLn("{}", [i]) // 0, 2, 4, 6
endfor

for i : int = 5 downcount 3:
    PrintLn("{}", [i]) // 5, 4, 3
endfor

for i : int = 6 while i > 0 step -2:
    PrintLn("{}", [i]) // 6, 4, 2
endfor
```

The initial value is evaluated once on entry. `count` and `downcount` also
evaluate their iteration count once. In contrast, `to` and `downto` reevaluate
their limit at each test, and the `while` form reevaluates its condition.
The step expression is evaluated for each step. `continue` still performs the
`for` loop's normal step before the next test. Avoid changing the
loop's control values from inside the body; use a `while` loop for explicit
state updates.

## For-In Loops

Iterate fixed arrays, slices, and dynamic arrays with `for ... in`:

```dq
var values : [3]int = [10, 20, 30]
for value :? in values:
    PrintLn("{}", [value])
endfor

for value : ref in values:
    value += 1
endfor
```

`:?` infers an ordinary element value. `: ref` binds to each element's storage,
so assignment changes the array. An existing variable or an explicit element
type can also be used. The source must be an addressable array value.

Do not resize a dynamic array while a reference iteration variable is in use;
its element addresses can become invalid. See [Arrays and Slices](080-arrays-and-slices.md).

## Return

`return` exits a function with no result. `return expression` exits a
value-returning function. Alternatively assign its built-in `result` variable
and fall through to `endfunc`. Every normal exit of a value-returning function
must have a valid result; see [Functions](060-functions.md).

`*Main` has no result. For an application's exit status, set the global
`exit_code` or use the runtime's explicit process-exit function.

## Exceptions and Cleanup

`try`, `except`, `finally`, and `raise` transfer control when an operation fails.
`finally` runs on normal exit as well as `return`, `break`, `continue`, and
exception propagation. The full rules and examples are in
[Exceptions and Error Handling](140-exceptions-and-error-handling.md).

## Method-Local Use

Object methods may use `use .` or an already imported module namespace to opt
module-scope values into unqualified lookup. This is a restricted method feature,
not a general import statement for arbitrary function bodies. See
[Modules and Packages](170-modules-and-packages.md#method-local-use).
