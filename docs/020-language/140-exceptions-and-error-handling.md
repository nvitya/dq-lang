# Exceptions and Error Handling

An **exception** reports a failure, such as invalid input or an operation that
cannot be completed. It stops the current work and looks for code that handles
the problem. Use `try` and `except` to handle a failure without ending the whole
program.

Custom exception type names conventionally start with a capital `E`, as in `EInput`.

```dq
use print

object EInput(Exception):
endobject

func RequirePositive(value : int):
    if value <= 0:
        raise EInput("expected a positive value, got {}", [value])
    endif
endfunc

func *Main():
    try:
        RequirePositive(0)
        PrintLn("Value accepted")
    except EInput as error:
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

## Three Ways to Handle File Errors

Consider a function that logs three measurements: temperature, relative humidity,
and pressure. Each version opens `measurements.log` in append mode and makes
three separate writes, one per measurement. Each line includes the same Unix
timestamp, in seconds, obtained from `Time()`.

### No Error Handling

This version calls libc directly and ignores all return values:

```dq
use time

func libc_fopen(filename : ^char, mode : ^char) -> pointer [[external('fopen')]] ..
func libc_fprintf(stream : pointer, fmt : ^char, ...) -> int32 [[external('fprintf')]] ..
func libc_fclose(stream : pointer) -> int32 [[external('fclose')]] ..

func LogMeasurements(temp : float, rh : float, pressure : float):
    var timestamp : float64 = Time()
    var f : pointer = libc_fopen("measurements.log", "a")
    libc_fprintf(f, "%.3f temp=%.2f\n", timestamp, temp)
    libc_fprintf(f, "%.3f rh=%.2f\n", timestamp, rh)
    libc_fprintf(f, "%.3f pressure=%.2f\n", timestamp, pressure)
    libc_fclose(f)
endfunc
```

The code is short, but it cannot tell the caller whether logging succeeded.
If opening fails, `f` is `nil`, and passing it to `libc_fprintf` is invalid and
may crash the program. If a write fails, the function still attempts the
remaining writes and silently loses data.

### Return-Value-Based Error Handling

With the same libc declarations, the function can check each result and return
`false` on failure. `libc_fopen` returns `nil` on failure; `libc_fprintf` returns
a negative value on failure. A successful open does not guarantee that any of
the writes will succeed, so each write needs its own check.

```dq
use time
use print

func libc_fopen(filename : ^char, mode : ^char) -> pointer [[external('fopen')]] ..
func libc_fprintf(stream : pointer, fmt : ^char, ...) -> int32 [[external('fprintf')]] ..
func libc_fclose(stream : pointer) -> int32 [[external('fclose')]] ..

func LogMeasurements(temp : float, rh : float, pressure : float) -> bool:
    var timestamp : float64 = Time()
    var f : pointer = libc_fopen("measurements.log", "a")
    if f == nil:
        return false
    endif
    if libc_fprintf(f, "%.3f temp=%.2f\n", timestamp, temp) < 0:
        libc_fclose(f)
        return false
    endif
    if libc_fprintf(f, "%.3f rh=%.2f\n", timestamp, rh) < 0:
        libc_fclose(f)
        return false
    endif
    if libc_fprintf(f, "%.3f pressure=%.2f\n", timestamp, pressure) < 0:
        libc_fclose(f)
        return false
    endif
    return libc_fclose(f) == 0
endfunc

func *Main():
    if not LogMeasurements(21.5, 48, 1013.2):
        PrintLn("Could not log measurements")
    endif
endfunc
```

Every failure path after opening closes the file before returning. Closing
also needs a check on the success path: libc buffers output, so a failure may
only be reported when `libc_fclose` flushes the remaining data. After a write
has already failed, the function returns `false` regardless of the close result.
The caller must check the function's return value too; intermediate callers
would need to pass that failure on explicitly.

### Exception-Based Error Handling

The standard [`file`](../040-packages/040-file.md) package's `OFile` object
raises `EFileError` when opening or writing fails. The logging function can
therefore express the sequence of operations directly:

```dq
use file
use time
use print

func LogMeasurements(temp : float, rh : float, pressure : float):
    var timestamp : float64 = Time()
    var f <- OFile()
    f.Open("measurements.log", "a")
    f.WriteText(Format("{:.3f} temp={:.2f}\n", [timestamp, temp]))
    f.WriteText(Format("{:.3f} rh={:.2f}\n", [timestamp, rh]))
    f.WriteText(Format("{:.3f} pressure={:.2f}\n", [timestamp, pressure]))
endfunc

func *Main():
    try:
        LogMeasurements(21.5, 48, 1013.2)
    except EFileError as error:
        PrintLn("Could not log measurements: {}", [error.message])
    endtry
