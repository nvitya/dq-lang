# DQ Enum Specification

## 1. Purpose

DQ enums define a closed, safe set of named integer-backed values.

The design is inspired by Delphi/FreePascal enums, but DQ allows explicit integer value assignments while preserving strong type safety. An enum variable can only contain one of the explicitly declared enumerator values.

Enum type names conventionally use the `N` prefix.

```dq
enum NColor:
  red
  green
  blue
endenum
```

The `E` prefix is reserved for exception types.

```dq
// Preferred naming convention
enum NTokenKind:
  identifier
  number
  string_literal
endenum

object EParseError:
endobj
```

## 2. Basic Syntax

```dq
enum NEnumName:
  item1
  item2
  item3
endenum
```

Each enum item receives an integer ordinal value.

By default, the first item has value `0`, and following items auto-increment by `1`.

```dq
enum NColor:
  red    // 0
  green  // 1
  blue   // 2
endenum
```

## 3. Explicit Integer Values

Enum items may have explicit integer values.

```dq
enum NColor:
  red = 1
  green = 5
  blue = 9
endenum
```

After an explicit value, following unassigned items continue from that value plus one.

```dq
enum NColor:
  red          // 0
  green = 5    // 5
  blue         // 6
  yellow = 10  // 10
  black        // 11
endenum
```

Explicit values must be compile-time integer constants representable by the enum storage type.

## 4. Enum Storage Type

An enum may specify an explicit integer storage type.

```dq
enum NHttpStatus : uint16:
  ok = 200
  not_found = 404
  server_error = 500
endenum
```

If no storage type is specified, the default storage type is `int32`.

Recommended allowed storage types:

```dq
int8
uint8
int16
uint16
int32
uint32
int64
uint64
```

Embedded code should use explicit storage types when binary layout matters.

```dq
enum NUartMode : uint8:
  normal = 0
  rs485 = 1
  irda = 2
endenum
```

## 5. Qualified Enumerator Access

The fully qualified form is always valid.

```dq
var c : NColor = NColor.red;
```

This form is preferred in ambiguous contexts, public APIs, generated code, and documentation.

## 6. Contextual Unqualified Enumerator Access

DQ also allows Delphi-like short enum item names without the enum type name, but only when the expected enum type is known from context.

```dq
var c : NColor = red;       // ok: expected type is NColor

if c == green:              // ok: left side gives NColor context
endif

function SetColor(acolor : NColor):
endfunc

SetColor(blue);             // ok: parameter type gives NColor context
```

Unqualified enum access is rejected when the enum type cannot be inferred.

```dq
var x : int = red;          // error: incompatible types
var x : ? = red;            // error: enum type is not inferable
Print(red);                 // error unless the argument type is known
```

Rule summary:

> Enumerators are scoped inside their enum type, but may be resolved unqualified from expected-type context.

## 7. Assignment and Type Safety

Enums are distinct strong types.

```dq
var c : NColor = NColor.red;
```

Implicit enum-to-integer conversion is not allowed.

```dq
var i : int = c;            // error
```

Implicit integer-to-enum conversion is not allowed.

```dq
var c : NColor = 1;         // error
```

Unchecked integer-to-enum casts are not allowed.

```dq
var c : NColor = NColor(1); // error
```

An enum is a safe closed type. It cannot carry an invalid value. Integer values received from external data must be validated before becoming enum values.

For protocol constants, hardware register fields, open numeric domains, and other values where arbitrary or unknown numeric values may occur, normal integer constants should be used instead of enums.

```dq
const CMD_READ  : uint8 = 0x01;
const CMD_WRITE : uint8 = 0x02;
```

## 8. Ordinal Conversion

Enum-to-integer conversion is performed with the built-in `Ord()` function.

```dq
var c : NColor = green;
var i : int = Ord(c);       // returns the integer value of green
```

`Ord()` is the only normal enum-to-integer conversion.

Direct casts from enum to integer are not allowed.

```dq
var i : int = int(c);       // error
```

The result type of `Ord()` is an integer type large enough to represent the enum storage type. Assignment to a narrower integer type follows normal DQ integer conversion rules.

## 9. Checked Integer-to-Enum Conversion

Integer-to-enum conversion is always checked and must use one of the generated enum helper functions.

For every enum type, the compiler provides these functions:

```dq
EnumType.FromOrd(value : int) -> EnumType
EnumType.FromOrd(value : int, defvalue : EnumType) -> EnumType
EnumType.TryFromOrd(value : int, rval : ref EnumType) -> bool
```

Example:

```dq
enum NState:
  idle = 0
  run = 10
  stop = 20
endenum

var s : NState;

s = NState.FromOrd(10);              // ok: run
s = NState.FromOrd(11);              // runtime error: invalid enum value
s = NState.FromOrd(11, NState.idle); // returns idle

if NState.TryFromOrd(20, s):     // true, s = stop
endif

if NState.TryFromOrd(21, s):     // false, s is not modified
endif
```

Rules:

- `FromOrd(value)` returns the enum value if `value` matches a declared enumerator value.
- `FromOrd(value)` raises a runtime error if `value` is not a declared enumerator value.
- `FromOrd(value, defvalue)` returns the enum value if valid, otherwise returns `defvalue`.
- `TryFromOrd(value, rval)` returns `true` and writes `rval` if valid.
- `TryFromOrd(value, rval)` returns `false` and leaves `rval` unchanged if invalid.

Only declared enum values are valid, even when the enum has holes.

## 10. Comparisons

Values of the same enum type may be compared directly.

```dq
if c == red:
endif

if c != NColor.blue:
endif
```

Different enum types are not directly comparable.

```dq
var c : NColor;
var s : NLedState;

if c == s:                  // error: different enum types
endif
```

Explicit ordinal conversion may be used when numeric comparison is really intended.

```dq
if Ord(c) == Ord(s):
endif
```

## 11. Duplicate Values

Duplicate enum values are always a compile-time error.

```dq
enum NResult:
  ok = 0
  success = 0              // error: duplicate enum value
endenum
```

Every enumerator in an enum must have a unique integer value.

No aliasing attribute is supported for normal enums.

## 12. Holes and Non-Continuous Enums

Enums may contain holes.

```dq
enum NState : uint8:
  idle = 0
  run = 10
  stop = 20
endenum
```

Only the declared values are valid enum values.

```dq
var s : NState = NState.FromOrd(10); // ok
var t : NState = NState.FromOrd(11); // runtime error
```

Holes do not make the enum an open integer range.

## 13. Forward Compatibility

The following features are intentionally left for later specification:

- enum reflection
- enum-to-string helpers
- string-to-enum parsing
- iteration over enum items
- bitflag enums
- scoped imports or enum aliases

Bitflag-style enums should likely be a separate feature, because their semantics differ from normal closed enumerations.
