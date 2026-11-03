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

A loop repeats a group of statements, called its body. Each repetition is called
an iteration. A `while` loop tests its condition before each iteration: if the
condition is true, it runs the body and then tests the condition again. When the
condition is false, execution continues after the loop. If the condition is false
at the first test, the body does not run at all.

The body of a `while` loop is written between `while ... :` and `endwhile`:

```dq
var remaining : int = 3
while remaining > 0:
    PrintLn("{}", [remaining])
    remaining -= 1
endwhile
```

This loop prints `3`, `2`, and `1`, each on a separate line. Each iteration
decreases `remaining` by one. Once `remaining` reaches zero, the condition
`remaining > 0` is false and the loop ends.

`break` exits the nearest enclosing loop. `continue` starts its next iteration;
a `while` loop then tests its condition again.

```dq
var number : int = 0
while number < 10:
    number += 1
    if number == 2:
        continue // Skip the rest of this iteration.
    endif
    if number == 5:
        break // Leave the loop immediately.
    endif
    PrintLn("{}", [number])
endwhile
```

This loop prints `1`, `3`, and `4`, each on a separate line. `continue` skips
the print when `number` is `2`, and `break` ends the loop when it reaches `5`,
even though `number < 10` is still true. The increment comes before `continue`
so that `number` advances even when the rest of the body is skipped.

## Numeric For Loops

A numeric `for` loop requires a control variable of a signed or unsigned integer
type, such as `int`, `uint`, `int32`, or `uint32`. Floating-point types are not
supported. A loop can declare its control variable or use an existing compatible
variable. The principal forms are:

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

A `for ... in` loop goes through the elements of a fixed array, slice, or dynamic
array in order, from first to last. This is called iterating over the array.
On each iteration, the loop variable represents the current element and the
loop body runs for that element. If the array is empty, the body does not run.

```dq
var values : [3]int = [10, 20, 30]
for value :? in values:
    PrintLn("{}", [value])
endfor

for value : ref in values:
    value += 1
endfor
```

The first loop prints `10`, `20`, and `30`, each on a separate line. The second
loop increases each element by one, leaving `values` as `[11, 21, 31]`.

`:?` infers an ordinary element value. `: ref` binds to each element's storage,
so assignment changes the array. An existing variable or an explicit element
type can also be used. The source must be an addressable array value.

Do not resize a dynamic array while a reference iteration variable is in use;
its element addresses can become invalid. See [Arrays and Slices](080-arrays-and-slices.md).

## Return Values

`return` exits a function immediately, so statements after it are skipped.
In a function with no result, use `return` on its own:

```dq
func PrintIfPositive(value : int):
    if value <= 0:
        return
    endif
    PrintLn("{}", [value])
endfunc
```

`PrintIfPositive(3)` prints `3`. `PrintIfPositive(0)` returns before reaching
`PrintLn`, so it prints nothing.

In a value-returning function, `return expression` sends a value back to the
caller. Alternatively, assign the built-in `result` variable and let execution
reach `endfunc`:

```dq
func Add(left : int, right : int) -> int:
    return left + right
endfunc

func Sum(values : []int) -> int:
    result = 0
    for value :? in values:
        result += value
    endfor
endfunc
```

`Add(2, 3)` returns `5`, and `Sum([10, 20, 30])` returns `60`. In `Sum`,
`result` holds the running total, so no separate accumulator variable is needed.
It starts at zero, which is also the returned value for an empty slice.
Assigning `result` does not exit the function; the loop keeps adding to it
until all elements have been visited, and `endfunc` returns the total.
Every normal exit of a value-returning function must have a valid result;
see [Functions](060-functions.md).

## `*Main()` Exit Status

`*Main()` has no result, so use the global `exit_code` to set the application's
exit status. Its default value is `0`, which means success; a nonzero value
indicates failure. Assigning `exit_code` does not stop execution. Use `return`
to leave `*Main()` early:

```dq
use print

func *Main():
    PrintLn("Operation failed")
    exit_code = 1
    return
endfunc
```

This application exits with status `1`. You can also let execution reach
`endfunc` after setting `exit_code`.

Alternatively, `Exit(n)` terminates the application immediately with status `n`,
even when called from a function other than `*Main()`:

```dq
use print

func *Main():
    PrintLn("Operation failed")
    Exit(2)
endfunc
```

This application exits with status `2`; `Exit` does not return to its caller.

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
