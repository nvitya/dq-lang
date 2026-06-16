# DQ Language Exception Handling and Backtrace Specification

This document specifies DQ exception handling and stack backtraces.  The first
part defines language behavior.  The second part describes the intended first
implementation for the LLVM/Linux runtime.

## 1. Language Semantics

### 1.1 Goals

Exception handling provides structured non-local error handling with:

- Python/Pascal-style `try`, `except`, `finally`, and `raise` syntax.
- Typed exception matching using the DQ object inheritance model.
- Guaranteed cleanup of owned local values during exception unwinding.
- A captured stack backtrace on every raised exception.

Exception handling is part of the normal hosted DQ runtime.  Embedded targets may
disable this feature later with a target/profile option.

### 1.2 Exception Base Type

The standard runtime provides a base object type:

```dq
object Exception:
public
  refcount   : int
  message    : str
  backtrace  : [*]pointer

  function *Create(amsg : strview)
  function *Destroy()

  function GetMessage() -> str
  function PrintBacktrace()
endobj
```

Only object references whose runtime type is `Exception` or derives from
`Exception` are throwable and catchable.  A raised exception object is heap-owned
by the exception runtime and is reference-counted.

The current `Exception` fields are the initial public/runtime shape.  Additional
standard subclasses, such as runtime bounds errors or allocation errors, may be
added later.

### 1.3 Raising Exceptions

The `raise` statement throws an exception:

```dq
raise Exception("invalid value")
raise EInvalidArgument("value out of range")
```

`raise Type(args...)` is shorthand for allocating a heap object and raising it:

```dq
raise Exception("invalid value")
```

is equivalent in ownership and lifetime to:

```dq
raise new Exception("invalid value")
```

The expression form is also valid when the expression has an exception object
reference type:

```dq
var e : Exception = new Exception("invalid value")
raise e
```

For `raise Type(args...)` and `raise new Type(args...)`, the newly allocated
object's initial reference is owned by the exception runtime.  For `raise expr`,
where `expr` is an existing exception reference, the runtime adds its own active
exception reference and does not consume the caller's reference.  If control later
returns to code that still owns `expr`, that code remains responsible for its
normal lifetime.

Raising `null` is a runtime error.  Raising a non-`Exception` object or a
non-object value is a compile-time error.

A standalone `raise` is valid only inside an `except` block.  It re-raises the
currently handled exception object and preserves its original backtrace.

```dq
except Exception as e:
  Log(e.GetMessage())
  raise
```

### 1.4 Backtrace Capture

The backtrace is captured when the exception is first raised, not when the
exception object is constructed.  Re-raising the same exception preserves the
original backtrace.

If debug information is available, `PrintBacktrace()` should print symbolized
function names and source locations when possible.  Without debug information it
must still print the raw instruction addresses.

The exact visual formatting of symbolized backtraces is implementation-defined,
but it should be stable enough for autotests to match function names, source
files, line numbers, or raw addresses by pattern.

### 1.5 Try / Except / Finally Syntax

The exception handling block uses Python-like colons with a Pascal-like
terminating `endtry` keyword:

```dq
try:
  RiskyOperation()
except EFileNotFound as e:
  PrintLn(e.GetMessage())
except Exception as e:
  PrintLn("general exception: ", e.GetMessage())
except:
  PrintLn("unknown exception")
finally:
  Cleanup()
endtry
```

`except` branches and the `finally` branch are optional, but a `try` statement
must contain at least one `except` or `finally` branch.

Valid forms:

```dq
try:
  Work()
except Exception as e:
  Handle(e)
endtry

try:
  Work()
finally:
  Cleanup()
endtry

try:
  Work()
except:
  HandleAny()
finally:
  Cleanup()
endtry
```

The catch-all `except:` branch is optional.  If present, it must be the last
`except` branch before `finally`.

### 1.6 Exception Matching

Typed `except` branches are tested in source order.  A branch matches when the
runtime exception type is the same as, or derives from, the branch type:

```dq
except Exception as e:
```

catches every exception derived from `Exception`.

