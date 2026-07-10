# Optimization And Debugging

DQ uses LLVM for code generation and optimization.

## Optimization Levels

Use `-O0` through `-O3`:

```bash
dq-comp -O0 app.dq
dq-comp -O2 app.dq
```

The compiler default is currently `-O1`.

For debugging and readable backtraces, use:

```bash
dq-comp -g -O0 app.dq
```

For quick runs, `dq-run app.dq` uses `-g -O0` when no compiler options are
provided.

## LTO

Full LTO can be enabled with:

```bash
dq-comp --lto app.dq
dq-comp --lto=full app.dq
```

Disable it explicitly with:

```bash
dq-comp --lto=off app.dq
```

ThinLTO is not currently supported.

## Debug Info And Backtraces

Runtime exceptions capture a backtrace when they are raised. Uncaught exceptions
and segmentation faults at the runtime boundary print diagnostic information.

Compile with debug info to get function and source line information:

```bash
dq-comp -g -O0 crash.dq
./crash
```

Inside exception handlers, print the captured backtrace:

```dq
try:
    MightFail()
except e:
    PrintLn("Backtrace:")
    e.PrintBacktrace()
endtry
```

Without `-g`, backtraces may still contain raw addresses or symbol names, but
source locations are less useful.

