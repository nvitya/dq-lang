# Runtime Library

The DQ runtime library provides the low-level services that ordinary programs
use implicitly or through standard modules.

## Implicit `sys`

Most non-RTL modules automatically receive the public interface of `rtl/sys`.
That module reexports:

| Module | Purpose |
| --- | --- |
| `rtl/rtlint` | low-level runtime intrinsics and type helpers |
| `rtl/exception` | `Exception`, `raise`, runtime error support, backtraces |
| `rtl/mem` | raw memory allocation and memory block helpers |
| `rtl/prgargs` | process argument access |

It also provides:

```dq
function Exit(status : int)
```

Use `dq-comp --no-use-sys` only for special low-level code that does not want the
implicit merged runtime symbols.

## Memory Helpers

Raw memory helpers are available through the implicit runtime interface:

```dq
function MemAlloc(allocbytes : int, zeromem : bool = false) -> pointer
function MemFree(memptr : pointer)
function MemFreeRef(memptrvar : ref pointer)
function MemCopy(dest : pointer, src : pointer, size : uint)
function MemSet(dest : pointer, value : int, size : uint)
```

Prefer normal DQ values, dynamic arrays, strings, and `new`/`delete` for ordinary
programming. Use these helpers for C interop, buffers, and low-level runtime
work.

```dq
var p : ^int = ^int(MemAlloc(SizeOf(int), true))
p^ = 123
MemFreeRef(p)  // frees and sets p to null
```

## Program Arguments

The process argument helpers are:

```dq
function ArgCount() -> int
function ArgStr(i : int) -> strview
```

`ArgStr(i)` returns an empty string for an out-of-range index.

## Exceptions And Backtraces

`Exception` carries a message and a captured backtrace.

```dq
try:
    raise Exception("bad input: {}", [name])
except e:
    PrintLn("error: {}", [e.message])
    e.PrintBacktrace()
endtry
```

For source-line backtraces, compile with debug info and without optimization:

```bash
dq-comp -g -O0 app.dq
```

`dq-run app.dq` uses those flags by default when no compiler options are passed.

