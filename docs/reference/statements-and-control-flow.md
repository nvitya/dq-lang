# Statements and Control Flow

This page defines executable statements. See the shorter
[Statements guide](../language/statements.md).

## Blocks

The canonical form begins a block with `:` and closes it with a matching
`end...` keyword such as `endif`, `endwhile`, `endfor`, `endfunc`, or `endtry`.
Supported statements may alternatively use braces. One block must use one
closing style consistently.

## Declaration and Assignment Statements

Variable and constant declarations are statements. `=` assigns; modify-assign
forms such as `+=`, `-=`, and the typed operator variants update an existing
assignable destination. Assignment is not an expression.

A property setter can be the destination of plain or modify assignment. The
getter must also exist for a modify assignment because the current value is
read before the result is written.

## Conditional Statements

`if` and `elif` conditions must be `bool`. At most one branch executes.

```dq
if ready:
    Run()
elif retry:
    TryAgain()
else:
    Stop()
endif
```

## While

`while condition` evaluates a Boolean condition before each iteration.
`continue` starts the next iteration and `break` exits the nearest loop.

## Numeric For Loops

The implemented forms are:

```dq
for i : int = first to last step amount:
endfor

for i : int = first downto last step amount:
endfor

for i : int = first count iterations step amount:
endfor

for i : int = first downcount iterations step amount:
endfor

for i : int = first while condition step amount:
endfor
```

`to` and `downto` use inclusive endpoints. `count` and `downcount` execute the
requested number of iterations. Their explicit step magnitude must be positive;
the direction comes from the loop form. The `while` form applies its step after
the body and permits an expression appropriate to its condition.

The initial value, limit/count, and step are evaluated according to the loop's
entry semantics rather than being arbitrary assignment expressions in the body.

## Return

`return` leaves the current function. A value-returning function may use
`return expression` or assign its built-in `result` variable before falling
through or returning.

## Exceptions

`raise expression` raises an exception object. `try` may have typed `except`
clauses and a `finally` clause.

```dq
try:
    ReadData()
except EFile as error:
    error.PrintMessage()
except Exception:
    raise
finally:
    CloseData()
endtry
```

Exception clauses are checked in source order. A clause matches its declared
object type and derived types. A bare `raise` inside a handler re-raises the
active exception.

`finally` executes whenever control leaves the protected statement, including
normal completion, propagation, `return`, `break`, and `continue`. If code in
`finally` raises another exception, that new exception becomes the active
failure.

During unwinding, initialized owned locals are cleaned up in reverse lifetime
order. A top-level unhandled exception prints its message and captured backtrace
through the hosted runtime before terminating.

## Method-Local Use

An object method may use the restricted local forms `use .` and an available
module namespace to opt selected module-scope values into method lookup. This
does not create a general function-local import facility.
