# Statements

DQ statements are separated by newlines or semicolons. Blocks normally use
`:` plus an `end...` keyword, and may also use braces.

## Variable Declaration

```dq
var x : int = 0
var y : int
var inferred_array : [?]int = [1, 2, 3]
```

Local reference variables use `ref`.

```dq
var value : int = 10
ref alias = value
alias = 20     // value is now 20
```

## Constant Declaration

```dq
const DEFAULT_PORT : int = 8080
```

Constants must be initialized with compile-time constant expressions.

## Assignment

```dq
x = 1
x += 1
arr[i] = x
obj.property = value
```

Assignment is not an expression.

## If

```dq
if condition:
    DoSomething()
elif other_condition:
    DoOther()
else:
    DoFallback()
endif
```

Conditions must be `bool`. Numbers are not compatible with `bool`, comparisons must be used.

## While

```dq
while i < limit:
    i += 1
endwhile
```

`break` leaves the nearest loop. `continue` continues with the next iteration.

## For

DQ numeric `for` loops have several forms.

```dq
for i : int = 0 to 5:
    PrintInt(i)
endfor

for i : int = 5 downto 1:
    PrintInt(i)
endfor

for i : int = 0 count 5:
    PrintInt(i)
endfor

for i : int = 10 downcount 5:
    PrintInt(i)
endfor

for i : int = 0 while i < 10 step 2:
    PrintInt(i)
endfor
```

The loop variable may be declared in the loop header or may refer to an existing
variable.

```dq
var i : int = 0
for i = 0 to 10 step 2:
    // ...
endfor
```

`step` must be positive for `to`, `downto`, `count`, and `downcount` forms. The
`while` form may use a negative step when the condition is written accordingly.

## Return

```dq
return
return value
```

Functions with a return type may either `return value` or assign to the built-in
`result` variable.

```dq
function Square(x : int) -> int:
    result = x * x
endfunc
```

## Break and Continue

```dq
while true:
    if Done():
        break
    endif
    if SkipThis():
        continue
    endif
endwhile
```

`break` and `continue` apply to the nearest enclosing loop.

## Try, Except, Finally

Exceptions are handled with `try`, `except`, and `finally`.

```dq
try:
    MayRaise()
except Exception as e:
    e.PrintMessage()
finally:
    Cleanup()
endtry
```

`finally` runs when control leaves the `try`, including normal completion,
exception propagation, `return`, `break`, and `continue`.

Multiple `except` clauses may be used to handle different exception object
types. An exception can be raised with `raise`.

```dq
raise Exception("message")
```

## Use in Methods

Object methods may use a restricted local `use` statement to bring selected
module-scope names into method lookup.

```dq
function OThing.Method():
    use .
    use helper_module
endfunc
```

This form is only valid in object methods. See [Modules](modules.md).

## Compound Brace Statements

Many block statements can use braces instead of `:` and `end...`.

```dq
if ok
{
    Run()
}
```

The colon form is the canonical form used by most DQ code.