Because matching is ordered, more specific derived types should be written before
their base type.  The compiler should report unreachable branches when it can
prove that an earlier branch catches all exceptions handled by a later branch.

Examples:

```dq
try:
  OpenFile(name)
except EFileNotFound as e:
  UseDefault()
except EFileError as e:
  ReportFileError(e)
except Exception as e:
  ReportGeneralError(e)
endtry
```

The handler variable after `as` is optional:

```dq
except EFileNotFound:
  UseDefault()
```

The handler variable is a local object reference valid only inside that `except`
block.  Binding the handler variable increments or otherwise pins the exception
object for the duration of the block.

### 1.7 Finally Execution

`finally` is executed exactly once after control enters the corresponding `try`
statement and before control leaves it.

It runs for all exits:

- normal fall-through from the `try` body
- handled exception
- unhandled exception continuing to an outer handler
- `return`
- `break`
- `continue`
- re-raise from an `except` block
- a new exception raised from the `try` or `except` body

If the `finally` block completes normally, the original control flow continues.
For example, a pending exception keeps unwinding, and a pending `return` returns.

If the `finally` block performs a new control transfer, that new transfer
replaces the previous pending transfer.  This follows Python-like semantics:

- `raise` in `finally` replaces any pending exception or return.
- `return` in `finally` replaces any pending exception or return.
- `break` or `continue` in `finally` replaces any pending exception or return
  when the target loop is valid.

This behavior is powerful but can hide errors.  The compiler may later warn when
`finally` contains `return`, `break`, `continue`, or `raise`.

### 1.8 Cleanup During Unwinding

Exception unwinding must preserve normal DQ ownership rules.

When control leaves a scope because of an exception, all owned locals in that
scope are finalized in reverse construction/declaration order, the same as for
normal scope exit.  This includes:

- fixed-storage objects with destructors
- dynamic strings
- dynamic arrays
- `anyvalue`
- future managed local types

This rule applies to every exited block and function scope, not only to scopes
that syntactically contain a `try`.

If a destructor or cleanup operation raises while another exception is already
unwinding, the new exception replaces the old one unless a later design chooses a
different suppressed-exception model.

### 1.9 Top-Level Handling

Hosted programs must install a top-level exception catcher in the startup/runtime
layer, probably in `rtl_linux.dq` for Linux targets.

An uncaught exception should:

1. print a clear uncaught-exception message
2. print the exception message
3. print the captured backtrace
4. terminate with a non-zero exit code

The exact exit code is runtime-defined initially, but should be documented once
implemented.

### 1.10 RuntimeError Migration

The current `RuntimeError(emsg)` handler mechanism remains temporarily.  It will
later be replaced or routed through the exception system.  At that point runtime
checks such as bounds errors, allocation failures, and invalid operations should
raise standard `Exception` subclasses instead of using the old callback-style
runtime error handler.

## 2. Implementation Plan

### 2.1 Parser

The compiler uses a single-pass forward parser without a separate tokenizer.
`compiler/parser/dqc_parser_stmt.cpp` should directly recognize:

- `try`
- `except`
- `finally`
- `endtry`
- `raise`

The parser should build the AST forms described below and enforce syntactic
rules such as:

- at least one `except` or `finally` branch
- catch-all `except:` only as the final `except`
- standalone `raise` only inside an `except` block
- `finally` appears at most once and after all `except` branches

### 2.2 AST

Add `OStmtTry` representing a `try` statement:

- `body`: `OStmtBlock` for the protected body
- `except_branches`: ordered list of `OExceptBranch`
- `finally_body`: optional `OStmtBlock`

Add `OExceptBranch`:

- `exception_type`: optional `OTypeObject*`; absent means catch-all
- `bound_variable`: optional local `OValSym*`
- `body`: `OStmtBlock`

Add `OStmtRaise`:

- `expr`: optional `OExpr*`; absent means re-raise

The AST/type-checking layer should validate that raised expressions and typed
handlers use `Exception` or derived object types.

### 2.3 Runtime Object Lifetime

Exception objects are heap allocated and managed by reference counting.

