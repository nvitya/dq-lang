# Memory Management and Ownership

Most local values in DQ clean themselves up when you leave their scope. Strings
and dynamic arrays manage their storage, and embedded objects created with `<-`
are destroyed automatically. Objects created with `new` need someone to release
them: either your code calls `delete`, or an **owner** does it automatically.

Use `autofree` to make a variable own a heap object:

```dq
use print

object OTask:
    name : str

    func *Create(aname : strslice):
        name = aname
    endfunc

    func *Destroy():
        @.PrintLn("Finished: {}", [name])
    endfunc
endobject

func *Main():
    var task = new autofree OTask("download")
    PrintLn("Working on {}", [task.name])
endfunc
```

This prints `Working on download` followed by `Finished: download`. The owner
releases the object when `Main` ends. Cleanup also happens on an early `return`
or when an exception leaves the scope.

## Choose a Storage Form

**Lifetime** means how long a value's storage exists. **Ownership** means who
is responsible for releasing it. Choose a form that fits how you use the value:

| Form | Lifetime and responsibility |
| --- | --- |
| Number, fixed array, structure, fixed-storage embedded string | Lives directly in its containing variable or object. |
| `str` | Manages its text storage; changing a shared string makes an independent value. |
| `[*]T` | Manages shared array storage; aliases see changes to the same array. |
| `var task <- OTask(...)` | Embedded object; lives with its scope or containing object. |
| `new OTask(...)` | Heap object; needs an explicit `delete` or an owner. |
| `autofree OTask`, `autofree ^T`, `autofree pointer` | Owns a heap reference and releases it automatically. |
| Pointer, `rostr`, `strslice`, `[]T`, ordinary object alias | Borrows existing storage; does not keep it alive. |

A **borrow** lets you use a value owned elsewhere. Keep the original value alive
for as long as you use the borrow. For example, a slice of a local array cannot
be returned for use after that local array has disappeared.

DQ has no tracing garbage collector. Copying an ordinary object reference does
not make it an owner, and reference counting for strings and dynamic arrays
does not automatically manage every object they refer to.

## Declaration and Construction

These two declarations both create an automatically owned heap object:

```dq
var first = new autofree OTask("download")
var second : autofree OTask = new OTask("resize")
```

In the second form, ordinary `new OTask(...)` creates the object and the
`autofree` destination takes ownership of it. An owner can also hold a typed
pointer or a raw allocation:

```dq
var number : autofree ^int = new int
var buffer : autofree pointer = MemAlloc(256)
```

An owner must refer to a compatible heap allocation. Do not give it a local
variable's address, an address inside another allocation, or an external
resource handle such as a file handle.

## Cleanup

An owner releases its current value when it is overwritten or leaves its scope.
Assigning `nil` releases it early:

```dq
var task = new autofree OTask("download")
task = new OTask("resize") // destroys download before taking the new object
task = nil                 // destroys resize; task is now empty
```

For objects, cleanup calls `*Destroy` and releases the heap storage. For
`autofree ^T` and `autofree pointer`, it releases the allocation without calling
an object destructor. A `nil` owner has nothing to release.

`delete` also works on an owner and leaves it `nil`:

```dq
var task = new autofree OTask("download")
delete task // task is now nil; leaving the scope will not delete it again
```

Local owners start as `nil`, and local values are cleaned up in reverse
construction order. Managed values, embedded objects, and owners receive cleanup
on normal block exit, `return`, loop transfers, and exception propagation.
Owned object fields are cleaned up in reverse declaration order when their
containing object is destroyed.

Global owners start as `nil`. Replacing them releases their old values, but DQ
does not automatically destroy globals at program shutdown. Release global
resources explicitly.

## Assignment and Moves

Only one owner should be responsible for a given allocation. Assigning one
owner to another **moves** that responsibility and clears the source:

```dq
var first = new autofree OTask("download")
var second : autofree OTask = first
// first is now nil; second owns the download task
```

Assigning an owner to an ordinary reference instead **borrows** the object:

```dq
var owner = new autofree OTask("download")
var borrowed : OTask = owner
borrowed.name = "resize" // changes the same object; owner still owns it
```

The borrow becomes invalid when the owner releases the object. Do not delete it
separately or give it to another owner.

