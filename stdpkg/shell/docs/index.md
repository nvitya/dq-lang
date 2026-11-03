# `shell`

`shell` runs child processes, captures their output, supplies standard input,
and reads or updates environment variables.

```dq
use shell
```

The package supports hosted Linux and x86_64 Windows. Linux uses `fork`,
`execvp`, and `poll`; Windows uses Unicode `CreateProcessW`, redirected pipes,
and job objects. Both use `threads` for background execution. Windows stdin is
fed by a separate writer thread so large input cannot block output capture.

## Running Commands

Create an `OShellRunner` and use `Run` for a foreground command:

```dq
use print
use shell

func *Main():
    var runner <- OShellRunner()
#ifdef WINDOWS
    var code : int = runner.Run('echo hello& echo warning >&2& exit /b 7', false)
#else
    var code : int = runner.Run('printf hello; printf warning >&2; exit 7', false)
#endif
    PrintLn('exit: {}', [code])
    PrintLn('stdout: {}', [runner.out])
    PrintLn('stderr: {}', [runner.err])

    // Argument arrays launch an executable directly.
#ifdef WINDOWS
    runner.RunWithInput(['sort'], 'pear\napple\n')
#else
    runner.Run(['printf', '%s', 'literal $HOME; *.dq'])
#endif
    PrintLn(runner.out)

    try:
#ifdef WINDOWS
        runner.Run('exit /b 3')
#else
        runner.Run('exit 3')
#endif
    except EShellResult as e:
        PrintLn('Caught exit {}: {}', [e.code, e.message])
    endtry
endfunc
```

The first command returns `7`, with `hello` in `out` and `warning` in `err`
(including the newlines written by Windows `echo`). The direct-execution example
sorts input on Windows and prints a literal argument on Linux. The last command
demonstrates the default exception behavior for a nonzero exit code.

### Command Strings and Argument Arrays

Each command-starting method has two overloads:

| Argument | Execution |
| --- | --- |
| `acmd : rostr` | run through the platform's command interpreter; shell quoting, expansion, pipes, and redirection apply |
| `args : []rostr` | execute `args[0]` with the remaining elements as individual arguments |

On Linux, command strings run through `/bin/sh -c`. On Windows, they use
`COMSPEC` (falling back to `cmd.exe`) with `/d /s /c`: AutoRun commands are
disabled, and the command's own quoting is preserved. Shell syntax differs:
use `;` and `$NAME` on Linux, versus `&` and `%NAME%` in Windows command strings.
Windows built-ins such as `dir`, and `.bat` or `.cmd` scripts, need a command
string. PowerShell is not selected automatically.

For argument arrays, Linux searches `PATH` for names without a slash. Windows
uses the native `CreateProcessW` executable search and `.exe` suffix rules.
An explicit executable path avoids the search. The child inherits the caller's
working directory and environment; there is no per-command directory or
environment option.

Use argument arrays for values that should be passed literally. A filename with
spaces is one array element and needs no additional quoting. In an argument
array, `|`, `>`, `*`, and `$HOME` are ordinary arguments unless the executable
itself interprets them. Windows arguments are encoded using the usual
[C runtime quoting rules](https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments):
empty values, spaces, embedded quotes, and trailing backslashes are preserved.
Programs with their own command-line parsers can differ; use command strings
for `cmd.exe` syntax. Executable names, arguments, and environment names/values
are converted between DQ's UTF-8 strings and Windows UTF-16 APIs.

## Foreground API

| Method | Behavior |
| --- | --- |
| `Run(acmd : rostr, exitcerr : bool = true) -> int` | execute a shell command and wait |
| `Run(args : []rostr, exitcerr : bool = true) -> int` | execute an argument array and wait |
| `RunWithInput(acmd : rostr, ainput : strslice, exitcerr : bool = true) -> int` | execute a shell command with supplied stdin and wait |
| `RunWithInput(args : []rostr, ainput : strslice, exitcerr : bool = true) -> int` | execute an argument array with supplied stdin and wait |

These calls collect output while the child runs and return its exit code after
monitoring finishes. By default, a nonzero result raises `EShellResult` instead
of returning normally. Pass `false` as `exitcerr` when nonzero exit codes are
expected and inspect the returned value yourself. This only suppresses exit-code
exceptions; process-start errors can still raise `EShellStart`. Calling a runner
while it is busy returns `-1` without replacing the running command's state.

