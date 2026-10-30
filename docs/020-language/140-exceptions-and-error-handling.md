# Exceptions and Error Handling

An exception reports a failure that normal execution cannot handle locally.
`raise` transfers control to a matching `except` handler; `finally` performs
cleanup whenever execution leaves a protected region.

```dq
use print

func RequirePositive(value : int):
    if value <= 0:
        raise Exception("expected a positive value, got {}", [value])
    endif
endfunc

func *Main():
    try:
        RequirePositive(0)
    except Exception as error:
        PrintLn("error: {}", [error.message])
    finally:
        PrintLn("finished")
    endtry
endfunc
```

This prints an error message followed by `finished`. `Exception` comes from the
implicit runtime interface when exception support is enabled.

## Raise and Match

`raise expression` raises an exception object derived from `Exception`. Handlers
are tested in source order. A handler for a base type also matches a derived
exception, so put specific handlers before general ones.

```dq
object EInput(Exception):
endobject

func Check():
    try:
        raise EInput("bad input")
    except EInput as error:
        PrintLn("input: {}", [error.message])
    except Exception:
        raise
    endtry
endfunc
```

`except Type as name` binds the exception for that handler. A typed clause can
omit the binding when it is not needed. A bare `raise` inside a handler rethrows
the active exception, preserving its context and captured backtrace.
If no handler matches, propagation continues to the caller.

## Finally and Lifetime

`finally` runs when the protected statement is left through normal completion,
an exception, `return`, `break`, or `continue`. If the finalizer raises another
exception, that new exception becomes the active failure.

Initialized local owners, managed values, and fixed objects are cleaned up in
reverse lifetime order during propagation. If object construction fails,
already initialized owned fields and base state receive cleanup. A handler's
exception object is managed by the exception machinery; do not treat its binding
as a separately owned heap allocation to delete.

Use normal scope ownership for resources where possible and `finally` when an
operation needs an explicit action on every exit. Process termination through
an external exit function is not an ordinary block exit and must not be used
as a substitute for structured cleanup.

## Messages and Backtraces

`Exception` has a message and a captured backtrace. Common operations are
`.message`, `GetMessage()`, `PrintMessage()`, and `PrintBacktrace()`.
The formatted constructor accepts a `strslice` format string and `[]anyvalue`
arguments. Its format string can be an unterminated substring view.

```dq
try:
    raise Exception("operation failed")
except Exception as error:
    error.PrintMessage()
    error.PrintBacktrace()
endtry
```

Compile with `-g -O0` for useful source locations. The hosted top-level handler
prints an unhandled exception and its frames, then terminates with failure.
Catching an error does not automatically make an operation's partially modified
state valid; restore or discard that state according to its API contract.

## Runtime Errors and Target Support

Range and encoding checks can raise runtime errors. Hardware-fault conversion is
platform-specific: Linux runtimes and Windows x86-64 support documented fault
handling, but this does not make invalid memory access safe. Recovery from
access faults is verified at `-O0`; optimized fault recovery has backend/runtime
limitations. Do not use it as a portable substitute for validating addresses.

Exceptions normally default to enabled on hosted targets and disabled on bare
targets. `--exceptions`/`--no-exceptions` and the project setting select support;
`EXCEPTIONS` describes the effective source configuration. WASI does not support
DQ exceptions. Runtime checks remain checks when exception handling is disabled,
but cannot be recovered with DQ handlers in that configuration.
See [Compiler Directives](190-compiler-directives.md) and
[Runtime and Program Execution](../030-compiler/015-runtime-and-program-execution.md).