endfunc
```

Each `WriteText` checks for failure inside `OFile`. If opening or any write
raises an exception, the remaining operations are skipped and the exception
travels to the caller's handler. There is no error result to check or pass on
in `LogMeasurements`.

The local `f` uses autofree ownership (`<-`), so its destructor closes the file
both on normal return and during exception propagation. Currently, `OFile.Close()`
does not report libc close errors, so this version cannot detect failures
reported only when buffered output is flushed at close time.

None of these approaches makes the three writes atomic: if a later write
fails, earlier lines may already have been written. Handling an error does not
undo those writes.

## Advantages of the Exception-Based Error Handling

The logging examples show several advantages of exceptions:

- **The normal sequence stays short and readable.** Opening the file and writing the
  three measurements appear together, without repeated checks and early
  returns between them. Adding another write does not require another error
  branch in the logging function.
- **Error details travel with the failure.** The boolean result only says that
  logging failed. An exception carries a type and **message**, and its **backtrace**
  helps locate the failure. Callers can distinguish error types without adding
  separate output parameters to every intermediate function.
- **Errors cannot be silently ignored.** With return values, forgetting a check
  can let the program continue working with invalid or incomplete data. An
  exception automatically stops the failed operation, even when the programmer
  has not supplied an error handler. The program normally reports the unhandled
  exception with its message and backtrace, then terminates when no matching error handler
  provided.
- **Cleanup code does not need to be repeated.** The common cleanup block - `except` or `finally` -
  makes the source code more maintainable and readable.

## Raising and Matching Exceptions

`Exception` is supplied by the implicit runtime when exception support is
enabled. Its constructor accepts a message, optionally with formatting
arguments, just like `PrintLn`.

You can define your own exception types to distinguish failures. This example
uses libc's `scanf` to read a number from the console. It raises `Exception`
if no number can be read, or `EInput` if the number is greater than 100:

```dq
use print

func libc_scanf(fmt : ^char, ...) -> int32 [[external('scanf')]] ..

object EInput(Exception):
endobject

func *Main():
    try:
        PrintLn("Enter a number no greater than 100:")
        var value : float64 = 0
        if libc_scanf("%lf", %value) <> 1:
            raise Exception("Could not read a number")
        endif
        if value > 100:
            raise EInput("Number is too big: {}", [value])
        endif
        PrintLn("Number accepted: {}", [value])
    except EInput as error:
        PrintLn("Input problem: {}", [error.message])
    except Exception as error:
        PrintLn("Other problem: {}", [error.message])
    endtry
endfunc
```

For example, entering `42` prints `Number accepted: 42`, entering `150` prints
`Input problem: Number is too big: 150`, and entering `hello` prints
`Other problem: Could not read a number`.

The `%lf` conversion stores a `float64` at the address `%value`. `scanf` returns
1 when it reads that number successfully. It reads a numeric prefix, so input
such as `42abc` also reads as 42; this example does not validate the entire line.

A handler for `Exception` also matches exceptions derived from it, including
`EInput`. Placing the `EInput` handler first lets it handle the too-big case
separately.

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
sequence of calls that led to it. If no handler catches the exception, the
default error handler prints both and ends the program with a failure status.

Save this example as `backtrace.dq`. It calls through several functions and
lets the exception reach the default error handler:

```dq
func CheckTemperature(temp : float):
    if temp > 100:
        raise Exception("Temperature is too high: {}", [temp])
    endif
endfunc

func LogTemperature(temp : float):
    CheckTemperature(temp)
endfunc

func ReadSensor():
    LogTemperature(150)
endfunc

func *Main():
    ReadSensor()
endfunc
```

Run it with `dq-run backtrace.dq`. Example output on Linux, with the
source paths shortened to the filename:

```text
UNCAUGHT EXCEPTION: Exception: Temperature is too high: 150
Backtrace =
  #0 CheckTemperature @ backtrace.dq:3
  #1 LogTemperature @ backtrace.dq:9
  #2 ReadSensor @ backtrace.dq:13
  #3 Main @ backtrace.dq:17
```

Read the backtrace from the failure outward: `CheckTemperature` raised the
exception, called by `LogTemperature`, which was called by `ReadSensor`, which
was called by `Main`. The source locations help you find each step in the code.

To get source code line info at the backtraces the executable must be compiled
with `-g`. Code optimization might "scramble" the source code, so for clean
source code info the optimization might need to be turned off with `-O0`.
The `dq-run` uses `-g -O0` by default to provide best error reporting.
The `dq-comp` however uses `-O1` by default for better performance.

The backtrace info is available on Windows and Linux, but might not be
available on every target.

## Cleanup Details and Limitations

Initialized local values are cleaned up in reverse construction order as an
exception leaves their scope. If a constructor fails, already initialized
autofree fields and base state are also cleaned up.

Catching an exception does not undo changes made before the failure. Restore
or discard partially updated data as appropriate for the operation. If a
`finally` block raises another exception, the new exception becomes the active
failure. Calling an external process-exit function does not perform normal
block-exit cleanup.

## Zero-Cost Exception Handling

DQ uses **zero-cost exception handling** based on unwind tables on supported
hosted targets. The compiler records which parts of a function are protected
and where their cleanup code and exception handlers are located. These tables
are stored in the executable; they do not need to be checked after each call.

Function calls inside a `try` block remain ordinary calls in the generated
machine code. They are not wrapped in an extra error-checking function, and
there is no hidden exception-result check after every call. When no exception
is raised, execution follows the normal sequence of operations.

When an exception is raised, the runtime uses the unwind tables to walk back
through the active function calls, run the required cleanup, and transfer
control to a matching handler. This work happens on the error path.

“Zero-cost” refers to the absence of per-call exception checks during normal
execution. It does not mean that raising an exception is free: finding a
handler, capturing a backtrace, and running cleanup take time. The tables and
handler code also increase executable size, and exception handling can affect
compiler optimizations. Ordinary cleanup, including `finally` blocks, still
runs wherever the program requires it.

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