### Captured Results

| Field | Type | Meaning after completion |
| --- | --- | --- |
| `out` | `str` | captured standard output |
| `err` | `str` | captured standard error |
| `output` | `str` | both streams merged in the monitor's pipe-read order |
| `exitcode` | `int` | process exit status; `-1` for Linux signal termination, Windows `Kill()`, or an unavailable result |
| `finished` | `bool` | initially `true`; set to `false` for a new run and `true` when monitoring finishes |

A new run clears all three output strings and sets `exitcode` to `-1`.
Output is captured as bytes without encoding or newline conversion. A Windows
child can produce CRLF or a different text encoding from a Linux child; the
runner does not normalize or decode it. Nothing is printed
automatically: use `Print` or `PrintLn` to display the captured strings.
The fields remain available when a foreground call raises `EShellResult`.

`output` does not guarantee the original write order across stdout and stderr.
The streams use separate pipes, and reads can combine multiple writes or split
them into chunks. The package does not provide a streaming callback or an
output-size limit; it keeps the complete captures in memory. Since the child
writes to pipes, its buffering can differ from an interactive terminal.

## Supplying Standard Input

`Run` and `StartBg` leave stdin inherited from the caller. A command waiting for
input can therefore block. Their `WithInput` variants supply a pipe instead:
the input is copied into runner-owned storage, sent while output is drained,
and the pipe is closed after all input has been written. An empty input string
provides EOF without supplying any bytes.

```dq
use print
use shell

func *Main():
    var runner <- OShellRunner()
    runner.RunWithInput(['sort'], 'pear\napple\n')
    PrintLn('Sorted input:\n{}', [runner.out])

    runner.RunWithInput(['sort'], '')
    PrintLn('Empty input produced {} bytes', [runner.out.length])
endfunc
```

The same input mechanism is available through `StartBgWithInput` for background
commands. The package offers one input string per run, rather than an interactive
stdin-writing API. Supply input to commands that consume it; the API does not
report how many input bytes the child actually accepted.

## Background Commands

| Method | Behavior |
| --- | --- |
| `StartBg(acmd : rostr) -> int` | start a shell command and a monitor thread |
| `StartBg(args : []rostr) -> int` | start an executable and a monitor thread |
| `StartBgWithInput(acmd : rostr, ainput : strslice) -> int` | start a shell command with supplied stdin |
| `StartBgWithInput(args : []rostr, ainput : strslice) -> int` | start an executable with supplied stdin |
| `Finished() -> bool` | poll completion and try to reap the monitor thread |
| `WaitFinishMs(ams : int) -> bool` | poll for completion for up to the requested milliseconds |
| `Kill()` | terminate the process group (Linux) or job (Windows), then wait up to 2000 ms for completion |
| `Join()` | inherited from `OThread`; wait for and reap the monitor thread without a timeout |

The start methods return `0` when the process and monitor thread were started,
or `-1` if the runner is busy or the monitor thread cannot be started. They can
also raise `EShellStart`. A successful start returns neither a PID nor the
child's exit code. Background completion does not raise `EShellResult`; inspect
`exitcode` after waiting.

```dq
use print
use shell

func *Main():
    var runner <- OShellRunner()
#ifdef WINDOWS
    var command : rostr = 'echo finished'
#else
    var command : rostr = 'sleep 0.05; printf finished'
#endif
    if runner.StartBg(command) <> 0:
        PrintLn('Could not start background command')
        Exit(1)
    endif

    // Other work can run here while the command executes.
    if not runner.WaitFinishMs(1000):
        PrintLn('Timed out; stopping the command')
        runner.Kill()
    endif
    runner.Join()
    PrintLn('exit: {}, stdout: {}', [runner.exitcode, runner.out])

    if runner.StartBgWithInput(['sort'], 'pear\napple\n') <> 0:
        Exit(1)
    endif
    runner.Join()
    PrintLn(runner.out)
endfunc
```

`WaitFinishMs` uses a monotonic deadline and sleeps in 10 ms intervals. It returns
`true` when completion is observed and `false` when the deadline is reached.
Zero or negative values make an immediate completion check; a negative value
does **not** mean wait indefinitely. Scheduling and polling can delay the return
beyond the requested duration. A timeout leaves the command running.

