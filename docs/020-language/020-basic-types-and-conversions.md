# Basic Types and Conversions

A type describes the values a variable can store and the operations allowed on
it. DQ checks types during compilation. It distinguishes numbers, Boolean values,
characters, text, pointers, and compound types rather than converting freely
between unrelated categories.

```dq
use print

func *Main():
    var count : int = 3
    var ratio : float = count / 2
    var ready : bool = count > 0
    PrintLn("{} {} {}", [count, ratio, ready])
endfunc
```

## Scalar Types

| Type | Values and representation |
| --- | --- |
| `bool` | `true` or `false`; not an integer. |
| `int`, `uint` (1) | Signed/unsigned integers as wide as a pointer. |
| `int8`, `int16`, `int32`, `int64` (1)| Signed integers of the specified bit width. |
| `uint8`, `uint16`, `uint32`, `uint64` (1)| Unsigned integers of the specified bit width. |
| `byte` | Alias for `uint8`. Preferred instead of `uint8` |
| `float32`, `float64` | Floating-point numbers of the specified width. |
| `float` (2)| Floating-point type chosen by the target. `float32` on the single-precision FPU targets arm_m4f-bare, arm_m33f-bare, and arm_m7f-bare. It is `float64` on other current targets, including arm_m7fd-bare and WebAssembly. |
| `char` | One unsigned 8-bit byte or UTF-8 code unit. |
| `char16` | One unsigned 16-bit UTF-16 code unit. |
| `wchar` | One unsigned 32-bit Unicode scalar value. |
| `pointer` | Generic address with no pointee type. |

**Notes:**

 **(1)**: For an unsigned integer with `N` bits, the range is `0` through `2^N - 1`. <br>For a signed integer, it is `-2^(N-1)` through `2^(N-1) - 1`.<br>`int`/`uint` are 32 bits on 32-bit targets and 64 bits on 64-bit targets.<br>Choose a fixed-width type when a file format, protocol, or external ABI requires it.

 **(2)**: Use explicit floating-point widths avoid making a binary interface depend on the target default.

## Other Type Categories

| Form | Purpose | Chapter |
| --- | --- | --- |
| `str`, `rostr`, `strslice`, `embstr(N)` | Text with different storage and borrowing rules. | [Character and String Types](070-strings-and-characters.md) |
| `[N]T`, `[*]T`, `[]T` | Fixed arrays, dynamic arrays, borrowed array views. | [Arrays](080-arrays-and-slices.md) |
| `enum` | A distinct set of named values. | [Enumerations](090-enumerations.md) |
| `struct`, `union` | Direct field storage or overlapping field storage. | [Structures and Unions](100-structures-and-unions.md) |
| `^T` | An address pointing to a `T`. | [Pointers](110-pointers.md) |
| `object`, `Object` | Object reference types; `Object` is the common object base type. | [Objects](120-objects-and-properties.md) |
| `func(...)` | A function reference with a particular signature. | [Functions](060-functions.md#function-references) |
| `anyvalue` | A boxed scalar/text value for heterogeneous APIs. | [anyvalue](150-anyvalue.md) |

`autofree T` qualifies object-reference and pointer types with cleanup ownership;
it does not change the underlying pointer representation. See
[Memory Management and Ownership](130-memory-management-and-ownership.md).

## Type Aliases

`type` introduces another name for an existing type:

```dq
type TCount = uint32
type FTransform = func(value : int) -> int
```

An alias does not create a new, incompatible type. In contrast, two separately
declared enumerations, structures, or unions remain distinct even if their
contents look identical.

## Numeric Conversions

Whole-number literals are contextual. Write `var scale : float = 2` or
`var size : uint8 = 3` directly. Integer expressions can convert to another
integer storage width; narrowing keeps the low bits. Integer-to-floating-point
conversion is available where the expected numeric type requires it.

```dq
var small : uint8 = 250
small = small + 10       // 260 is stored as 4 in eight bits
var real : float64 = small
```

A destination does not retroactively widen the operands of an expression. The
calculation width and signedness rules are explained in
[Expressions and Operators](040-expressions-and-operators.md#integer-calculations).

Floating-point values do not implicitly convert to integers. Use `Round`, `Floor`,
`Ceil`, or `Trunc` to choose the rounding operation:

```dq
var value : float = 3 / 2
var rounded : int = Round(value)
var downward : int = Floor(value)
var upward : int = Ceil(value)
var truncated : int = Trunc(value)
```

Type-call syntax such as `uint16(value)` requests a defined explicit conversion.
The `expression as Type` spelling uses the same conversion rules. A cast is not
a substitute for a rounding operation, range check, or valid memory address.

## Boolean Isolation

Numbers and Boolean values are separate. Conditions and logical operators take
`bool`; `if count:` and `int(true)` are not numeric conversion shortcuts.
Convert through explicit program logic, for example `iif(ready, 1, 0)` or `n <> 0`.

## Characters, Enumerations, and Pointers

Character types are distinct from integers. Prefer the read-only `ch.ord`
property to obtain a character code as `int`; `Ord(ch)` is equivalent.
`char(code)`, `char16(code)`, and `wchar(code)` request explicit conversions.
Checked `IntToChar`/`IntToWchar` helpers provide raising and fallback forms.
A constant character literal below 256 can initialize a `char` directly.
Perform character arithmetic on its integer code, for example
`char(ch.ord + 1)`.

Enums use `Ord`, `.ord`, `FromOrd`, and `TryFromOrd`; ordinary integer casts do
not bypass enum membership checks. See [Enumerations](090-enumerations.md).

Integer/address and unrelated pointer conversions require explicit casts.
Converting an address does not establish its lifetime, alignment, or ownership.

## Size and Layout Queries

`SizeOf(Type)` and `SizeOf(value)` report storage size in bytes. `OffsetOf(Type,
field)` reports a structure or union member's byte offset. Use these queries
rather than assuming pointer widths or padding. `Len(value)` and `.length`
measure logical array elements or text bytes, not the total storage size.
