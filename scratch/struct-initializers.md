# DQ Struct Initializer Literals

Status: design summary; not implemented yet.

## Motivation

Peripheral descriptions and similar constant data often consist of several
related values.  In C this is sometimes represented by a macro which expands to
several function arguments:

```c
#define PAD_SD0_CLK  0x000, 0x03001A00, (0x000 + 7)
```

DQ should represent such data as a typed structure value instead of as an
argument-list macro.  This preserves the meaning and type of each member and
allows a function to accept the description as one value.

```dq
struct SPad:
    fmux  : uint32
    ioblk : uint32
    gpio  : uint32
endstruct

function ConfigurePad(aps : SPad):
    // ...
endfunc
```

## Contextual aggregate literals

A brace-enclosed list is a contextual aggregate literal.  Its structure type
comes from the surrounding declaration, assignment, parameter, return type,
array element, or containing structure member.

```dq
const PAD_SD0_CLK : SPad =
    { 0x000, 0x03001A00, 0x000 + 7 }

var pad : SPad = { 1, 2, 3 }
pad = { gpio: 7, fmux: 1, ioblk: 2 }

ConfigurePad({ 1, 2, 3 })

function GetPad() -> SPad:
    return { fmux: 1, ioblk: 2, gpio: 3 }
endfunc
```

The literal does not create an anonymous tuple or anonymous structure type.  It
requires an expected structure type.

## Explicit type

When the surrounding expression does not provide a type, the normal DQ
type-call casting form supplies it:

```dq
var pad : ? = SPad({ 1, 2, 3 })
ConfigurePad(SPad({ fmux: 1, ioblk: 2, gpio: 3 }))
```

An untyped aggregate cannot independently satisfy declaration inference:

```dq
var pad : ? = { 1, 2, 3 }  // error: aggregate type cannot be inferred
```

`SPad({})` is the explicitly typed form of an entirely default-initialized
`SPad`.

## Member initialization

Members may be initialized positionally, by name, or with a mixture of the two.
A named entry uses `member: value` syntax.

```dq
// Positional: structure declaration order.
{ 0x004, 0x03001A04, 8 }

// Named: members may be written in any order.
{ gpio: 8, fmux: 0x004, ioblk: 0x03001A04 }

// Mixed: a positional value after a named member continues with the member
// following that member in declaration order.
{ fmux: 0x004, ioblk: 0x03001A04, 8 }
```

Every member must normally be initialized exactly once.  This is intentionally
stricter than C: adding a member to a structure should make existing complete
initializers fail rather than silently initialize the new member to zero.

Unknown members, duplicate members, excess positional values, incompatible
member values, and missing members are compile errors.

## Deliberately missing members

A final `?` explicitly acknowledges that some members are missing and requests
default initialization for every member not otherwise initialized.

```dq
var pad : SPad = { 1, ? }

var gpio_only : SPad = {
    gpio: 7,
    ?
}

ConfigurePad({ ioblk: 0x03001A04, ? })
```

Rules for `?` inside an aggregate:

- it is aggregate syntax, not an ordinary value expression;
- it may occur at most once and must be the final entry;
- it default-initializes all unassigned members, including members preceding a
  named member entry;
- it is an error when no member is missing.

Thus:

```dq
{ 1, 2, 3 }     // complete initializer
{ 1, ? }        // remaining members deliberately use their defaults
{ 1, 2 }        // error: a member is missing
{ 1, 2, 3, ? }  // error: nothing is missing
{ ? }           // default-initialize every member
{}              // existing all-default aggregate form
```

Default initialization is defined per member type; it does not promise that
the raw bytes or padding of the structure are all zero.  Numeric members, for
example, receive their normal numeric default.

## Constants

The same literals can initialize structure constants.  Every supplied member
initializer, and every default introduced by `?`, must be valid in a constant
expression.

```dq
const PAD_SD0_CMD : SPad = {
    fmux:  0x004,
    ioblk: 0x03001A04,
    gpio:  0x000 + 8
}
```

The resulting value is a normal typed constant and is passed as one function
argument:

```dq
ConfigurePad(PAD_SD0_CMD)
```

## Overload resolution

For a contextual aggregate argument, each overload candidate supplies its
parameter type.  A candidate is viable when the literal is a valid initializer
for that structure.  If multiple structure parameter types accept the literal
with the same rank, the call is ambiguous and an explicit cast resolves it:

```dq
ConfigurePad(SPad({ 1, 2, 3 }))
```
