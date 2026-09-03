# Types and Conversions

This page defines primitive types and general conversion rules. See the shorter
[Types guide](../language/types.md) for examples.

## Type Categories

DQ is statically and strictly typed. Important categories are:

- Boolean: `bool`;
- signed integers: `int`, `int8`, `int16`, `int32`, `int64`;
- unsigned integers: `uint`, `uint8`, `uint16`, `uint32`, `uint64`;
- floating point: `float`, `float32`, `float64`;
- characters: `char`, `char16`, `wchar`;
- text: `str`, `strview`, `cstring(N)`, and unsized `cstring`;
- pointers: `^T` and the generic `pointer`;
- aggregate and reference types: arrays, structures, enumerations, objects,
  func references, and `anyvalue`.

`byte` aliases `uint8`. `int` and `uint` have pointer-sized width. `Object` is
the common object-reference type.

## Boolean Isolation

`bool` is not an integer. Conditions and lowercase logical operators require
Boolean operands; numeric truthiness is not supported. Conversions between
Boolean and numeric types must be expressed by program logic rather than casts.

## Integer and Floating-Point Conversion

Widening or otherwise context-supported integer-to-floating conversion may be
implicit. Potentially narrowing or representation-changing conversions should
use type-call syntax.

```dq
var small : int32 = int32(value)
var real : float64 = integer_value
```

Floating-point to integer conversion uses `Round`, `Floor`, or `Ceil` as
appropriate. Assignment and overload resolution reject conversions that are not
defined for the source and destination types.

## Character Conversion

`char`, `char16`, and `wchar` are distinct from integers. `Ord(value)` obtains an
integer ordinal. Type-call casts construct a character when the value is valid
for the destination representation.

```dq
var n : uint = Ord(ch)
var byte_ch : char = char(n)
var scalar : wchar = wchar(n)
```

`IntToChar` and `IntToWchar` provide checked runtime conversion, with raising
and fallback overloads. A single-character literal has type `wchar`; it can
initialize `char` only when its constant value is below 256.

Arithmetic is not defined directly on character values. Convert with `Ord`
first, perform arithmetic, then convert back explicitly.

## Enumeration Conversion

Enums are distinct from all integer types and from other enum types. Use
`Ord`, `FromOrd`, or `TryFromOrd` rather than integer casts. See [Enums](enums.md).

## Pointer Conversion

Typed pointers preserve their pointee type. Explicit casts may convert between a
typed pointer and `pointer`, between compatible pointer representations, or
between a raw pointer and an object reference where supported. Integer/pointer
conversions are explicit and are the programmer's responsibility.

## Text Conversion

Text conversions distinguish ownership and encoding. Passing `str`, `strview`,
or compatible `cstring` data to a view-style parameter may borrow storage;
producing an owned `str` retains or copies as required. UTF-8, UTF-16, and
Unicode-scalar conversion uses explicit helpers. See
[Strings and Characters](strings-and-characters.md).

## Assignment Compatibility

Assignment requires the source to be the destination type or to have a defined
assignment conversion. The same compatibility check is used for initialization,
arguments, and return values, with additional borrowing rules for reference and
view parameters.

No general user-defined implicit conversions, numeric-to-Boolean conversions,
or unrelated pointer conversions are performed.
