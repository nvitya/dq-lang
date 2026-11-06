# Exceptions and Error Handling

An **exception** reports a failure, such as invalid input or an operation that
cannot be completed. It stops the current work and looks for code that handles
the problem. Use `try` and `except` to handle a failure without ending the whole
program.

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
        PrintLn("Value accepted")
    except Exception as error:
        PrintLn("Error: {}", [error.message])
    endtry
    PrintLn("Ready for another value")
endfunc
```

This prints:

```text
Error: expected a positive value, got 0
Ready for another value
```

`raise` reports the problem. Execution skips the remaining statements in the
`try` block and enters the matching `except` handler. `as error` gives the
handler a name for the exception, so it can read the message.

## Choosing How to Report Failure

Use an ordinary result when the caller is expected to make a routine choice:
for example, a search can return `false` when it finds no match. Use an exception
when the current operation cannot continue and the caller needs to handle the
failure or pass it on. An exception can travel through several function calls
without every intermediate function checking and returning an error result.

Place a `try` block around the operation you can recover from. After its handler
finishes, execution continues after `endtry`; it does not retry the failed
statement or resume the rest of the `try` body.

## Raising and Matching Exceptions

`Exception` is supplied by the implicit runtime when exception support is
enabled. Its constructor accepts a message, optionally with formatting
arguments, just like `PrintLn`.

You can define your own exception types to distinguish failures. Put more
specific handlers first, because the first matching handler is used:

```dq
use print

object EInput(Exception):
endobject

func *Main():
    try:
        raise EInput("Please enter a number")
    except EInput as error:
        PrintLn("Input problem: {}", [error.message])
    except Exception as error:
        PrintLn("Other problem: {}", [error.message])
    endtry
endfunc
```

This prints `Input problem: Please enter a number`. A handler for `Exception`
also matches exceptions derived from it, including `EInput`.

If a handler does not need the exception value, omit `as error`:

```dq
except EInput:
    // handle an input problem without reading its message
```

If no handler matches, the exception continues to the caller. A handler can
also report a problem and then pass the same exception on with a bare `raise`:

```dq
try:
    RequirePositive(0)
except Exception as error:
    PrintLn("Could not continue: {}", [error.message])
    raise
endtry
```

Raising it again this way preserves the original context and backtrace. Do not
call `delete` on the handler's exception variable; the exception machinery
manages that object.

## Cleanup with `finally`

A `finally` block performs an action whenever execution leaves a `try` block.
It runs after normal completion or a handled error, and also when an exception
continues to the caller:

```dq
use print

func *Main():
    try:
        PrintLn("Starting work")
        raise Exception("Connection lost")
    except Exception as error:
        PrintLn("Error: {}", [error.message])
    finally:
        PrintLn("Work finished")
    endtry
endfunc
```

This prints `Starting work`, `Error: Connection lost`, and `Work finished`.
You can also use `try` with `finally` alone when you want cleanup without
handling the exception there.

`finally` also runs when `return`, `break`, or `continue` leaves the protected
block. It is useful for an explicit action such as restoring a setting or
releasing an external resource. For objects owned by your code, prefer
[automatic cleanup](130-memory-management-and-ownership.md): local autofree variables,
managed values, and embedded objects already clean themselves up during exception
propagation.

### Restoring State Without Handling the Error

A `finally` block is useful even when cleanup does not involve memory. This
complete program temporarily changes a setting and restores it before a caller
handles the error:

```dq
use print

var quiet : bool = false

func RunQuietly():
    var previous : bool = quiet
    quiet = true
    try:
        raise Exception("work failed")
    finally:
        quiet = previous
        PrintLn("Setting restored")
    endtry
endfunc

func *Main():
    try:
        RunQuietly()
    except Exception as error:
        PrintLn("Error: {}; quiet={}", [error.message, quiet])
    endtry
endfunc
```

Output:

```text
Setting restored
Error: work failed; quiet=false
```

`RunQuietly` has no `except` handler, so the exception continues to `Main`.
Its `finally` block runs first. Saving the previous setting also makes this
work when `quiet` was already `true` before the call.

## Messages and Backtraces

The exception's `.message` describes the problem. A **backtrace** shows the
sequence of calls that led to it. Print both when diagnosing a failure:

```dq
try:
    raise Exception("operation failed")
except Exception as error:
    error.PrintMessage()
    error.PrintBacktrace()
endtry
```

`GetMessage()` also returns the message. The formatted constructor accepts a
`rostr` format string and a `[]anyvalue` argument list.

Compile with `-g -O0` for useful source locations; `dq-run` uses these options
for you. On hosted targets, an unhandled exception normally prints its message
and backtrace, then ends the program with a failure status.

## Cleanup Details and Limitations

Initialized local values are cleaned up in reverse construction order as an
exception leaves their scope. If a constructor fails, already initialized
autofree fields and base state are also cleaned up.

Catching an exception does not undo changes made before the failure. Restore
or discard partially updated data as appropriate for the operation. If a
`finally` block raises another exception, the new exception becomes the active
failure. Calling an external process-exit function does not perform normal
block-exit cleanup.

## Runtime Errors and Target Support

Runtime checks, such as range and encoding checks, can also report failures.
Some targets convert hardware faults into exceptions: Linux runtimes and
Windows x86-64 have fault handling, with recovery verified at `-O0`. Optimized
fault recovery has backend/runtime limitations. Validate addresses before use;
fault handling is not a portable way to make invalid memory access safe.

Exceptions normally default to enabled on hosted targets and disabled on bare
targets. The compiler options `--exceptions` and `--no-exceptions`, or a project
setting, select support. `EXCEPTIONS` is defined when that support is enabled.
WASI does not support DQ exceptions.

Runtime checks still apply when exception handling is disabled, but failures
cannot be recovered with DQ handlers in that configuration. See
[Compiler Directives](190-compiler-directives.md) and
[Runtime and Program Execution](../030-compiler/015-runtime-and-program-execution.md).
