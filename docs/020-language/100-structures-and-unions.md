# Structures and Unions

A **structure** keeps related values together. For example, a point has an `x`
and a `y` coordinate, and a measurement might have a reading and a timestamp.
Each named value inside a structure is a **field**.

Structure type names conventionally start with a capital `S`, as in `SPoint`.

```dq
use print

struct SPoint:
    x : int
    y : int
endstruct

func *Main():
    var point : SPoint = {x: 10, y: 20}
    var copy : SPoint = point
    copy.x = 30
    PrintLn("{} {}", [point.x, copy.x])
endfunc
```

This prints `10 30`. Assigning a structure copies its fields, so changing
`copy.x` leaves `point.x` unchanged.

A **union** lets several fields share the same storage. It is useful for binary
formats and C interfaces, and is covered after the structure examples.

## Structures

Declare fields between `struct Name:` and `endstruct`. Use a dot to read or
change a field:

```dq
struct SPoint:
    x : int
    y : int
endstruct

var point : SPoint
point.x = 10
point.y = 20
point.y += 5 // point is now {x: 10, y: 25}
var origin : SPoint = {} // both coordinates are zero
```

A structure stores its fields directly in the variable that contains it.

Structures cannot contain managed strings, dynamic arrays, object references, embedded objects,
or aggregates with managed fields. Use an [object](120-objects-and-properties.md) when you need those fields.

