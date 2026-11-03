# Runtime and Program Execution

The DQ runtime is written in DQ and lives in `stdpkg/rtl`. It supplies the
services needed by generated code: managed storage, runtime checks, module
initialization, and platform startup. The compiler builds the required runtime
modules alongside your program.

## Hosted Startup

On Linux, Windows, and WASI, the selected runtime supplies the native `main`
entry point. Startup records the command-line arguments, initializes imported
modules in dependency order, then calls the DQ `func *Main()` entry point.
Each canonical module is initialized once.

Returning from `*Main` ends a normal hosted run. The implicit `exit_code`
variable starts at zero; assign it to select the process result while allowing
local scope cleanup to finish:

```dq
use print

func *Main():
    if ArgCount() < 2:
        PrintLn("Please supply a name")
        exit_code = 1
        return
    endif
    var name : rostr = ArgStr(1)
    PrintLn("Hello, {}!", [name])
endfunc
```

Run the saved program with `dq-run hello.dq -- Alice`. `ArgCount()` includes the
program name at index zero. `ArgStr(i)` returns a borrowed zero-terminated
pointer, or `nil` when the index is outside the argument list.

`Exit(status)` terminates a hosted process immediately through the C runtime.
Use a normal return when local destructors and `finally` actions need to run.
DQ does not currently perform automatic global/module finalization at shutdown;
release resources explicitly when their shutdown actions matter.

## Implicit Runtime Interface

Ordinary modules automatically receive the public names of `rtl/sys`. These
include memory helpers, string helpers, and, on hosted targets, argument access.
The exception interface is included when exceptions are enabled. The
[`sys` chapter](../040-packages/015-sys.md) lists the user-facing services.

`--no-use-sys` suppresses that implicit import. It is intended for bootstrap and
low-level runtime modules that must control their own dependencies.

## Runtime Features

| Feature | Hosted default | Bare default | Selection |
| --- | --- | --- | --- |
| Exception handling | Enabled, except on WASI | Disabled | `--exceptions` / `--no-exceptions`, or `exceptions` in a project. |
| Dynamic strings | Enabled | Disabled | `--dynstrings` / `--no-dynstrings`, or `dynstrings` in a project. |

The compiler defines `EXCEPTIONS` and `DYNSTRINGS` only when the corresponding
feature is enabled. Use [conditional compilation](../020-language/190-compiler-directives.md#conditional-compilation)
for code that needs both configurations.

With exceptions disabled, `try`, `except`, `finally`, and `raise` are rejected.
Runtime checks remain active and report failures through `RuntimeError` and its
runtime error hook. Hosted defaults print the error and terminate the process.

With dynamic strings disabled, heap-owning `str` operations are rejected.
Literals, `rostr`, `strslice`, and fixed-storage `embstr` remain available.
See [Project Files](050-project-files.md#dynamic-strings) for feature settings.

## Exceptions and Diagnostics

On exception-enabled hosted targets, an uncaught exception reaches the runtime's
top-level reporter, which prints the message and backtrace and exits with failure.
Compile with `-g -O0` to retain useful source locations:

```bash
dq-comp -g -O0 app.dq
dq-run app.dq
```

The second command chooses those flags when no compiler options are supplied.
See [Exceptions and Error Handling](../020-language/140-exceptions-and-error-handling.md)
for handlers and cleanup. Signal/access-violation conversion has platform and
optimization limitations documented in
[Exceptions and Backtraces](../090-appendices/032-exceptions-and-backtraces.md).

## Bare-Metal Startup

Bare targets produce objects by default and do not supply a complete board
startup environment. A board project provides its entry point, reset/startup
code, memory layout, and linker script. Do not assume hosted argument access,
process termination, or an operating-system exception reporter.

On bare targets, the default `Exit` implementation loops indefinitely and
`Errno()` returns `-1`. Runtime integration belongs to the board support code.
See [Targets and Cross-Compilation](070-targets-and-cross-compilation.md) and
[bare-metal project runtimes](050-project-files.md#bare-metal-runtimes).
