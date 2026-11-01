# Structures and Unions

A **structure** keeps related values together. For example, a point has an `x`
and a `y` coordinate, and a measurement might have a reading and a timestamp.
Each named value inside a structure is a **field**.

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
var point : SPoint = {x: 10, y: 20}
point.y += 5 // point is now {x: 10, y: 25}
var origin : SPoint = {} // both coordinates are zero
```

A structure stores its fields directly in the variable that contains it. Each
structure declaration defines a separate type: two structures with the same
fields are still different types.

For short text stored inside a structure, use a bounded embedded string:

```dq
struct SProduct:
    name : embstr(32)
    price : float
endstruct

var product : SProduct = {}
product.name = "Bread"
product.price = 2.5
```

Structures cannot contain managed fields such as `str` or dynamic arrays.
Use an [object](120-objects-and-properties.md) when you need those fields.
Borrowed `rostr` and `strslice` fields are allowed, but their source text must
remain alive. See [Embedded Strings](070-strings-and-characters.md#embstrn-and-embstr-embedded-strings).

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
structure to its defaults.

The surrounding declaration, assignment, parameter, or return type tells the
compiler which structure the braces describe. Nested structure fields and
fixed-array elements also provide that context:

```dq
struct SLine:
    start : SPoint
    finish : SPoint
endstruct

var line : SLine = {start: {0, 0}, finish: {10, 20}}
var points : [2]SPoint = [{0, 0}, {10, 20}]
```

If the surrounding code does not supply a type, write it explicitly:

```dq
var point : ? = SPoint({10, 20})
```

`var point : ? = {10, 20}` cannot infer a structure type from the fields alone.

## Structure Methods and Inheritance

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

A structure can inherit fields and methods from one base structure:

```dq
struct SPoint3D(SPoint):
    z : int
endstruct

var point : SPoint3D = {x: 10, y: 20, z: 30}
```

A derived structure is still a value that is copied on assignment. Structure
methods do not have the virtual dispatch described for objects.

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
copies the complete union storage. DQ does not record which field
you last wrote or check which field you read. Read a different field only when
you know those bytes form a valid value of its type.

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

## Initializer Details and Restrictions

You can mix positional and named structure entries. After a named entry, the
next positional entry fills the field following it in declaration order:

```dq
var point : SPoint = {x: 10, 20} // x is 10; y is 20
```

Field order follows the source declaration, regardless of storage padding.
For inherited fields, base fields come first, followed by derived fields.
If a derived field hides a base field with the same name, named initialization
selects the derived field; fill the base field positionally or leave it to `?`.

A fill marker `?` must be last and must have at least one remaining field to
initialize. Trailing commas are not supported.

A structure literal is a temporary value and cannot be passed directly to a
reference parameter; put it in a variable first. Structure literals may also
initialize compile-time constants, including public constants imported from
another module. Union constants are not supported.

### Permitted Members and Restrictions

Unions accept numbers, characters, enums, pointers, function references, and
fixed arrays, structures, or unions made from permitted members. They cannot
contain managed strings, dynamic arrays, object references, embedded objects,
or aggregates with managed fields.

A union must contain at least one field. Unions do not support inheritance,
methods, properties, field initializers, automatic conversion to a field's type,
or union-valued constants. Each union declaration defines a distinct type.

## Layout and External Interfaces

A structure may have unused bytes between fields to meet the target's alignment
requirements. A union is large enough for its largest field, rounded up for
alignment, and uses the strictest alignment required by any field.

`[[packed]]` requests packed layout; `[[align(n)]]` requests alignment on
supported declarations. Packing can leave fields at unaligned addresses, which
matters when passing their addresses to external code or hardware.

Use fixed-width fields and check `SizeOf` and `OffsetOf` when matching a C
record or hardware interface. These queries, member access, and address-taking
also work with unions. See [Attributes](180-attributes.md) and
[C Interoperability](200-c-interoperability.md).