| Source | Assign to `autofree T` | Assign to ordinary `T` |
| --- | --- | --- |
| Ordinary `T` | Take ownership; the source remains an alias. | Copy the reference. |
| `autofree T` variable | Move ownership; set the source to `nil`. | Borrow; the source stays owner. |

Replacing an owner first releases its previous value. Assigning an owner to
itself leaves it unchanged.

## Functions

An ordinary object or pointer parameter borrows its argument, including when
the caller passes an owner. The call does not clear or transfer the owner:

```dq
func Rename(task : OTask, name : strslice):
    task.name = name
endfunc

var task = new autofree OTask("download")
Rename(task, "resize") // task still owns the renamed object
```

Use `ref autofree T` when the function needs to replace the caller's owner:

```dq
func ReplaceTask(task : ref autofree OTask):
    task = new OTask("save") // releases the caller's previous task
endfunc
```

A `ref autofree T` parameter requires an owning variable; a `ref T` parameter
requires ordinary reference storage. Their ownership qualifications must match.
To return a new object, use an ordinary object result and let the caller assign
it to an owner. `autofree` return types are not supported.

## Arrays

An array can own its objects by using `autofree` as the element type:

```dq
var tasks : [*]autofree OTask
tasks.Append(new OTask("download"))
tasks.Append(new OTask("resize"))
tasks.Delete(0) // destroys download; resize remains in the array
```

Replacing or removing an element destroys its previous object. New empty
elements are `nil`. Assigning a dynamic array to another variable still shares
the same array; its remaining objects are destroyed when the last reference to
that array is released.

Single-element `.Append()`, `.Prepend()`, and `.Insert()` take ownership of an
ordinary source or move an owning source. `.Pop()` and `.PopFirst()` remove
an object without destroying it and return an ordinary reference. Give the
result an owner or delete it explicitly:

```dq
var tasks : [*]autofree OTask
tasks.Append(new OTask("download"))
var next : autofree OTask = tasks.Pop() // next now owns the removed task
```

Use `for item : ref in tasks` to replace array elements in place. The reference
keeps the element's ownership, so replacement frees the previous object.
See [Dynamic Array `autofree` Elements](080-arrays-and-slices.md#dynamic-array-autofree-elements)
for more examples.

### Array Copying Restrictions

Assigning a fixed array of owners moves every element: each destination's
previous value is released, and each source element becomes `nil`.
Self-assignment leaves the array unchanged.

Dynamic arrays of owners cannot use `.Clone()` or copy a range with
`.Append()`, `.Prepend()`, `.Insert()`, or the slice variants. Copying those
objects into separate owning elements would duplicate ownership. Borrowed
`[]autofree T` views can still be indexed, iterated, and passed to read-only
APIs. Single-element insertion can move an element from the same array even
when the operation moves the array's storage.

## Explicit Allocation and Release

For manual lifetime management, use `new` and `delete`:

```dq
var task : OTask = new OTask("download")
// use task
delete task = nil
```

`delete task = nil` clears this ordinary variable as well as destroying the
object. Other aliases are left invalid. Never delete fixed storage created
with `<-`.

The implicit runtime also provides `MemAlloc`, `MemFree`, and `MemFreeRef` for
raw storage. Match each allocation to the correct release operation. External
resources often have their own close or release function and cannot be managed
by an `autofree` pointer alone.

## Placement Restrictions and Type Inference

`autofree` can qualify an object type, a typed pointer, or `pointer`. It can be
used for variables, object fields, fixed- or dynamic-array elements, and `ref`
parameters. It cannot be used directly on numbers, booleans, characters,
structures, unions, function references, or whole array types.

It is also not supported on embedded `<-` fields, structure or union fields,
properties or their indices, function results, or `refin` and `refout`
parameters. Type aliases may name owning types, with the same placement rules.

Bare `:?` inference produces an ordinary, non-owning type. For
`new autofree T`, omit `:?` as in the opening example, or explicitly use
`autofree ?` for ownership-preserving inference. The same distinction applies
to inferred `for ... in` variables.

## Managed Values and Scope Cleanup

Cleanup does not turn every copy into an independent value. A copied structure
copies its fields according to their types; references in allowed fields still
refer to the original storage. Slices, raw pointers, borrowed text, and ordinary
object references must not outlive the values they refer to.

String and dynamic-array storage uses reference counting. Cycles of ordinary
object references do not become automatically owned as a result. Plan an owner
and a shutdown path for resources that outlive a local scope.
