# DQ Language Exception Handling and Backtrace Specification

This document outlines the specification and implementation plan for adding Python/Pascal-style exception handling (`try: ... except: ... finally: ... endtry`) and stack backtraces to the DQ language and its compiler.

## 1. Syntax

### 1.1 `try...except...finally` block
The exception handling block uses Python-like colons with a Pascal-like terminating `endtry` keyword.

```dq
try:
  // Code that may raise an exception
except SomeException as e:
  // Handle a specific exception type
except:
  // Catch-all handler (optional)
finally:
  // Always executed, whether an exception was raised or not (optional)
endtry
```

### 1.2 `raise` statement
Used to throw an exception object. A standalone `raise` can be used inside an `except` block to re-raise the currently caught exception.

```dq
raise Exception('sometest')
```

## 2. Standard Library Changes (stdpkg)

- Introduce a base `Exception` class in the standard runtime library (`stdpkg/rtl`).
- The `Exception` class will include fields for the error message and an array of `void*` instruction pointers for the backtrace.
- Methods such as `GetMessage()` and `PrintBacktrace()` will be provided.

## 3. Implementation Plan

The following is the implementation plan for integrating this specification into the DQ compiler and runtime.

### 3.1 Memory Management and Re-raising

Exception objects will be dynamically allocated on the heap and managed using **Reference Counting**. 
- Reference counting safely handles the complexity of exceptions escaping their `except` block or being re-raised.
- When `raise` is called, the exception object is allocated on the heap with a reference count of 1.
- `except Exception as e:` will bind `e` to the exception, keeping it alive for the duration of the block.
- At the end of the `except` block, the runtime decrements the reference count. If it reaches 0, the exception is freed.
- If the exception is re-raised (via a standalone `raise` inside the `except` block), the runtime simply processes the same object without freeing it prematurely.

### 3.2 Parser (`compiler/parser/`)

- The compiler uses a single-pass forward parser without a separate tokenizer.
- Modify `dqc_parser_stmt.cpp` to directly recognize the `try`, `except`, `finally`, `endtry`, and `raise` keywords, and to parse these statements to generate the corresponding AST nodes.

### 3.3 AST (`compiler/ast/`)

- Add `OStmtTry` representing a `try` block, containing:
  - `body`: `OStmtBlock` for the try execution.
  - `except_branches`: A list of `OExceptBranch` objects containing the expected exception type, the bound variable name (if any), and the body.
  - `finally_body`: Optional `OStmtBlock`.
- Add `OStmtRaise` containing the `OExpr*` expression for the exception object to be thrown (or null if re-raising).

### 3.4 Code Generation (`compiler/codegen/`)

- Modify code generation to use the standard **Itanium C++ ABI exception handling** (`_Unwind_RaiseException` / `libunwind`). This integrates perfectly with LLVM's `invoke` and `landingpad` instructions.
- When inside a `try` block, any function calls must be emitted as `invoke` instructions instead of `call`.
- Generate `landingpad` blocks for `OStmtTry` to catch exceptions.
- The `landingpad` will use LLVM's `@llvm.eh.typeid.for` to match the exception type against the `except` branches.
- Implement cleanup blocks for `finally` execution before resuming the unwind via `resume`.

### 3.5 Runtime System (`stdpkg/rtl/`)

- Implement `__dq_raise_exception` which allocates the Itanium exception header, stores the DQ Exception object (managing its reference count), captures the backtrace, and calls `_Unwind_RaiseException`.
- Implement a personality function `__dq_personality_v0` to interface with the unwinder and determine if a `landingpad` catches the exception.
- Implement stack backtrace capturing using `libunwind` (`unw_getcontext`, `unw_init_local`, `unw_step`) inside the exception construction or `raise` runtime logic.
- Stack backtrace symbolization will be implemented by linking against **`libbacktrace`**, which will parse the DWARF debugging information and translate the instruction pointers into source code lines and function names.

## 4. Verification Plan

### Automated Tests
Create tests in `autotest/tests/exceptions/` testing:
- Simple throw/catch (`raise Exception(...)`).
- Catching specific exception types vs catch-all.
- Correct execution of `finally` blocks during exception unwinding and normal execution.
- Function scope unwinding (throwing from a nested function).
- Re-raising an exception inside an `except` block.
- Verification of heap allocation / reference counting (no memory leaks when throwing).
- Test backtrace format output using standard pattern matching for function names and line numbers.
