# DQ Language Specification

**Version:** 0.1.8 (Draft)
**Status:** Work in Progress
**Last Updated:** 2026-01-22

---

## Table of Contents

1. [Overview](#1-overview)
2. [Design Principles](#2-design-principles)
3. [Lexical Structure](#3-lexical-structure)
4. [Types](#4-types)
5. [Variables and Constants](#5-variables-and-constants)
6. [Expressions and Operators](#6-expressions-and-operators)
7. [Statements and Control Flow](#7-statements-and-control-flow)
8. [Functions](#8-functions)
9. [Objects](#9-objects)
10. [Pointers and References](#10-pointers-and-references)
11. [Modules and Namespaces](#11-modules-and-namespaces)
12. [Preprocessor and Compiler Directives](#12-preprocessor-and-compiler-directives)
13. [Memory Management](#13-memory-management)
14. [Standard Library](#14-standard-library)
15. [Explicitly Rejected Features](#15-explicitly-rejected-features)
16. [Open Questions](#16-open-questions)

---

## 1. Overview

DQ is a compiled, strongly-typed programming language designed to combine:
- Pascal-like readability and explicitness
- C++-style object model (stack or heap allocation)
- No implicit type conversions (unlike C/C++)
- Predictable operator precedence (avoiding the classic C traps)
- Embedded-friendly design with manual memory control

### 1.1 Target Use Cases
- Systems programming
- Embedded development
- Desktop applications
- Cross-platform development

### 1.2 Compilation Model
- Ahead-of-time (AOT) compilation
- LLVM backend (planned)
- Future: Embedded profile with reduced runtime overhead (no exceptions, minimal RTL)

---

## 2. Design Principles

### 2.1 Core Philosophy
- **Explicit over implicit**: No hidden conversions, allocations, or behaviors
- **Readability preferred over minimal keystrokes**: Code should be self-documenting
- **Object context has priority**: Inside objects, member names take precedence over imported names
- **Predictable behavior**: Same code produces same results regardless of context

### 2.2 Key Differentiators from Other Languages

| Feature | DQ Approach | Why |
|---------|-------------|-----|
| Type conversions | Always explicit | Avoids `3/2*10 != 3*10/2` bugs |
| Boolean type | Strict, no int↔bool | Prevents accidental truthy/falsy bugs |
| Division | `/` always returns float | Clear semantics, use `IDIV` for integers |
| Operator precedence | Bitwise before comparison | Prevents `a AND b == 0` ambiguity |
| Object allocation | Choice of stack or heap | Same type, different storage |
| Self reference | Not required in objects | Clean method bodies |

---

## 3. Lexical Structure

### 3.1 Case Sensitivity
DQ is **case-sensitive**.

### 3.2 Comments
```dq
// Single-line comment

/* Multi-line
   comment */
```

### 3.3 Keywords

#### Reserved Keywords
```
and         array       auto        break       case        const
continue    delete      else        ensure      except      false
for         function    if          implementation          import
in          interface   module      new         not         null
object      or          out         packed      private     property
public      raise       ref         return      specialize  struct
to          true        try         type        use         var
virtual     while       xor
```

#### Operator Keywords (Case-Sensitive)
- **Uppercase (integer/bitwise)**: `AND`, `OR`, `XOR`, `NOT`, `SHL`, `SHR`, `IDIV`, `IMOD`
- **Lowercase (logical)**: `and`, `or`, `not`

### 3.4 Identifiers
- Must begin with a letter (A-Z, a-z) or underscore
- May contain letters, digits, and underscores
- ASCII only — no Unicode identifiers allowed

### 3.5 Attributes

Attributes are specified using double square brackets `[[ ... ]]` syntax:

```dq
[[attribute_name]]                  // single attribute
[[attr1, attr2]]                    // multiple attributes
[[attribute("value")]]              // attribute with argument
```

Attributes can be placed:
- Before a declaration
- After a function signature (before the body)

See section 8.5 for function-specific attributes.

### 3.6 Literals

#### Numeric Literals
```dq
42          // int
3.14        // float (float64)
0xFF        // hexadecimal int
0b1010      // binary int
1_000_000   // underscores allowed for readability
```

**Note**: No type suffixes on literals (like `10i32`). Types are always clear from context or explicit declarations.

#### String and Character Literals

Both single (`'...'`) and double (`"..."`) quotes are supported interchangeably. The **type is determined by length**:

```dq
// Character literals (exactly one character)
'A'             // char
"A"             // char
'\n'            // char (escape sequence)
"\t"            // char (escape sequence)

// String literals (zero or multiple characters)
"Hello, World!" // str
'Hello, World!' // str
""              // str (empty string)
''              // str (empty string)
"Line 1\nLine 2"// str (with escape sequence)
```

**Rules**:
- **Exactly one character** (including escape sequences) → `char`
- **Zero or multiple characters** → `str`
- Both quote styles are equivalent — use whichever is convenient for the content

**Concatenation**: When using `+`, `char` is implicitly promoted to `str`:
```dq
s : str = 'Hell' + 'o' + " world";  // 'Hell' is str, 'o' is char, " world" is str
```

#### Multi-line Strings

Triple-quoted strings (`"""..."""` or `'''...'''`) allow multi-line content with automatic indentation handling:

```dq
sql := """
    select
      d.MTIME, d.SENSID, d.VALUE
    from
      MDATA d
    where
      (d.MTYPE=1)
    order by
      d.MTIME asc
    """;
```

**Rules**:
- **Auto-dedent**: Common leading whitespace is stripped based on the closing `"""` indentation
- **Newline trimming**: The first newline after opening quotes and the last newline before closing quotes are ignored (when quotes are on their own lines)
- **Escape sequences**: Still processed (`\n`, `\t`, `\\`, etc.)
- **Type**: Always `str`, regardless of content length

**Example with indentation**:
```dq
function example() {
    msg := """
        Line 1
        Line 2
        """;
    // Result: "Line 1\nLine 2" (8 spaces stripped, no trailing newline)
}
```

**Single-line usage** (useful for strings containing both quote types):
```dq
json := """{"key": "value with 'quotes'"}""";
```

#### Boolean Literals
```dq
true
false
```

---

## 4. Types

### 4.1 Primitive Types

#### Integer Types
| Type | Size | Range |
|------|------|-------|
| `int8` | 8-bit | -128 to 127 |
| `byte`, `uint8` | 8-bit | 0 to 255 |
| `int16` | 16-bit | -32,768 to 32,767 |
| `uint16` | 16-bit | 0 to 65,535 |
| `int32` | 32-bit | -2³¹ to 2³¹-1 |
| `uint32` | 32-bit | 0 to 2³²-1 |
| `int64` | 64-bit | -2⁶³ to 2⁶³-1 |
| `uint64` | 64-bit | 0 to 2⁶⁴-1 |
| `int` | pointer-sized | platform dependent |
| `uint` | pointer-sized | platform dependent |

**Note**: `byte` and `uint8` are interchangeable. The type `word` is intentionally not provided due to ambiguity (16-bit vs 32-bit depending on platform conventions).

#### Floating Point Types
| Type | Size | Description |
|------|------|-------------|
| `float32` | 32-bit | IEEE 754 single precision |
| `float64` | 64-bit | IEEE 754 double precision |
| `float` | 64-bit | Alias for `float64` |

#### Boolean Type
```dq
bool    // true or false, no implicit conversion to/from integers
```

#### Character Types
```dq
char    // 32-bit Unicode scalar value (0..0x10FFFF, excluding surrogates)
char16  // 16-bit UTF-16 code unit
char8   // 8-bit code unit / byte
```

**Character from Unicode code point**:
```dq
c : char = char(0x30);      // '0' (digit zero)
c : char = char(0x1F600);   // 😀 (emoji)
```

### 4.2 String Types

#### Dynamic String (`str` / `string`)
- Python-like semantics
- Unicode-aware
- Heap-backed, variable length
- `str` is the primary spelling; `string` is an alias

```dq
s1 : str = "Hello";
s2 : str = s1 + " World";
len : int = length(s1);     // Pascal-style function
len : int = s1.length;      // also available as property
ch : char = s1[0];          // indexed access (Unicode-aware)
```

#### Fixed-Size String Buffer (`cstring[N]`)
- N bytes inline (no heap)
- NUL-terminated
- For protocols, ABI, packed structs

```dq
name : cstring[32];         // 32-byte inline buffer
name = "Viktor";            // copies UTF-8, truncates to N-1, adds NUL
s : str = name;             // converts to dynamic string
```

### 4.3 Array Types

#### Fixed-Size Array (`T[N]`)
- Inline storage
- Compile-time constant length
- Value semantics

```dq
arr : int32[4];
arr[0] = 10;
len : int = arr.length;     // == 4 (compile-time known)
```

#### Dynamic Array (`T[...]`)
- Heap-backed
- Variable length
- Reference semantics (TBD: copy vs reference)

```dq
arr : int32[...];
arr.append(10);
arr.append(20);
len : int = arr.length;     // runtime value
```

#### Multi-dimensional Arrays
```dq
matrix : int[3][4];      // fixed 3x4 matrix
grid : int[...][...];    // fully dynamic 2D
rows : int[...][4];      // dynamic rows, fixed 4 columns per row
```

### 4.4 Pointer Types

```dq
p : ^int32;                 // pointer to int32
p : ^MyStruct;              // pointer to object/struct
ptr : pointer;              // untyped pointer (like void*)
```

### 4.5 Type Aliases

```dq
type MyInt = int32;
type Callback = function(x: int) -> int;
type MethodCallback = function(x: int) -> int of object;
```

### 4.6 Enumeration Types

Enumerations define a set of named constant values with a mandatory underlying storage type.

#### Declaration Syntax

```dq
// Storage type is mandatory
type TProcState : byte = (psIdle, psRising, psFalling, psSaving);

// With explicit values (for protocols, hardware registers, etc.)
type TCmd : byte = (cmdNone = 0, cmdRead = 0x10, cmdWrite = 0x20, cmdErase = 0xFF);

// Auto-increment from last explicit value
type TStatus : int16 = (stNone = 0, stOk, stWarning, stError);  // values: 0, 1, 2, 3
```

#### Scope and Naming

Enum values are placed in the **module scope** (Pascal-style), allowing direct use without qualification:

```dq
state : TProcState = psIdle;

if state = psRising {
    // ...
}
```

**Naming convention**: Use a short prefix derived from the type name to avoid conflicts:
- `TProcState` → `ps` prefix: `psIdle`, `psRising`
- `TCommand` → `cmd` prefix: `cmdRead`, `cmdWrite`
- `TResult` → `res` prefix: `resOk`, `resError`

This convention prevents name collisions when multiple enum types are in scope.

#### Type Conversions

Conversions between enum types and their underlying integer type must be **explicit**:

```dq
cmd : TCmd = cmdRead;

// Enum to integer: explicit cast
val : byte = byte(cmd);             // val = 0x10

// Integer to enum: explicit cast
cmd2 : TCmd = TCmd(0x20);           // cmd2 = cmdWrite

// Comparison with integer literals: requires cast
if byte(cmd) = 0x10 { ... }         // OK: explicit
// if cmd = 0x10 { ... }            // ERROR: type mismatch
```

#### Supported Underlying Types

Any integer type can be used as the underlying storage:
- `byte`, `uint8`, `int8`
- `uint16`, `int16`
- `uint32`, `int32`
- `uint64`, `int64`

```dq
type TSmallEnum : byte = (seA, seB, seC);           // 1 byte
type TLargeFlags : uint32 = (lfNone = 0, lfAll = 0xFFFFFFFF);  // 4 bytes
```

---

## 5. Variables and Constants

### 5.1 Variable Declaration

Types must always be specified explicitly. This signals a declaration.

```dq
var x : int32 = 10;         // explicit type with initialization
var y : int32;              // uninitialized (must be assigned before use)
x : int32 = 10;             // 'var' keyword is optional
```

### 5.2 Type Inference with `auto`

Use the special type `auto` when you want the compiler to infer the type:

```dq
var x : auto = 10;          // inferred as int
var s : auto = "hello";     // inferred as str
x : auto = some_function(); // inferred from return type
```

### 5.3 Constants

Constants are declared with `const(type)` syntax, where the type is mandatory.

#### Single-Line Form

```dq
const(float)  PI = 3.14159265358979;
const(uint32) MAGIC = 0xDEADBEEF;
const(int)    MAX_SIZE = 1024;
```

#### Block Form

Use the block form to group related constants of the same type:

```dq
const(uint) {
    BUFFER_SIZE = 1024;
    BUFFER_MASK = BUFFER_SIZE - 1;
    MAX_ITEMS = 256;
}

const(float) {
    PI = 3.14159265358979;
    E  = 2.71828182845904;
}
```

Constants are evaluated at compile time and can reference other constants in expressions.

---

## 6. Expressions and Operators

### 6.1 Arithmetic Operators

| Operator | Description | Notes |
|----------|-------------|-------|
| `+` | Addition | |
| `-` | Subtraction / Unary minus | |
| `*` | Multiplication | |
| `/` | Division | **Always returns float** |
| `IDIV` | Integer division | Returns integer |
| `IMOD` | Integer modulo | Returns integer |

**Division Rule**: `/` always yields `float`. Integer division requires explicit `IDIV`.

```dq
f : float = 10 / 3;         // == 3.333...
i : int = 10 IDIV 3;        // == 3
i : int = round(10 / 3);    // == 3 (explicit rounding)
```

**Rounding Functions**:
- `round(x)`: Scientific rounding (half away from zero)
- `floor(x)`: Round toward negative infinity
- `ceil(x)`: Round toward positive infinity

### 6.2 Comparison Operators

| Operator | Description |
|----------|-------------|
| `==`, `=` | Equality (both accepted) |
| `!=`, `<>` | Inequality (both accepted) |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

### 6.3 Logical Operators (lowercase, for `bool` only)

| Operator | Description | Short-circuit |
|----------|-------------|---------------|
| `and` | Logical AND | Yes |
| `or` | Logical OR | Yes |
| `not` | Logical NOT | N/A |

### 6.4 Bitwise Operators (UPPERCASE, for integers only)

| Operator | Description |
|----------|-------------|
| `AND` | Bitwise AND |
| `OR` | Bitwise OR |
| `XOR` | Bitwise XOR |
| `NOT` | Bitwise NOT (complement) |
| `SHL`, `<<` | Shift left |
| `SHR`, `>>` | Shift right |

### 6.5 Operator Precedence

The precedence is designed to avoid the classic C trap where `a AND b == 0` parses as `a AND (b == 0)`.

**DQ Precedence (highest to lowest)**:

1. Primary: `()`, `.`, `[]`, function calls
2. Unary arithmetic: `-`, `^` (dereference)
3. Bitwise NOT: `NOT`
4. Multiplicative: `*`, `/`, `IDIV`, `IMOD`
5. Additive: `+`, `-`
6. Shift: `SHL`, `SHR`, `<<`, `>>`
7. Bitwise AND: `AND`
8. Bitwise XOR: `XOR`
9. Bitwise OR: `OR`
10. **Comparison**: `<`, `>`, `<=`, `>=`, `==`, `=`, `!=`, `<>`
11. Logical NOT: `not`
12. Logical AND: `and`
13. Logical OR: `or`

**Key design points**:
- Bitwise operators bind **tighter** than comparisons
- Logical `not` is **lower** than comparisons (enables braceless expressions)

```dq
// This works as expected without parentheses:
if i2 AND i1 <> 0: ...       // parses as (i2 AND i1) <> 0

// This results to same:
if i2 AND i1 <> 0 and i1 << 1 > 5: ...
// as this:
if ((i2 AND i1) <> 0) and ((i1 << 1) > 5): ...
```

### 6.6 Assignment Operators

```dq
x = 10;                     // assignment
x := 10;                    // also assignment (optional Pascal style)
x += 5;                     // compound: add
x -= 5;                     // compound: subtract
x *= 2;                     // compound: multiply
x /= 2;                     // compound: divide (result is float!)
```

**Important**: Assignment is a **statement**, assignments are not allowed in expressions (like after `if`).
An assignment chaining is not possible (e.g. `a := b := 1;` is a compiler error);

### 6.7 Named Arguments

Named arguments use the `:=` assignment operator for clarity:

```dq
connect(port := 80, timeout_ms := 5000);
cfg : TConfig = (baud := 115200, parity := .None);
```

**Note**: The `:=` operator is required for named arguments (not `=`) to avoid confusion with equality comparison.

---

## 7. Statements and Control Flow

### 7.1 Block Modes

DQ supports two block modes (selectable via compiler directive):

#### Brace Mode (default)
```dq
if condition {
    // statements
}
```

#### Indentation Mode (optional)
```dq
#{opt blockmode indent}
if condition:
    // statements
```

### 7.2 If Statement

```dq
if condition {
    // then branch
}
else if other_condition {
    // else-if branch
}
else {
    // else branch
}

// Single-statement form:
if condition: statement;
else: statement;
```

**Important**: Condition must be of type `bool`. No implicit int→bool conversion.

### 7.3 While Loop

```dq
while condition {
    // body
}
```

### 7.4 For Loop

```dq
for i = 0 to 10 {           // inclusive: 0, 1, 2, ..., 10
    // body
}

for i = 10 to 0 {           // descending (TBD: step)
    // body
}

// Iteration over collections:
for ch : char in str_value {
    // iterate over characters
}

for item in array_value {
    // iterate over elements
}
```

### 7.5 Break and Continue

```dq
break;                      // exit innermost loop
continue;                   // skip to next iteration
```

### 7.6 Return Statement

```dq
return value;               // return from function
return;                     // return from void function
```

**note**: function return values also can be set using the built-in `result` variable.


### 7.7 Ensure Statement (Defer)

```dq
ensure { cleanup_code(); }  // executes at scope exit
```

---

## 8. Functions

### 8.1 Function Declaration

```dq
function name(param1 : Type1, param2 : Type2) -> ReturnType {
    // body
}

function void_func(x : int) {
    // no return type means void
}
```

### 8.2 Return Value

```dq
function add(a : int, b : int) -> int {
    return a + b;
}

// Alternative (Pascal-style): assign to 'result'
function add(a : int, b : int) -> int {
    result = a + b;
}
```

### 8.3 Parameter Passing Modes

| Mode | Syntax | Description |
|------|--------|-------------|
| Value | `param : T` | Copy of value (default for small types) |
| In | `in param : T` | Read-only reference (default for large types) |
| Ref | `ref param : T` | Mutable reference |
| Out | `out param : T` | Must be assigned before function returns |

```dq
function Inc(ref x : int, amount : int) {
    x += amount;
}

function ParseInt(in s : str, out value : int) -> bool {
    // ...
}
```

**Rules**:
- `ref`, `in`, `out` parameters cannot escape the function
- They cannot be stored in heap objects or returned
- They cannot be null
- They are always typed (no untyped references)

**Argument Namespace**: Parameters can be accessed explicitly via `@arg.`:

```dq
function Foo(par1 : int) -> int {
    result := @arg.par1 * 2;    // explicit argument access
}
```

This is useful when a local variable shadows a parameter name.

### 8.4 Function Types and Delegates

```dq
// Function pointer type
type IntFunc = function(x : int) -> int;

// Method pointer (delegate)
type Callback = function(x : int) -> int of object;

// Usage
cb : Callback = obj.method;
cb(42);
```

### 8.5 Function Attributes

Attributes modify function behavior and properties. They use the `[[ ... ]]` syntax and can be placed:
- Before the function declaration
- After the function signature (before the opening brace)

Multiple attributes can be specified in a single bracket pair (comma-separated) or in separate brackets.

```dq
// Attribute before declaration
[[section("ramcode")]]
function Foo(x : int) -> int {
    px : ^int = &x;
    @io.println("x = ", px^);
}

// Attributes after signature
function Bar(x : int) -> int [[stdcall, override]] {
    return x * 2;
}

// Static function
[[static]]
function Helper(x : int) -> int {
    return x + 1;
}

// Alternative: attribute after signature
function Helper2(x : int) -> int [[static]] {
    return x + 1;
}

// Inline function
function GetValue() -> int [[inline]] {
    return 42;
}
```

#### Common Function Attributes

| Attribute | Description |
|-----------|-------------|
| `[[inline]]` | Suggest inlining the function |
| `[[static]]` | Static linkage (not exported) |
| `[[stdcall]]` | Use stdcall calling convention |
| `[[cdecl]]` | Use cdecl calling convention (default) |
| `[[override]]` | Override base class method (see section 9.5) |
| `[[section("name")]]` | Place function in specific memory section |
| `[[naked]]` | Emit function without prologue/epilogue (embedded use) |
| `[[noreturn]]` | Function never returns (for panic/abort functions) |

**Note**: The exact set of supported attributes and their semantics are implementation-defined. Some attributes may be target-specific.

---

## 9. Objects

### 9.1 Object Declaration

```dq
object Worker {
    // Fields
    name : str;
    counter : int;

    // Constructor
    function __ctor(aname : str) {
        name = aname;
        counter = 0;
    }

    // Destructor
    function __dtor() {
        // cleanup
    }

    // Methods
    function DoWork() {
        counter += 1;
        write(name + " work #" + str(counter));
    }

    function write(msg : str) {
        .print(msg);        // leading dot = global function
    }
}
```

### 9.2 Object Instantiation

#### Stack Allocation (Value)
```dq
w : Worker("Alice");        // constructor called directly
w.DoWork();
// destructor called automatically at scope exit
```

#### Heap Allocation (Pointer)
```dq
w : ^Worker = new Worker("Bob");   // explicit type at new
w : ^Worker = new("Bob");           // target-typed new (type inferred from LHS)
w.DoWork();
delete w;                           // explicit deletion
```

### 9.3 Visibility

```dq
object MyClass {
public
    // visible outside the object

private
    // only visible within the object
}
```

### 9.4 Properties

```dq
object TSocket {
public
    property port : uint16 read mport write SetPort;

private
    mport : uint16;

    function SetPort(value : uint16) {
        mport = value;
    }
}
```

### 9.5 Inheritance

```dq
object Dog : Animal {
    // inherits from Animal
    function Speak() [[override]] {
        print("woof");
    }
}
```

**Rules**:
- Single inheritance only
- Methods are non-virtual by default
- Mark base methods as `virtual` to allow override
- Use `override` keyword in derived types
- Polymorphism requires reference semantics (`^Animal`)

### 9.6 Name Resolution in Objects

Inside object methods:
1. Bare `name` resolves to **object members first**
2. If not found, looks in local injected namespace (from local `use`)
3. `.name` always resolves to **global/module namespace**

```dq
use math;

object Circle {
    radius : float;

    function Area() -> float {
        return .PI * radius * radius;   // .PI = global PI from math
    }

    function Area2() -> float {
        use math;                        // local injection
        return PI * radius * radius;     // PI found in injected namespace
    }
}
```

---

## 10. Pointers and References

### 10.1 Pointer Syntax

```dq
p : ^int32;                 // pointer to int32
p = &x;                     // address-of (C-style)
y = p^;                     // dereference (Pascal-style)
```

**Note**: The `@` symbol is reserved for namespace references. The `&` symbol is used to get the address of a variable.

### 10.2 Pointer Arithmetic

```dq
p : ^byte = buffer;
pend : ^byte = p + length;   // pointer arithmetic allowed
while p < pend {
    // ...
    p += 1;
}
```

### 10.3 Null

```dq
p : ^Worker = null;
if p == null { ... }
delete null;                 // safe (no-op)
```

### 10.4 Ref Binding to Pointer

```dq
p : ^MyStruct = ...;
if p != null {
    ref s : MyStruct = p^;  // bind ref to pointee
    s.field = 10;           // modify through ref
}
```

---

## 11. Modules and Namespaces

### 11.1 Module Declaration

```dq
module Net.Socket

// Interface section (public declarations)

type Socket = int32;

function open(host : str, port : uint16) -> Socket;
function close(ref s : Socket);

implementation

// Implementation section (private + bodies)

function open(host : str, port : uint16) -> Socket {
    // ...
}

function close(ref s : Socket) {
    // ...
}

initialization
{
    // runs at program start (optional)
}

finalization
{
    // runs at shutdown (optional)
}

```

**Structure**:
- `implementation` keyword separates interface from implementation
- `initialization` and `finalization` sections are optional
- No braces around sections — keyword alone marks the boundary

### 11.2 Import/Use Statements

```dq
use math;                           // merge into module-global namespace
use Drivers.UART as UART;           // alias
```

### 11.3 Qualified Access

```dq
@math.PI                            // qualified access to module symbol
@Drivers.UART.init()               // nested module access
```

### 11.4 Name Resolution Order

**In object context for bare `name`**:
1. Object/type scope (members)
2. Block injected namespace (local `use`)
3. Type injected namespace (type-level `use`)
4. ERROR (module-global requires `.name`)

**For `.name`**:
1. Module-global namespace (module scope + top-level `use`)
2. Prelude / builtins

### 11.5 File Layout

```
module path 'A.B.C' -> file 'A/B/C.dq'
package root contains dq.toml (or dq.pkg)
```

---

## 12. Preprocessor and Compiler Directives

### 12.1 Compilation Pipeline

```
DQ source -> Preprocessor (#ifdef/#include) -> comp1 (comptime) -> comp2 (codegen)
```

### 12.2 Preprocessor Conditionals

```dq
#{ifdef WINDOWS}
    use windows;
#{elifdef LINUX}
    use linux;
#{else}
    #{error "Platform not supported"}
#{endif}
```

### 12.3 Compile-Time (comp1) Access of the Defines

**TODO**: Full behaviour is to be defined.


```dq
comptime if (defined(WINDOWS) {
    useimport("uart_windows.dq");
} else if (defined(LINUX) {
    import("uart_linux.dq");
} else {
    compile_error("Platform not supported");
}
```

The defined() is a special built-in function which never gives a compiler error, returns true, when a symbol defined.

The defines are accessible with a @def. namespace.


```dq
@def.WINDOWS        // platform define (bool)
@def.TARGET_ARM     // architecture
@def.DEBUG          // build configuration
@def.CUSTOM_FLAG    // user-defined
```

### 12.5 Compiler Directives

```dq
#{opt blockmode indent}     // switch to indentation mode
#{opt blockmode braces}     // switch to brace mode

#{push}                     // save directive state
#{opt blockmode indent}
// indentation-mode code here
#{pop}                      // restore previous state

#{opt warn disable W001}    // disable warning (TBD)
#{opt optimize 2}           // optimization level (TBD)
```

---

## 13. Memory Management

### 13.1 Allocation

```dq
// Stack allocation (automatic)
x : MyStruct;

// Heap allocation
p : ^MyStruct = new MyStruct();
p : ^MyStruct = new();              // target-typed
```

### 13.2 Deallocation

```dq
delete p;                           // calls destructor, frees memory
delete null;                        // safe (no-op)
```

### 13.3 Scope-Based Cleanup

```dq
function Example() {
    p : ^Worker = new("Test");
    ensure { delete p; }            // guaranteed cleanup at scope exit

    // use p...
}
```

---

## 14. Standard Library

### 14.1 Built-in Functions

```dq
print(...)          // output without newline
println(...)        // output with newline
length(x)           // length of array/string (Pascal-style)
sizeof(T)           // size in bytes
typeof(x)           // type information (TBD)
```

### 14.2 String Operations

```dq
length(s)           // character count (Pascal-style function)
s.length            // also available as property
s[i]                // indexed access (char)
s.split(',')        // split into str[...]
s1 + s2             // concatenation
```

### 14.3 Array Operations

```dq
arr.length          // element count
arr[i]              // indexed access
arr.append(x)       // add element (dynamic arrays)
```

---

## 15. Explicitly Rejected Features

The following features have been explicitly rejected for DQ:

| Feature | Reason |
|---------|--------|
| Implicit int↔bool conversion | Major source of bugs in C |
| Implicit numeric widening | Causes `3/2*10 != 3*10/2` bugs |
| C-style `::` namespace operator | Visual noise; use `.` and `@` instead |
| Mandatory `this.` / `self.` | Reduces readability |
| Textual macros | Unsafe, no types, no scope |
| Assignment as expression | Assignment is statement-only; `=` in expressions means comparison |
| C operator precedence | `a & b == 0` trap |
| Case-insensitive identifiers | Causes problems at scale |
| Unicode identifiers | Keep identifiers ASCII-only |
| Numeric literal suffixes | Types should be clear from declarations |

---

## 16. Open Questions

### 16.1 Syntax Questions
- [ ] Lambda syntax (if supported)
- [ ] Pattern matching syntax

### 16.2 Semantic Questions
- [ ] Full generics design (planned, Pascal-style syntax)
- [ ] Interface/trait system
- [ ] Nullable types (`?T` syntax?)
- [ ] Operator overloading rules
- [ ] Move semantics
- [ ] Concurrency primitives

### 16.3 Generics (Planned)

Generics will use Pascal-style `specialize` syntax:

```dq
// Generic type definition (syntax TBD)
type TFPGMapObject<TKey, TValue> = object {
    // ...
}

// Specialization
type TNanoSocketMap = specialize TFPGMapObject<TSocket, TNanoSocket>;
```

### 16.4 Exception Handling (Planned)

Exceptions will follow Pascal/Python style with `raise`:

```dq
// Raising exceptions
raise EInvalidArgument("value out of range");

// Handling exceptions (syntax TBD, likely try/except)
try {
    risky_operation();
}
except EFileNotFound as e {
    println("File not found: ", e.message);
}
except {
    // catch all
}
```

### 16.5 Standard Library Questions
- [ ] Prelude contents (what's auto-imported?)
- [ ] Collection types
- [ ] I/O abstractions
- [ ] Error handling conventions

### 16.6 Tooling Questions
- [ ] Canonical formatter style
- [ ] Package manager design
- [ ] Debug format (DWARF)
- [ ] Language server protocol support

---

## Appendix A: Example Programs

### A.1 Basic Syntax Example

```dq
function imul(a : int, b : int) -> int {
    return a * b;
}

function main() -> int {
    var i1 : int = 2;
    var i2 : int = 3;
    var f : float = i1 / i2;
    println("f =", f);

    var res : int = imul(i1, i2);
    println("mul(i1, i2) =", res);

    // for-loop example
    var sum_for : int = 0;
    for i = 0 to 4 {
        sum_for += i;
    }
    println("sum_for =", sum_for);

    // while-loop example
    var sum_while : int = 0;
    var j : int = 0;
    while j < 5 {
        sum_while += j;
        j += 1;
    }
    println("sum_while =", sum_while);

    // if / else with boolean expression
    if (i1 < i2 and sum_for == sum_while) or (res > 0 and f < 1.0) {
        println("condition is TRUE");
    }
    else: println("condition is FALSE");

    return 0;
}
```

### A.2 Object Example

```dq
import io;

object WorkHelper {
    worklog : int[...];

    function help(anum : int) {
        worklog.append(anum);
    }
}

object Worker {
    name    : str;
    counter : int;
    helper  : ^WorkHelper;

    function __ctor(aname : str) {
        name = aname;
        counter = 0;
        helper = new WorkHelper();
    }

    function __dtor() {
        delete helper;
    }

    function DoWork(anum : int) {
        helper.help(anum);
        counter += 1;
        write(name + " work #" + str(counter));
    }

    function write(msg : str) {
        .print(msg);            // global print
    }
}

function main() {
    // stack-allocated object
    w1 : Worker("Alice");
    w1.DoWork(5);
    w1.DoWork(3);

    // heap-allocated object
    w2 : ^Worker = new("Bob");
    ensure { delete w2; }
    w2.DoWork(7);
}
```

### A.3 Expressions Example

```dq
import sockets;

type TSockCallBackFunc = function(aobj : pointer, asock : int) of object;

object TSockTester {
public
    property port : uint16 read mport write SetPort;
    onevent : TSockCallBackFunc = null;

    function __ctor(aport : uint16) {
        mport = aport;
    }

private
    mport : uint16;
    msocket : int = -1;

    function SetPort(avalue : uint16) {
        mport = avalue;
    }
}

function main() -> int {
    println("Hello from DQ!");

    i1 : int = 3;
    i2 : int = 12345;

    i3 : int = i2 AND i1;           // bitwise AND

    // if (i3)                      // ERROR: bool expression expected

    if i3 <> 0 {                    // OK: explicit comparison
        println("i3 is not null!");
    }

    // Precedence: no parentheses needed
    if i2 AND i1 <> 0 and (i1 SHL 1) > 5 {
        println("both are true!");
    }

    // i4 : int = i2 / i1;          // ERROR: / returns float
    f1 : float = i2 / i1;           // OK
    i5 : int = i2 IDIV i1;          // OK: integer division
    i6 : int = round(i2 / i1);      // OK: explicit rounding

    b : bool = i2 <> i1;
    b2 : bool = (i6 == i5);         // '==' or '=' both work

    return 0;
}
```

---

## Appendix B: Changelog

### v0.1.8 (2026-01-22)
- Added `byte` as alias for `uint8` (no `word` due to platform ambiguity)
- Clarified assignment is statement-only, no chaining allowed (`a := b := 1` is error)
- Added note about `result` variable for function return values
- Changed override syntax to attribute style: `function Speak() [[override]]`
- Simplified module declaration: removed `export` keyword, added braces to `initialization`/`finalization`
- Simplified `use` statements (removed `interface` and `for objects` variants)
- Simplified name resolution rules
- Added `defined()` function for compile-time conditionals (marked TODO for full spec)
- Updated operator precedence examples with `<<` shorthand

### v0.1.7 (2026-01-22)
- Changed constant syntax to `const(type)` for both single-line and block forms
- Added block form `const(type) { ... }` for grouping related constants

### v0.1.6 (2026-01-22)
- Added enumeration types with mandatory underlying storage type
- Enum values use Pascal-style module scope (no qualification needed)
- Support for explicit values (for protocols, hardware registers)
- Explicit conversions required between enum and integer types

### v0.1.5 (2026-01-22)
- Changed dynamic array syntax from `array<T>` to `T[...]` for consistency with fixed arrays
- Added multi-dimensional array examples
- Added `char(codepoint)` syntax for creating characters from Unicode values

### v0.1.4 (2026-01-22)
- Added `@arg.` namespace for explicit argument access within functions

### v0.1.3 (2026-01-22)
- Added multi-line string literals using triple quotes (`"""..."""` or `'''...'''`)
- Auto-dedent based on closing quote indentation
- Newline trimming at start/end when quotes are on their own lines

### v0.1.2 (2026-01-22)
- Changed address-of operator from `ptr()` function to `&` symbol
- Changed integer modulo from `mod` to `IMOD` keyword (consistency with `IDIV`)
- Added attribute syntax using `[[ ... ]]` brackets
- Added function attributes section with common attributes
- Changed named parameter syntax from `=>` to `:=` for clarity
- Changed compiler directive syntax from `#{comp ...}` to `#{opt ...}`
- Unified string/char literals: both quote styles allowed, type determined by length

### v0.1.1 (2026-01-21)
- Initial draft specification
- Consolidated from multiple ChatGPT design conversations
- Core language features defined
- Many details still marked as TBD/Open Questions

---

*This specification is a work in progress. Feedback and contributions are welcome.*
