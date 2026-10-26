# Structures and Unions

A structure stores related fields together as one value. A union gives several
field names different interpretations of the same storage. Both have nominal
types: matching layouts do not make two different declarations interchangeable.

```dq
use print

struct SPoint:
    x : int
    y : int
endstruct

union UWord:
    raw : uint32
    bytes : [4]byte
endunion

func *Main():
    var point : SPoint = {x: 10, y: 20}
    var copy : SPoint = point
    copy.x = 30
    var word : UWord = {raw: 42}
    PrintLn("{} {} {}", [point.x, copy.x, word.raw])
endfunc
```

This prints `10 30 42`. Structure assignment copies the value; changing the
copy does not change the original structure.

## Structures

A `struct` is a value type whose storage contains its fields directly.

```dq
struct SPoint:
    x : int
    y : int
endstruct
```

Structure assignment copies the value. `{}` default-initializes all fields.
Structures may define methods and may inherit from a base structure, but do not
have object identity or virtual dispatch.

Structures cannot contain managed fields such as `str` or dynamic arrays. For
text owned by a structure, use `embstr(N)` to store a fixed buffer inline.
Borrowed `rostr` and `strslice` fields are allowed, but do not keep their text
alive. See [Embedded Strings](070-strings-and-characters.md#embstrn-and-embstr-embedded-strings).

### Structure initializer literals

A brace literal obtains its structure type from its context. Entries may be
positional, named with `field: value`, or mixed:

```dq
var point : SPoint = { 10, 20 }
point = { y: 40, x: 30 }
DrawPoint({ x: 50, y: 60 })
```

The context may be a declaration, assignment, function parameter, return type,
fixed-array element, or containing structure field. The normal cast form makes
the type explicit when necessary:

```dq
var inferred : ? = SPoint({ 10, 20 })
```

An uncast brace literal has no independent type, so `var inferred : ? = { 10,
20 }` is invalid. Structure types remain nominal: matching field layouts do not
implicitly convert one named structure type to another.

Positional entries follow source declaration order, independently of padding or
packed LLVM layout. After a named entry, the positional cursor continues at the
field following that named field. Every field must be initialized exactly once.
There is no trailing-comma form.

A final `?` explicitly default-initializes every field not otherwise assigned:

```dq
var point : SPoint = { x: 10, ? }
var default_point : SPoint = {?}
```

`?` must be final and is an error when all fields were already assigned. `{?}`
and the existing `{}` form both default-initialize the whole structure.

For derived structures, positional order is the flattened base-to-derived
declaration order. Named lookup uses normal most-derived lookup, so a hidden
base field cannot be named through the derived type; initialize it positionally
or leave it to `?`.

Structure literals are rvalues and do not bind directly to reference
parameters. Nonempty brace literals apply to structures and unions;
`{}` retains its existing default-initialization uses for supported aggregates.
They may also initialize compile-time structure constants, including public
constants imported from another module.

Nested structure fields accept contextual literals, for example
`{point: {10, 20}, count: 4}`. A union field selects one alternative with a
dotted name or a nested literal:

```dq
value = {tag: 11, value.float32_value: 3.14}
value = {tag: 22, value.bytes: [1, 2, 3, 4]}
value = {tag: 11, value: {float32_value: 3.14}}
```

`value.member: expression` is equivalent to `value: {member: expression}`.
The union field may appear only once, even if the entries select different
alternatives. A union literal initializes one alternative; its remaining
storage receives the default initialization. Selected alternatives can
themselves contain nested structure or union literals. Union constants are
not supported.


## Structure Methods and Inheritance

A structure can declare methods and inherit from one base structure:

```dq
struct SPoint:
    x : int
    y : int
    func Sum() -> int:
        return x + y
    endfunc
endstruct

struct SPoint3D(SPoint):
    z : int
endstruct
```

Methods can also be declared inside the type and defined later using a qualified
name. Structures retain value semantics and do not acquire object identity or
virtual dispatch. Inherited fields occur before derived fields in positional
initializers; named initialization uses normal most-derived member lookup.

## Storage Unions

Declare a named union with `union Name: ... endunion`. All its members start at
offset zero. The size is the largest member size rounded to the union alignment;
the alignment is the maximum required by its members. A containing structure
applies its ordinary padding and packing rules.

Union assignment copies the complete storage value. `{}` initializes all its
bytes to zero. A nonempty initializer chooses exactly one alternative; the
remaining storage is default-initialized. `SizeOf`, `OffsetOf`, member access,
and address-taking work for union storage.

There is no active-member tag or runtime check. After writing one member,
reading another reinterprets the same bytes. Ensure they form a valid value for
the type read. Byte ordering follows the target architecture.

### Nested Union Fields

A named union field can be declared directly inside a structure or object:

```dq
struct SValue:
    tag : int
    union data:
        integer : int32
        real : float32
        bytes : [4]byte
    endunion
endstruct

var value : SValue = {tag: 1, data.integer: 42}
```

Members remain qualified, for example `value.data.integer`. The union field's
visibility applies as a whole. A dotted initializer is equivalent to the nested
form `data: {integer: 42}`. Specifying two alternatives of the same union field
in one initializer is an error.

### Permitted Members and Restrictions

Unions hold storage with trivial lifetime: scalar numbers, characters, enums,
pointers, function references, and fixed arrays/structures/unions composed of
permitted members. Managed strings, dynamic arrays, object references, embedded
objects, and aggregates containing managed fields are rejected.

Unions must not be empty. They do not support inheritance, methods, properties,
per-member initializers, implicit conversion to an alternative, or union-valued
constants. Separate union types remain distinct even with identical fields.

## Layout and External Interfaces

Ordinary structures include alignment padding. `[[packed]]` requests packed
layout; `[[align(n)]]` requests alignment on supported storage declarations.
Packing can make fields unaligned, which matters when passing their addresses
to external code or hardware.

Use fixed-width fields and check `SizeOf` and `OffsetOf` when matching a C
record or a hardware interface. See [Attributes](180-attributes.md) and
[C Interoperability](200-c-interoperability.md).
