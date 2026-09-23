# `autofree` Ownership Types

`autofree T` is an owning form of an object-reference or pointer type.
It has the same runtime representation as `T`, but records that its storage is
responsible for releasing the referenced heap allocation. This page defines the
ownership transfer and cleanup rules. See [Objects and Properties](objects-and-properties.md),
[Structures, Pointers, and Function References](structures-pointers-and-function-references.md),
and [Arrays and Slices](arrays-and-slices.md) for the underlying types.

## Declaration and Construction

`autofree` may decorate an object type, a typed pointer type, or `pointer`:

```dq
var object_owner : autofree OThing = new OThing()
var buffer_owner : autofree ^byte = ^byte(MemAlloc(256))
var generic_owner : autofree pointer = MemAlloc(256)
```

It cannot decorate a primitive, `pointer`, structure, union, function-reference,
or array type directly. It may instead be the element type of a fixed or dynamic
array:

```dq
var fixed : [2]autofree OThing
var dynamic : [*]autofree OThing
```

`new autofree T` allocates a `T` and gives the expression type `autofree T`, so
the ownership type can be inferred:

```dq
var owner : ? = new autofree OThing()
```

Ordinary `new T` still produces `T`. Assigning that normal result to an
`autofree T` destination claims it.

Type aliases may name an `autofree` type, but all placement restrictions still
apply where the alias is used.

## Cleanup

When an `autofree` value is destroyed or overwritten, its current value is
released if it is non-`nil`:

- `autofree O` calls `O.*Destroy()` and then releases the object's heap storage.
- `autofree ^T` releases the pointed-to allocation without a destructor call.

The owner storage is set to `nil` after cleanup. Local owners are initialized to
`nil` before their initializer is evaluated and are cleaned up on scope exit,
early control-flow exit, and exception unwinding. Object fields are initialized
the same way and are cleaned up in reverse declaration order when their object
is destroyed.

Global owners are zero-initialized. Reassignment cleans up their previous value,
but DQ does not currently run automatic global/module de-initialization at
program shutdown.

`delete` is valid for an `autofree` object or pointer and always leaves
that owner `nil`:

```dq
delete object_owner
if object_owner == nil:
    // true
endif
```

For ordinary object and pointer values, use the existing `delete value = nil`
form when the source variable must also be cleared.

## Assignment and Moves

Assignment to an `autofree T` first cleans up its previous value. The source
then determines whether the assignment claims, moves, or borrows:

| Source | Assignment to `autofree T` | Assignment to normal `T` |
| --- | --- | --- |
| normal `T` | claims the reference; the normal source remains an ordinary alias | ordinary copy/borrow |
| `autofree T` lvalue | moves the reference and clears the source | borrows/copies the reference; the source stays owner |

For example:

```dq
var first : autofree OThing = new OThing()
var second : autofree OThing = first
// first is nil; second owns the allocation
```

Assigning an owner to itself is a no-op. A normal alias whose value has been
claimed by an owner must not later be deleted or otherwise treated as an owner;
it becomes invalid once the `autofree` destination releases the allocation.

## Placement Restrictions

`autofree` is allowed for variables, object fields, fixed-array elements,
dynamic-array elements, and `ref` parameters. It is not allowed for:

- embedded (`<-`) object fields;
- structure or union members;
- properties or property indices;
- function return types;
- `refin` or `refout` parameters.

These restrictions prevent ownership cleanup from being hidden in storage forms
that do not have a unique, well-defined destruction path.

## Functions

A normal value parameter borrows both normal and `autofree` object/pointer
arguments. Passing an owner to such a parameter does not transfer or clear it.

A `ref` parameter preserves ownership qualification exactly: `ref autofree T`
requires an `autofree T` argument, while `ref T` requires normal `T` storage.
`autofree` return types, `refin autofree T`, and `refout autofree T` are rejected.

## Arrays

For `[N]autofree T`, whole-array assignment moves every element: each target
element is cleaned up, each source owner is transferred, and the source cells
are cleared. Self-assignment is a no-op.

`[*]autofree T` keeps normal dynamic-array manager sharing: assigning the array
shares its manager, and its elements are destroyed exactly once when the final
manager reference is released. Element assignment uses the normal owner rules.

Single-element `Append`, `Prepend`, and `Insert` claim a normal source or move
an `autofree` lvalue source. They also support a source element from the same
destination array, including when the operation reallocates or shifts elements.

`Clone()`, `Append`/`Prepend`/`Insert` with a range source, and the corresponding
slice insertion operations are rejected for `[*]autofree T`, because copying an
array range would duplicate ownership. Borrowed `[]autofree T` views remain
usable for indexing, iteration, and read-only APIs.

`Pop()` and `PopFirst()` are ownership extraction operations for an
`[*]autofree T`: they remove an element without destroying it and return normal
`T`. The caller must explicitly `delete` the result or assign it to an
`autofree T` destination.
