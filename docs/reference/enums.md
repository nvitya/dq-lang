# Enumerations

This page defines enumeration behavior. Common examples are also shown in the
[Types guide](../language/types.md).

## Declaration

An enum declares a closed set of named values backed by an integer storage
type.

```dq
enum NColor = (red, green, blue)
enum NState : uint8 = (idle = 0, running = 10, stopped = 20)
```

When no storage type is written, the storage type is `uint8`. An explicit
storage type must be one of the fixed-width signed or unsigned integer types
from `int8` through `uint64`, and every enumerator value must fit that type.

Implicit values begin at zero and increment from the previous value. An
explicit value changes the value from which following implicit enumerators are
calculated. After the first enumerator, an explicit value cannot move backward.
Every enumerator value must be unique. A duplicate is a compile error, and a
numeric value not assigned to an enumerator is not a valid member of the enum.

## Naming and Lookup

Qualification uses the enum type name.

```dq
var color : NColor = NColor.green
```

An enumerator may be unqualified when contextual typing identifies exactly one
enum type, including initialization, assignment, argument, return, comparison,
and `case`-like contexts supported by the compiler. Otherwise it must be
qualified.

## Type Safety

An enum is distinct from its storage type and every other enum. There is no
implicit conversion to or from integers, and arithmetic is not defined on enum
values.

Equality and inequality compare values of the same enum type. Use `.ord` or
`Ord(value)` explicitly when numeric ordering is intended.

## Ordinal Conversion

The read-only `.ord` property and `Ord(value)` return the stored integer value.
Checked construction uses the enum type's helpers:

```dq
var state : NState = NState.FromOrd(10)
var fallback : NState = NState.FromOrd(11, idle)

var output : NState = idle
if NState.TryFromOrd(20, output):
    // output is stopped
endif
```

`FromOrd(value)` raises a runtime error if no enumerator has that value. The
two-argument overload returns its fallback instead. `TryFromOrd` returns `true`
and updates the output only for a valid value.

## Forward Compatibility

When persistent or wire formats may gain new values, decode with `TryFromOrd` or
the fallback overload rather than assuming every stored integer is currently
known.
