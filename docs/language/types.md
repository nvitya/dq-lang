# Types

DQ is statically and strictly typed. Most conversions must be explicit, with a
small number of numeric conversions provided for convenience.

## Primitive Types

The common primitive types are:

| Type | Meaning |
| --- | --- |
| `bool` | Boolean value, either `true` or `false` |
| `int`, `uint` | Signed and unsigned integer with pointer-sized width |
| `int8`, `int16`, `int32`, `int64` | Fixed-width signed integers |
| `uint8`, `uint16`, `uint32`, `uint64` | Fixed-width unsigned integers |
| `byte` | Alias for `uint8` |
| `float32`, `float64` | Floating point types |
| `float` | Platform-preferred floating point type |
| `char` | DQ character value |
| `cchar` | C-compatible character byte |
| `pointer` | Untyped generic pointer |
| `Object` | Untyped object, compatible with all objects |

`int` and `uint` have the same width as a pointer. Use fixed-width integer types
when binary layout or C ABI details matter.

## Boolean Type

`bool` is distinct from numeric types. Numeric values are not implicitly used as
conditions.

```dq
var n : int = 1

if n <> 0:
    // ok
endif
```

## Numeric Conversions

Integer values may be converted to floating point values when needed. Other
conversions should be written explicitly with type-call syntax.

```dq
var i : int = 3
var f : float64 = i
i = Round(f + 1)
```

Floating point to integer conversions should use the available conversion
functions such as `Round`, `Floor`, or `Ceil` where appropriate.

## Type Aliases

`type` creates an alias.

```dq
type TFloat = float64
type FCallback = function(value : int) -> int
```

## Type Inference

General type inference for variables is not implemented. Write the declared type
explicitly.

```dq
var value : int = 3
```

The special `?` marker is currently used for fixed array length inference from
an array literal.

```dq
var values : [?]int = [1, 2, 3]
```

## Structures

`struct` defines a value type with fields.

```dq
struct SPoint:
    x : int
    y : int
endstruct
```

Struct values can be initialized with `{}` to zero/default initialize their
fields.

```dq
var p : SPoint = {}
```

Struct fields are accessed with `.`.

```dq
p.x = 10
p.y = 20
```

Pointers to structs are automatically dereferenced for member access.

```dq
var pp : ^SPoint = &p
pp.x = 11      // same target as pp^.x
```

Structs may also have methods. Methods are declared inside the struct or defined
outside with a qualified name.

## Objects

`object` defines a reference type with fields, methods, properties, inheritance,
constructors, destructors, and virtual dispatch. See
[Objects](objects.md).

## Enumerations

`enum` defines a distinct enumeration type.

```dq
enum NColor = (red, green, blue)
enum NState : uint8 = (idle = 0, running = 10, stopped = 20)
```

The storage type must be an integer type. If no storage type is specified, the
compiler chooses the default enum storage type.

Enum values are strongly typed. They do not implicitly convert to or from
integers, and different enum types are not interchangeable.

```dq
var c : NColor = red
var q : NColor = NColor.green
```

Enum values may be used without qualification when the expected enum type is
known from context.

```dq
function IsGreen(color : NColor) -> bool:
    return color == green
endfunc
```

Enums provide ordinal conversion helpers.

```dq
var s : NState = NState.FromOrd(10)
var fallback : NState = NState.FromOrd(11, idle)

var out : NState = idle
if NState.TryFromOrd(20, out):
    // out was assigned
endif
```

`FromOrd(value)` raises a runtime error if the ordinal is invalid. The overload
with a fallback returns the fallback for invalid values. `TryFromOrd` returns a
boolean success flag and writes the output argument when valid.

## Fixed Arrays

Fixed arrays are value types with a compile-time length.

```dq
var values : [3]int = [1, 2, 3]
var inferred : [?]int = [10, 20, 30]
```

`[?]T` infers the fixed array length from the array literal.

Fixed arrays expose a `.length` property and support indexing and slicing.

## Array Literals

Array literals are written with square brackets.

```dq
var static_values : [?]int = [1, 2, 3]
var dynamic_values : [*]int = [1, 2, 3]
```

The expected type determines whether the literal initializes a fixed array,
dynamic array, array slice-compatible value, or another supported array-like
target.

## Dynamic Arrays

Dynamic arrays are written as `[*]T`.

```dq
var values : [*]int = [1, 2, 3]
values.Append(4)
```

Dynamic arrays expose `.length` and `.capacity`, support indexing and slicing,
and provide mutation methods such as `Append`, `Prepend`, `Insert`, `Delete`,
`Pop`, `PopFirst`, `SetLength`, `SetCapacity`, `Reserve`, `Compact`, and
`Clear`.

## Array Views

Function parameters often use view-style array types such as `[]T`.

```dq
function Sum(values : []int) -> int:
    var i : int = 0
    while i < values.length:
        result += values[i]
        i += 1
    endwhile
endfunc
```

Slices produce view values.

```dq
var a : [*]int = [1, 2, 3, 4]
Sum(a[1:3])
Sum(a[:])
```

## Strings

DQ has several text-related types:

| Type | Meaning |
| --- | --- |
| `str` | Dynamic heap-managed string |
| `strview` | Non-owning string view |
| `cstring(n)` | Fixed-size zero-terminated C-style string storage |
| `cstring` | C-style string argument type |
| `^cchar` | Pointer to C-compatible character data |

## String and Character Literals

Double-quoted literals are text. Single quotes can also delimit text, but a
single-quoted literal containing exactly one character is a `char`.

```dq
var text1 : str = "hello"
var text2 : str = 'hello'
var slash_text : str = "/"
var slash_char : char = '/'
```

This matters when comparing text. `"/"` is a string literal, but `'/'` is a
character literal.

```dq
if url == "/":
    // ok: compares text with text
endif

if url == '/':
    // wrong: '/' is char, not str/strview/cstring text
endif
```

Use double quotes for one-character strings when the value is text. Use single
quotes for character values, such as string indexing results or APIs that take
`char`.

```dq
if url[0] == '/':
    // ok: compares char with char
endif
```

`str` is copy-on-write. Assigning a string value shares storage until a value is
mutated.

```dq
var a : str = "abc"
var b : str = a
b[0] = 'X'      // a remains "abc"
```

Dynamic strings expose `.length` and `.capacity` and provide mutation methods
such as `Append`, `Prepend`, `Insert`, `Delete`, `SetLength`, `Truncate`, `Pop`,
`PopFirst`, `Reserve`, `Compact`, `Clear`, and `Clone`.

## Anyvalue

`anyvalue` can hold values for generic formatting and variable argument style
APIs.

```dq
var v : anyvalue = 123
var s : str = v.AsStr("")
```

Arrays of `anyvalue` are commonly used with formatting functions.

```dq
Print("{}: {}", ["answer", 42])
```

## Function Reference Types

Function references are declared with `function(...)`.

```dq
type FUnary = function(value : int) -> int

function Inc(value : int) -> int:
    return value + 1
endfunc

var cb : FUnary = Inc
var result : int = cb(10)
```

Function references can be compared with `null`.

Object method references use `of object`.

```dq
type FObjText = function(msg : cstring) of object
```
