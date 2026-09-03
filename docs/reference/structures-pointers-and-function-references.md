# Structures, Pointers, and Function References

This page collects lower-level value and callable types. Introductory examples
are in [Types](../language/types.md), [Memory and Pointers](../language/memory-and-pointers.md),
and [Functions](../language/functions.md).

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
parameters. Nonempty brace literals apply to structures, not objects or unions;
`{}` retains its existing default-initialization uses for supported aggregates.
They may also initialize compile-time structure constants, including public
constants imported from another module.

## Typed Pointers

`^T` points to storage containing `T`. `%value` obtains an address and `pointer^`
dereferences a typed pointer.

```dq
var value : int = 10
var p : ^int = %value
p^ = 11
```

`nil` is compatible with pointer types. Dereferencing nil or an invalid,
misaligned, expired, or otherwise unsuitable address is a runtime memory error
or undefined at an external ABI boundary.

## Pointer Member Access

A pointer to a structure is implicitly dereferenced for member access.

```dq
var point : SPoint = {}
var pp : ^SPoint = %point
pp.x = 10       // equivalent target to pp^.x
```

This convenience does not make the pointer own or extend the lifetime of the
structure.

## Pointer Arithmetic and Indexing

Adding or subtracting an integer moves by elements of the pointed-to type.
`p[offset]` returns the pointer at that offset; unlike C indexing, it does not
dereference. Write `p[offset]^` to access the element. Subtracting compatible
pointers produces an element distance.

Pointer arithmetic is valid only within an allocation or its one-past-end
position. The one-past-end pointer may be compared or subtracted but not
dereferenced.

## Generic Pointers

`pointer` carries an address without a pointee type. It must be explicitly cast
to a suitable typed pointer before dereference or scaled arithmetic. No runtime
type or lifetime information is attached.

## Function Reference Types

A function reference type records a signature.

```dq
type FTransform = function(value : int) -> int

function Double(value : int) -> int:
    return value * 2
endfunc

var transform : FTransform = Double
```

A compatible function must have matching parameter types and modes, return type,
calling convention, and method binding form. Function references can be compared
with `nil`.

An `of object` function reference also stores an object instance:

```dq
type FHandler = function(message : strview) of object
```

Assigning a virtual method resolves the implementation for that instance.
Calling the reference later passes the stored receiver. Its lifetime is not
automatically extended beyond the normal object-reference rules.

## External Function Pointers

Explicit casts can bridge compatible C function pointers and simple DQ function
references. The programmer is responsible for matching the actual ABI,
including calling convention, argument layout, variadic rules, and return type.
