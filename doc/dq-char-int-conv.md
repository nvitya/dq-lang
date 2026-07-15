# DQ Character–Integer Conversion Specification

## 1. Scope

This document defines conversions between the DQ character types and integer types:

- `char`
- `wchar`
- integer types

The goal is to keep `char` and `wchar` type-safe and incompatible with integers while still supporting efficient low-level conversions.

---

## 2. Type Compatibility

`char` and `wchar` are distinct character types.

They are not implicitly compatible with integer types.

Character literals are always `wchar`.

Assignment of a character literal to `char` is accepted only when the literal's
`wchar` value is less than `256`.

```dq
var c1 : char  = 'a'       // valid, Ord('a') < 256
var c2 : char  = '€'       // compile-time error, Ord('€') >= 256
var wc : wchar = '€'       // valid
```

The following assignments are therefore invalid:

```dq
var c  : char
var wc : wchar
var x  : int

c  = x   // error
wc = x   // error
x  = c   // error
x  = wc  // error
```

Explicit conversion is required in both directions.

---

## 3. Character to Integer Conversion

The built-in `Ord()` function returns the integer value of a character:

```dq
Ord(value : char)  -> int
Ord(value : wchar) -> int
```

The conversion is exact and cannot fail.

Examples:

```dq
var c  : char  = 'A'
var wc : wchar = '€'

var ci  : int = Ord(c)
var wci : int = Ord(wc)
```

---

## 4. Explicit Integer to Character Casts

The character type names provide explicit integer-to-character casts:

```dq
char(value)
wchar(value)
```

Examples:

```dq
var c  : char  = char(12)
var wc : wchar = wchar(0xFD00)
```

These casts are intended to be zero-overhead operations at runtime.

### 4.1 Constant Arguments

When the argument is a compile-time constant expression, the compiler validates the value.

For `char`, the constant value must not be greater than `255`.

```dq
char(65)       // valid
char(255)      // valid
char(256)      // compile-time error
char(200 + 56) // compile-time error
```

For `wchar`, the constant value must be a valid Unicode scalar value:

```text
0x0000 .. 0xD7FF
0xE000 .. 0x10FFFF
```

The UTF-16 surrogate range is invalid:

```text
0xD800 .. 0xDFFF
```

Examples:

```dq
wchar(0x0041)    // valid
wchar(0xFD00)    // valid
wchar(0x1F600)   // valid
wchar(0xD800)    // compile-time error
wchar(0x110000)  // compile-time error
```

Constant-expression checking is performed after constant folding.

### 4.2 Runtime Arguments

When the argument is not a compile-time constant, `char()` and `wchar()` perform unchecked runtime casts.

No range check, branch, exception, or conversion helper is required.

```dq
var x : int = GetValue()

var c  : char  = char(x)
var wc : wchar = wchar(x)
```

This allows efficient operations such as:

```dq
c = char(Ord(c) + 1)
```

The explicit cast makes the numeric interpretation visible while preserving zero-overhead execution.

---

## 5. Runtime-Checked Integer to `char` Conversion

Runtime-checked conversion to `char` is provided by:

```dq
function IntToChar(value : int) -> char
function IntToChar(value : int, defvalue : char) -> char
```

### 5.1 Raising Variant

```dq
IntToChar(value)
```

- Returns `char(value)` when `value <= 255`.
- Raises a range or conversion error when `value > 255`.

Examples:

```dq
IntToChar(65)   // 'A'
IntToChar(255)  // char(255)
IntToChar(256)  // error
```

Only the upper limit is checked. Negative values are accepted and use the normal integer-to-`char` narrowing semantics.

```dq
IntToChar(-1)   // char(255), with normal low-8-bit narrowing
```

### 5.2 Default-Value Variant

```dq
IntToChar(value, defvalue)
```

- Returns `char(value)` when `value <= 255`.
- Returns `defvalue` when `value > 255`.
- Does not raise an error for an out-of-range upper value.

Example:

```dq
IntToChar(300, '?')  // returns '?'
```

---

## 6. Runtime-Checked Integer to `wchar` Conversion

Runtime-checked conversion to `wchar` is provided by:

```dq
function IntToWchar(value : int) -> wchar
function IntToWchar(value : int, defvalue : wchar) -> wchar
```

A valid value must be a Unicode scalar value:

```text
0x0000 .. 0xD7FF
0xE000 .. 0x10FFFF
```

### 6.1 Raising Variant

```dq
IntToWchar(value)
```

- Returns `wchar(value)` when the value is valid.
- Raises a range or conversion error otherwise.

Examples:

```dq
IntToWchar(0x41)      // 'A'
IntToWchar(0x1F600)   // valid
IntToWchar(0xD800)    // error
IntToWchar(0x110000)  // error
```

### 6.2 Default-Value Variant

```dq
IntToWchar(value, defvalue)
```

- Returns `wchar(value)` when the value is valid.
- Returns `defvalue` when the value is invalid.
- Does not raise an error for an invalid value.

Example:

```dq
IntToWchar(0x110000, '?')  // returns '?'
```

---

## 7. Arithmetic

Direct arithmetic between character and integer types is not implicitly allowed:

```dq
var c : char = 'A'

c = c + 1  // error
```

The programmer must explicitly convert the character to an integer and explicitly cast the result back:

```dq
c = char(Ord(c) + 1)
```

This preserves the distinction between character values and numeric values without adding runtime overhead.

---

## 8. Conversion Summary

```dq
Ord(c)                  // char or wchar -> int; exact

char(x)                 // explicit cast
                        // constant: compile-time upper-range check
                        // runtime: unchecked

wchar(x)                // explicit cast
                        // constant: compile-time Unicode scalar check
                        // runtime: unchecked

IntToChar(x)            // runtime check for x > 255; raises on failure
IntToChar(x, defvalue)  // runtime check for x > 255; fallback on failure

IntToWchar(x)           // runtime Unicode scalar validation; raises on failure
IntToWchar(x, defvalue) // runtime Unicode scalar validation; fallback on failure
```