- `raise Type(args...)` allocates the object on the heap and initializes it with
  `refcount = 1`; that initial reference belongs to the exception runtime.
- `raise new Type(args...)` behaves the same as `raise Type(args...)`.
- `raise expr` increments the exception reference count for the active exception
  and does not consume the caller's existing reference.
- `except Exception as e:` binds a local reference and keeps the object alive for
  the duration of the handler.
- At the end of a handler, the handler reference is released.
- Re-raise keeps the same object and does not capture a new backtrace.

The exact helper API should live with the exception runtime, for example:

- `__dq_exception_addref`
- `__dq_exception_release`
- `__dq_raise_exception`
- `__dq_rethrow_exception`
- `__dq_current_exception`

### 2.4 LLVM Code Generation

The first hosted implementation should use LLVM exception handling with the
Itanium C++ ABI style unwinder on Linux and other compatible targets.

Required pieces:

- a DQ personality function, for example `__dq_personality_v0`
- landing pads for typed handlers and cleanup blocks
- `invoke` for calls that may unwind while cleanups or handlers are active
- `resume` for continuing an unhandled unwind
- generated type metadata for matching `Exception` object runtime types

The implementation must be function/block cleanup aware.  It is not sufficient
to emit `invoke` only for calls textually inside a `try` body.  Any call that can
raise while there are live owned locals or active `finally` blocks must unwind
through generated cleanup paths.

One possible model is a function-wide exception context stack:

- entering a scope with owned locals registers a cleanup action
- entering a `try/finally` registers a finally action
- calls use `invoke` when the active context stack is non-empty
- unwind edges run the active cleanups in reverse order
- handler landing pads branch to matching `except` bodies or resume the unwind

Normal control transfers (`return`, `break`, `continue`) must also run the same
cleanup/finally actions needed to leave their scopes.

### 2.5 Runtime System

The runtime implementation in `stdpkg/rtl` and target startup code should provide:

- `Exception` base object and helper functions
- exception object reference-count helpers
- raise/re-raise helpers
- backtrace capture at first raise
- top-level uncaught exception handling

For Linux hosted targets:

- use the platform unwinder through the Itanium ABI
- use `libunwind` or compatible platform APIs to capture instruction pointers
- use `libbacktrace` or compatible DWARF support for symbolization
- print raw addresses when debug info or symbolization is unavailable

`rtl_linux.dq` should install the top-level exception catcher around user module
initialization and program entry code.

### 2.6 Portability

The initial implementation may be Linux/Itanium-ABI-only.

Windows needs a compatible design later, likely using LLVM's Windows EH support
and the platform unwinder rather than the Itanium ABI.  The language semantics in
this document should remain independent of that backend choice.

Embedded targets may disable exception support.  The compiler should then reject
`try`, `except`, `finally`, and `raise`, or only allow a restricted profile if
one is specified later.

## 3. Verification Plan

Create tests in `autotest/tests/exceptions/`.

Positive tests:

- simple throw/catch with `raise Exception(...)`
- `raise new Exception(...)`
- derived exception caught by base handler
- specific handler chosen before base handler
- catch-all handler
- handler without `as`
- `finally` on normal fall-through
- `finally` on handled exception
- `finally` on unhandled exception
- nested function throw caught by caller
- re-raise preserving the original backtrace
- cleanup/destructor ordering while unwinding
- `return`, `break`, and `continue` through `finally`
- uncaught top-level exception prints message and backtrace
- backtrace with debug info prints function/source information
- backtrace without debug info prints raw addresses

Negative/compiler tests:

- raising a non-exception value
- raising a non-exception object
- typed `except` using a non-exception type
- catch-all handler before a typed handler
- duplicate or provably unreachable handlers
- standalone `raise` outside an `except` block
- `try` without `except` or `finally`
- multiple `finally` branches

Runtime robustness tests:

- exception object reference count is released after handling
- re-raised exception is not freed prematurely
- exceptions escaping multiple stack frames clean up all owned locals
- exception raised inside `finally` replaces the pending exception
