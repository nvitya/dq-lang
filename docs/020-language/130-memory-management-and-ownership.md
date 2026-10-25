# Memory Management and Ownership

Lifetime is the period during which storage exists. Ownership identifies who
must release it. DQ has no tracing garbage collector: ordinary objects and raw
allocations need explicit ownership, while strings and dynamic arrays manage
their backing storage and `autofree` provides scoped cleanup for heap references.

```dq
use print

object OResource:
    func *Destroy():
        @.PrintLn("resource destroyed")
    endfunc
endobject

func *Main():
    var first = new autofree OResource()
    var second : autofree OResource = first
    PrintLn("moved={}", [first == nil])
endfunc
```

The first owner becomes `nil` after the move. The second owner destroys the
resource at scope exit, including when the scope is left by an exception.

## Choose a Storage Form

| Form | Lifetime and responsibility |
| --- | --- |
| Scalar, fixed array, structure, bounded string | Stored directly in the containing variable or aggregate. |
| `str` | Managed string value; sharing detaches on mutation. |
| `[*]T` | Shared managed array; aliases observe mutations. |
| `var object <- OType(...)` | Fixed object storage; follows its containing scope/aggregate. |
| Ordinary `new OType(...)` | Heap allocation requiring explicit destruction or transfer to an owner. |
| `autofree OType`, `autofree ^T`, `autofree pointer` | Storage that owns and releases a heap reference. |
| Pointer, `rostr`, `strslice`, `[]T`, ordinary object alias | Borrows existing storage; does not by itself keep it alive. |

Copying a structure copies its fields according to their own types; it is not a
recursive copy of every object or managed array they reference. Returning or
retaining a borrow into a local is invalid even if the compiler accepts a
particular low-level conversion.

## Explicit Allocation and Release

`new` allocates an object or typed storage. `delete` destroys a heap object (if
applicable) and releases the allocation. Use `delete value = nil` when an ordinary
variable must also be cleared. Other aliases remain invalid after release.
Never delete fixed object storage created with `<-`.

The implicit runtime also provides `MemAlloc`, `MemFree`, and `MemFreeRef` for
raw storage. Pair allocation and release correctly; an `autofree` pointer must
refer to an allocation compatible with the runtime's memory release operation,
not a stack address, interior address, or arbitrary external resource handle.

## Managed Values and Scope Cleanup

Strings, dynamic arrays, fixed objects, and owner variables run their required
cleanup on normal block exit, `return`, loop transfers, and exception unwinding.
Locally owned values are cleaned up in reverse construction order. Borrowed
references and views must not be used after that cleanup.

Reference counting of strings and dynamic arrays does not make arbitrary object
reference graphs automatically owned. Global/module shutdown is not a general
automatic destruction pass; plan global resource shutdown explicitly.

## Declaration and Construction

`autofree` may decorate an object type, a typed pointer type, or `pointer`:

```dq
var object_owner : autofree OThing = new OThing()
var buffer_owner : autofree ^byte = ^byte(MemAlloc(256))
var generic_owner : autofree pointer = MemAlloc(256)
```

It cannot decorate a numeric/Boolean/character primitive, structure, union, function-reference,
or array type directly. It may instead be the element type of a fixed or dynamic
array:

```dq
var fixed : [2]autofree OThing
var dynamic : [*]autofree OThing
```

Use `new autofree T` to declare an owning variable directly:

```dq
var owner = new autofree OThing()
```

Ordinary `new T` still produces `T`. Assigning that normal result to an
`autofree T` destination claims it.

Bare `:?` inference always produces the normal, non-owning type. It cannot be
combined with `new autofree T`; omit `:?` as above, or write `autofree ?` when
the destination's ownership must be explicit. The same rule applies to inferred
`for ... in` variables.

Type aliases may name an `autofree` type, but all placement restrictions still
apply where the alias is used.

## Cleanup

When an `autofree` value is destroyed or overwritten, its current value is
released if it is non-`nil`:

- `autofree O` calls `O.*Destroy()` and then releases the object's heap storage.
- `autofree ^T` and `autofree pointer` release the pointed-to allocation without
  an object destructor call.

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

Use `for item : ref in owners` to replace an owning array element in place. The
reference preserves the element's ownership qualification, so assigning a new
value cleans up the previous element and leaves the array as the owner.

`Pop()` and `PopFirst()` are ownership extraction operations for an
`[*]autofree T`: they remove an element without destroying it and return normal
`T`. The caller must explicitly `delete` the result or assign it to an
`autofree T` destination.