For short text stored inside a structure, use a fixed-storage [embedded string](070-strings-and-characters.md#embstrn-and-embstr-embedded-strings):

```dq
struct SProduct:
    name : embstr(32)
    price : float
endstruct

var product : SProduct = {}
product.name = "Bread"
product.price = 2.5
```

Each structure declaration defines a separate type: two structures with the same
fields are still different types.

### Structure Initialization

A local structure variable declared without an initializer starts uninitialized.
Reading it immediately produces a `VarNotInit` compiler error. Use `= {}` to
explicitly initialize all fields to their zero/default values:

```dq
func Example():
    var point : SPoint       // uninitialized local variable
    var origin : SPoint = {} // x and y are zero
    point = {10, 20}         // initialize before reading
endfunc
```

Global structure variables and structure fields embedded in objects are
zero-initialized by default when no initializer is supplied.

### Structure Initializer Literals

A brace literal supplies the initial field values. You can name the fields or
list values in field declaration order:

```dq
var named : SPoint = {x: 10, y: 20}
var positional : SPoint = {10, 20}
named = {y: 40, x: 30} // named fields may appear in either order
```

Use a final `?` to give all remaining fields their default values:

```dq
var point : SPoint = {x: 10, ?} // y is zero
var origin : SPoint = {}       // all fields use their defaults
```

Without `?`, supply every field exactly once. `{?}` also initializes the whole
structure to its defaults. The `?` fill marker always have to be the last.

The compiler checks and interprets the structure literals according the
expected structure type supplied by a variable or function argument.

Nested initialization literals can be used for structures containing structure fields.
Here, `start` and `finish` are both `SPoint` fields, so each inner pair of braces creates an `SPoint`:

```dq
struct SLine:
    start : SPoint
    finish : SPoint
endstruct

var line : SLine = {start: {0, 0}, finish: {10, 20}}
```

You can initialize a fixed array of structures with a single literal:

```dq
var points : [2]SPoint = [{0, 0}, {10, 20}]
```

You can mix positional and named structure entries. After a named entry, the
next positional entry fills the field following it in declaration order:

```dq
var point : SPoint = {x: 10, 20} // x is 10; y is 20
```

A structure literal is a temporary value and cannot be passed directly to a
reference parameter; put it in a variable first. Structure literals may also
initialize compile-time constants, including public constants imported from
another module. Union constants are not supported.

## Storage Size with `SizeOf()`

`SizeOf(Type)` returns a type's storage size in bytes. You can also pass a
variable name as `SizeOf(variable)`. Both forms produce a compile-time integer
constant:

```dq
const POINT_SIZE : int = SizeOf(SPoint)
var point : SPoint = {}
var bytes : int = SizeOf(point) // same as SizeOf(SPoint)
```

For a structure, the size includes all fields and any padding between or after
them. For a union, it is the size of the shared storage, including any padding.
Alignment and `[[packed]]` can affect the result; see
[Layout and External Interfaces](#layout-and-external-interfaces).

## Field Offsets with `OffsetOf()`

`OffsetOf(Type, field)` returns the number of bytes from the start of a
structure to the named field. Pass the type name and the field name without
quotes. The result is a compile-time integer constant; no structure variable
is needed:

```dq
struct SHeader [[packed]]:
    kind : uint8
    length : uint32
endstruct

const KIND_OFFSET : int = OffsetOf(SHeader, kind)     // 0
const LENGTH_OFFSET : int = OffsetOf(SHeader, length) // 1
```

Here, `length` starts one byte after the start of the structure. `OffsetOf()`
includes any padding before a field, so removing `[[packed]]` may change the
result. See [Layout and External Interfaces](#layout-and-external-interfaces)
for alignment and padding rules.

Use `OffsetOf()` when checking a binary layout or matching an external record.
`OffsetOf()` also accepts union types, where every field has offset zero.

## Structure Methods

A method is a function that belongs to a type. Use one to perform an operation
on the structure's fields:

```dq
struct SSize:
    width : int
    height : int

    func Area() -> int:
        return width * height
    endfunc
endstruct

var size : SSize = {width: 4, height: 3}
var area : int = size.Area() // 12
```

Inside the method, fields are available by name. Methods can also be declared
inside the structure and defined later with a qualified name, such as
`func SSize.Area() -> int:`.

Structure methods do not have the virtual dispatch described for objects.

## Structure Inheritance

A structure can inherit fields and methods from one base structure:

```dq

struct SPoint:
    x : int
    y : int
endstruct

struct SPoint3D(SPoint):
    z : int
endstruct

var point : SPoint3D = {x: 10, y: 20, z: 30}
```

When inintializing a derived structure base fields come first, followed by derived fields.

## Storage Unions

A union provides several ways to interpret the same bytes. Declare it with
`union Name:` and `endunion`:

```dq
union UWord:
    raw : uint32
    bytes : [4]byte
endunion

var word : UWord = {raw: 42}
var number : uint32 = word.raw // 42
```

All fields begin at the same address. Writing `word.raw` also changes the bytes
seen through `word.bytes`. Which byte comes first depends on the target's byte
order.

Initialize a union with exactly one field, or use `{}` to zero all its storage.
Storage beyond the chosen field receives default initialization. Assignment
copies the complete union storage.

A union must contain at least one field. Unions do not support inheritance,
methods, properties, field initializers, automatic conversion to a field's type,
or union-valued constants. Each union declaration defines a distinct type.

### Nested Union Fields

A structure or object can contain a named union field. For example, a
measurement may store an integer reading or a floating-point reading:

```dq
struct SReading:
    kind : int
    union data:
        integer : int32
        real : float32
    endunion
endstruct

var reading : SReading = {kind: 1, data.integer: 42}
var value : int32 = reading.data.integer // 42
```

Here, the program can use `kind` to remember which form it stored. DQ does not
maintain this field automatically. `data.integer: 42` is a shorter spelling of
`data: {integer: 42}`. Select only one alternative for each union field in an
initializer; alternatives may themselves contain nested structures or unions.

## Structure Field Alignment and Padding

Fields are stored in declaration order. Each field normally starts at an offset
that satisfies its type's **alignment requirement**. For example, an `uint32`
field starts at an offset divisible by four. The compiler inserts
unused bytes, called **padding**, between fields when needed.

A structure's alignment is the largest alignment required by any of its fields
or its base structure. For example, if that alignment is four bytes, the
structure must start at an address divisible by four.

Its size must also be a multiple of four in that example. If the fields and
the padding between them take 10 bytes, the compiler adds 2 unused bytes at the
end, making the size 12 bytes. In an array, the next structure then starts 12
bytes later, again at an address divisible by four.

### Packed Structures

Put `[[packed]]` after the structure name, before the colon, to store fields
consecutively without adding padding between them or after the last field.
A packed structure has one-byte alignment:

```dq
struct SRecord:
    tag : byte
    count : uint32
    code : uint16
endstruct

struct SPackedRecord [[packed]]:
    tag : byte
    count : uint32
    code : uint16
endstruct
```

On a target where `uint32` has four-byte alignment and `uint16` has two-byte
alignment, these declarations have the following layouts:

| Query | Normal structure | Packed structure |
| --- | --- | --- |
| `OffsetOf(Type, tag)` | 0 | 0 |
| `OffsetOf(Type, count)` | 4 | 1 |
| `OffsetOf(Type, code)` | 8 | 5 |
| `SizeOf(Type)` | 12 | 7 |

Packing an outer structure does not change the internal layout of a nested
structure field. That field retains its own size and any internal padding.
Packed fields may have unaligned addresses; this matters when passing their
addresses to external code or hardware.

### Explicit Field Alignment

Use `[[align(n)]]` after a field declaration to request a larger alignment in
bytes:

```dq
struct SAlignedRecord:
    tag : byte
    value : uint16 [[align(8)]]
endstruct
```

Here, `value` starts at offset `8`, the structure has eight-byte alignment, and
its size is `16` bytes. A smaller alignment request does not reduce a field's
natural alignment. In a packed structure, field alignment attributes are ignored.

A union uses the strictest alignment required by any field and is large enough
for its largest field, rounded up to that alignment. `[[packed]]` is not
supported on unions.

Use fixed-width fields and check `SizeOf` and `OffsetOf` when matching a C
record or hardware interface. These queries, member access, and address-taking
also work with unions. See [Attributes](180-attributes.md) and
[C Interoperability](200-c-interoperability.md).