### Lifetime and Cleanup

Use one runner for one command at a time. Wait for and join a background run
before reading its output, reusing the runner for a foreground call, or destroying
the runner. Treat the public result fields as read-only, and do not inspect or
change the strings while the monitor thread is writing them. Manage the runner
from one controlling thread.

`OShellRunner` has no destructor that automatically kills or joins a running
command. `Join()` is the simplest way to wait indefinitely; for a bounded wait,
use `WaitFinishMs`, handle a timeout explicitly, and join before releasing the
object.

`Kill()` is forceful termination, with no graceful shutdown phase. On Linux it
sends `SIGKILL` to the process group created for the child; a descendant that
leaves that group can escape the kill. On Windows, the child is created suspended
and assigned to a job before it runs, so `Kill()` terminates the job's process
tree, including descendants. Windows termination uses status `0xFFFFFFFF`,
which the runner maps to `-1`; other Windows exit statuses are returned as their
unsigned 32-bit values.

`Kill()` returns no success status, and its internal wait is bounded; use `Join()`
to finish cleanup. Descendants keeping captured pipe ends open can postpone
completion after the original child exits, because monitoring drains the pipes
before collecting the exit result. Windows process and job handles are retained
until reuse or destruction to avoid racing termination against handle cleanup.

## Errors

| Exception | When it is used |
| --- | --- |
| `EShellStart` | empty command or executable, or failure to set up pipes or create the process; Windows job/handle setup failures also use this exception |
| `EShellResult` | foreground command finishes with a nonzero result and `exitcerr` is `true` |

Both derive from `Exception` and expose its `message` field. `EShellResult` also
has `code : int`, matching the runner's exit result. Its constructors accept
either `(acode : int, amsg : rostr)` or
`(acode : int, afmt : strslice, avalues : []anyvalue)`.

On Linux, failure to execute a program after the fork is reported by the child:
it writes a diagnostic to captured stderr and exits with `127`, producing
`EShellResult` by default in foreground execution. Windows `CreateProcessW`
failure instead raises `EShellStart`, including the native error number. If the
interpreter starts but cannot find a command, its own exit code and diagnostic
are captured on either platform. A failed process launch leaves the runner
available for reuse. The API does not separately report all native read, write,
or wait failures.

## Environment Variables

```dq
func GetEnv(name : rostr, default_value : rostr) -> str ..
func SetEnv(name : rostr, value : rostr) ..
```

`GetEnv` returns a copy of the variable's value, or `default_value` if the
variable is absent. An existing empty value remains empty. `SetEnv` creates or
overwrites a variable in the current process and raises `Exception` if the
native update fails. There is no unset function in this package.
Windows environment names are case-insensitive; Linux names are case-sensitive.

```dq
use print
use shell

func *Main():
    PrintLn('HOME: {}', [GetEnv('HOME', '(not set)')])
    SetEnv('DQ_SHELL_EXAMPLE', 'hello from DQ')
    PrintLn('Current process: {}', [GetEnv('DQ_SHELL_EXAMPLE', '')])

    var runner <- OShellRunner()
#ifdef WINDOWS
    runner.Run('echo %DQ_SHELL_EXAMPLE%')
#else
    runner.Run('printf "%s" "$DQ_SHELL_EXAMPLE"')
#endif
    PrintLn('Child process: {}', [runner.out])
endfunc
```

Environment changes apply to the current process and children started afterward;
they do not change the parent shell or an already-running child. Since the
environment is process-wide, perform setup before launching worker threads or
coordinate concurrent access yourself.

## Running the Examples

The complete programs above are available in `stdpkg/shell/examples/`.
From the repository root:

```bash
build/dq-run stdpkg/shell/examples/capture.dq
build/dq-run stdpkg/shell/examples/input.dq
build/dq-run stdpkg/shell/examples/background.dq
build/dq-run stdpkg/shell/examples/environment.dq
```

With an installed release, use `dq-run` in place of `build/dq-run`. On Windows,
the built tool path is `build\dq-run.exe`. The examples select platform-specific
shell commands with `#ifdef WINDOWS` and use the system's `sort` executable for
stdin demonstrations. Linux command strings also use `printf` and `sleep`.
