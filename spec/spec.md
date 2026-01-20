# DQ Programming Language — Draft Specification

> **Status:** Early draft (design-in-progress)
>
> **Goal:** Define a strongly typed, compiled, multi‑platform systems language with clear syntax, predictable semantics, and explicit control over memory and namespaces. DQ draws inspiration from Pascal, C/C++, Python, and Zig, while intentionally avoiding their most common pitfalls.

---

## 1. Design Goals

* Strong, static typing with minimal implicit conversions
* Readable and explicit syntax suitable for large code bases
* Efficient ahead‑of‑time compilation to native machine code
* Manual memory management (RAII‑style patterns encouraged)
* Suitable for both embedded and desktop systems
* Clear and deterministic name resolution rules
* Object‑oriented programming with single inheritance

---

## 2. Compilation Model

DQ compilation is split into clearly defined phases.

### 2.1 Preprocessing (textual, minimal)

DQ has an explicit and intentionally small **preprocessing phase**. It is used primarily for platform-dependent code selection.

* Block directives:

  * `#{ifdef NAME}`
  * `#{endif}`

```dq
#{ifdef WINDOWS}
use win32;
#{endif}
```

The preprocessor is restricted to well-delimited blocks; it is not a general macro system.

### 2.2 Compile-time evaluation (comp1)

* Constant evaluation
* Compile-time feature checks and configuration

### 2.3 Runtime compilation (comp2)

* Full type checking
* Code generation

No textual macro substitution is performed beyond the explicit preprocessing directives above.

---

## 3. Lexical Rules

* Case‑sensitive identifiers
* C‑style comments only:

  * Line: `// comment`
  * Block: `/* comment */`

---

## 4. Types

### 4.1 Scalar types

* `int`, `uint` (platform-dependent width unless explicitly sized later)
* `bool`
* `char` is a 32-bit Unicode scalar value (equivalent to `int32`)

### 4.2 Floating point

* `float` is **double precision by default**
* Explicit sizes are available:

  * `float32`
  * `float64`

### 4.3 Strings

* The fundamental string type is `str`
* `str` is dynamically allocated and behaves similarly to Python strings
* Encoding/storage is implementation-defined and may use variable-width encoding; the effective storage adapts to the characters stored

### 4.4 User-defined types

* `struct`
* `object` (supports methods and inheritance)

### 4.5 Dynamic arrays

DQ supports dynamically sized arrays as a built-in generic container type.

```dq
values : array<int>;
```

* `array<T>` represents a heap-allocated, dynamically sized sequence of elements of type `T`
* Element type `T` must be a concrete type
* Memory management follows normal ownership and destruction rules

Type inference may be introduced later but is not required in this draft.

---

## 5. Variables and Declarations

* No `var` keyword
* A declaration is identified by `name : type`

```dq
x : int = 10;
name : string8 = "DQ";
```

### Type inference (planned)

DQ plans to support explicit opt-in type inference using the `auto` keyword.

```dq
x : auto = arg1;
```

* `auto` requests the compiler to infer the variable type from the initializer

* Type inference is only allowed when an initializer is present

* The inferred type becomes fixed and strongly typed (no dynamic typing)

* Redeclaration in the same scope is **forbidden**

* Shadowing is **disallowed by default** and must be explicitly enabled

---

## 6. Blocks and Control Flow

DQ supports multiple block styles.

### 6.1 Braces and single-statement blocks

Two block forms are supported:

* **Single-statement block** using `:`
* **Multi-statement block** using `{}`

```dq
if x > 0: y = 1;

while x > 0 {
    x = x - 1;
}
```

### 6.2 Indentation block mode

DQ can switch to a Python-style indentation mode using a compile-time directive:

* `#{comp blockmode indent}`

In this mode, indentation defines blocks (Python-like), intended to ease migration and improve readability where desired.

Supported constructs:

* `if / else`
* `for`
* `while`
* `return`

---

## 7. Expressions and Operators

* Explicit boolean logic
* Boolean operators have **lower precedence** than arithmetic operators
* Supported comparison operators:

  * `==`, `!=`, `<`, `>`, `<=`, `>=`, `=`, `<>`

Assignment is only valid as a standalone statement.

```dq
if a = b AND c <> 0 {
    // valid
}
```

---

## 8. Functions

Functions are declared using the `function` keyword. The return type is specified using the `->` operator.

```dq
function add(a : int, b : int) -> int {
    return a + b;
}
```

* The return type is mandatory unless the function returns no value
* A function without a return value omits the return type

```dq
function log(msg : str) {
    // no return value
}
```

Argument passing modes:

* Value (default)
* `ref`, `in`, `out` (planned)

### Named arguments (planned)

DQ plans to support named arguments to improve clarity and configurability, especially for initialization-style calls.

Proposed syntax uses the `=>` operator:

```dq
uart.Init(40000 => baudrate, 2 => port);
```

* Named arguments may be mixed with positional arguments (rules to be defined)
* This feature is planned and not required for the initial compiler implementation

---

## 9. Objects

```dq
function write(msg : str)
{
    println(msg);
}

object Worker
{
    worklog : array<int>;

    function write(msg : str)
    {
        .println("object: ", msg);
    }

    function doWork()
    {
        write("working");
        .write("calling global");
    }
}

```

* `this.` is implicit
* Object namespace has higher priority than global namespace

---

## 10. Modules and Namespaces

* Modules define namespaces
* Namespace qualifier: `@`

```dq
@math.PI
```

* Leading dot (`.`) refers to the **global namespace** from object contexts

```dq
x = 2 * .PI;
```

### `use` statement

```dq
use math;
```

Effects:

* Makes module symbols available without qualification
* May be used at module, local, or (planned) object scope

---

## 11. Conditional Compilation

DQ supports conditional compilation via the **preprocessor**.

* `#{ifdef NAME}` ... `#{endif}`

```dq
#{ifdef WINDOWS}
use win32;
#{endif}
```

`@def.*` identifiers may still exist for compile-time feature tests, but platform selection is primarily expressed via `#{ifdef}` blocks.

---

## 12. Memory Management (Draft)

* Explicit allocation and deallocation
* Deterministic destruction
* `ensure {}` blocks for cleanup are supported

```dq
function DoWork()
{
    w : Worker = new("hard");
    ensure { delete w; }
    w.DoWork();
}
```

---

## 13. Roadmap and Deferred Features

The following features are desired but may be staged during implementation:

* **Generics** (planned, implemented later)

The following features are core goals and are expected to be supported:

* **Exceptions**
* **Backtrace support** (debugging and error reporting)

---

---

## 14. Status

This specification is intentionally incomplete.

Its purpose is to:

* Lock in core syntax and semantics
* Serve as a reference for compiler prototyping
* Provide a stable base for further refinement

Further sections will be expanded iteratively.

