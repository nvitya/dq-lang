# Memory Management and Autofree

The earlier chapters introduced automatic string and array storage, borrowed
pointers, and the three ways to create objects. This chapter brings those rules
together and explains how **autofree variables** manage heap allocations as
values pass between functions, objects, and collections.

An **autofree type**, such as `autofree OTask` or `autofree ^SPoint`, adds automatic
cleanup to an object-reference or pointer type. A variable of that type releases
its allocation when it is replaced or leaves scope. It is also called the
allocation's **owner**: it is responsible for releasing that allocation. A
**borrow** lets other code use the allocation without taking responsibility for
its cleanup.

Keeping that responsibility clear prevents both memory leaks (storage that is
never released) and dangling references (references to storage that no longer
exists).

For object construction and destructors, see
[Creating and Destroying Objects](120-objects-and-properties.md#creating-and-destroying-objects).
For the lifetime of an address, see
[Allocation and Lifetime](110-pointers.md#allocation-and-lifetime).

## Choosing Storage and Cleanup

**Lifetime** means how long a value's storage exists. **Ownership** means who
is responsible for releasing it. Choose a form that fits how you use the value:

| Form | Lifetime and responsibility |
| --- | --- |
| Number, fixed array, structure, fixed-storage embedded string | Lives directly in its containing variable or object. |
| `str` | Manages its text storage; changing a shared string makes an independent value. |
| `[*]T` | Manages shared array storage; aliases see changes to the same array. |
| `var task <- OTask(...)` | Embedded object; lives with its scope or containing object. |
| `new OTask(...)` | Heap object; needs an explicit `delete` or an autofree variable. |
| `autofree OTask`, `autofree ^T`, `autofree pointer` | Autofree types; variables release their heap allocations automatically. |
| Pointer, `rostr`, `strslice`, `[]T`, ordinary object alias | Borrows existing storage; does not keep it alive. |

Keep borrowed storage alive for as long as you use it. For example, a slice of
a local array cannot be returned for use after that local array has disappeared.

An object variable declared without `autofree` does not destroy the object
automatically. Assigning it to another variable does not change that:

```dq
var task : OTask = new OTask("download")
var another : OTask = task
```

There is one task object, and both variables let you access it. The second
declaration does not create a second object, and neither variable has an
autofree type. When both variables leave scope, the object remains allocated.
DQ does not find and release it for you.

For automatic cleanup, declare `task` with an autofree type. Otherwise, call
`delete` when you are finished with the object. After it is destroyed, you must
stop using it through either variable; having another variable refer to the
object does not keep it alive.

Strings and dynamic arrays release their own storage automatically, but an
array's storage is separate from any heap objects its elements refer to. For
example, releasing a `[*]OTask` array does not destroy its task objects. Use
`[*]autofree OTask` when the array should also destroy those objects during
cleanup.

## Explicit Allocation and Release

Allocate heap objects with `new` and raw byte buffers with `MemAlloc`. Use
`delete` to release either allocation. The allocation stays in memory until you
release it; leaving the function does not release it automatically.

This complete program demonstrates both forms. Each function releases its
allocation when its work finishes normally. `RunTask` prints the supplied mode,
but returns early if the mode is empty. `ProcessData` fills its buffer with
`fillvalue`, which must be between 0 and 255 because each byte holds a value in
that range. The early-return branches **deliberately contain a mistake**: they
return before reaching the cleanup.

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

func RunTask(amode : rostr):
    var task : OTask = new OTask("download")
    if amode == "":
        PrintLn("Task mode must not be empty")
        return // mistake: skips delete, leaving the task allocated
    endif

    PrintLn("Working on {} in {} mode", [task.name, amode])
    delete task
endfunc

func ProcessData(fillvalue : int):
    var buffer : ^byte = ^byte(MemAlloc(256))
    if buffer == nil:
        PrintLn("Could not allocate buffer")
        return // no allocation was made, so there is nothing to release
    endif
    if fillvalue < 0 or fillvalue > 255:
        PrintLn("Invalid fill value: {}", [fillvalue])
        return // mistake: skips delete, leaving the buffer allocated
    endif

    MemSet(buffer, fillvalue, 256)
    PrintLn("First byte: {}", [buffer[0]^])
    delete buffer
    PrintLn("Buffer released")
endfunc

func *Main():
    PrintLn("-- normal completion")
    RunTask("fast")
    ProcessData(42)

    PrintLn("-- early return")
    RunTask("")
    ProcessData(300)
    PrintLn("Back in Main")
endfunc
```

Output when the allocations succeed:

```text
-- normal completion
Working on download in fast mode
Finished: download
First byte: 42
Buffer released
-- early return
Task mode must not be empty
Invalid fill value: 300
Back in Main
```

On the normal path, `delete task` calls the task's destructor and releases its
storage. `delete buffer` releases the byte buffer; there is no object destructor
to call for it. These variables are not used again before their functions end,
so there is no need to clear them. Other variables that refer to the same
allocation must no longer be used to access it either. Never delete an embedded
object created with `<-`.

When the mode is empty or the fill value is outside 0 to 255, `return`
immediately leaves the function. It skips all remaining statements, including
`delete`. For example, `ProcessData(300)` allocates its buffer before
checking the value, then returns without freeing it. Notice that the second
pair of calls prints neither `Finished: download` nor `Buffer released`. Both
functions return to `Main`, but the task object and the 256-byte buffer from
those calls are still allocated.

Their local variables disappear when the functions return, so the program no
longer has a way to reach those allocations and release them. This is a
**memory leak**. Repeating these calls would leave more memory allocated each
time, even though no work was completed.

You could add the missing release before each early `return`. However,
every additional exit path needs the same care, including exceptions. The next
section introduces autofree variables, which release their allocations on both
normal completion and early return, without a separate cleanup call on each
path.

The implicit runtime also provides `MemFree` for releasing raw storage and
`MemFreeRef` for releasing it and clearing a generic `pointer` variable in one
call. External resources often have their own close or release function; match
each resource to the operation its API requires.

### Reusing a Variable After `delete`

For a variable without `autofree`, `delete buffer` releases the allocation but
leaves the old address in the variable. That address is no longer valid. If the
variable stays in scope, use `delete buffer = nil` to release the allocation and
set the variable to `nil` in one statement:

```dq
var buffer : ^byte = ^byte(MemAlloc(256))
// use the buffer

delete buffer = nil // release the storage and mark the variable as empty

// Later, allocate a new buffer if there is none.
if buffer == nil:
    buffer = ^byte(MemAlloc(512))
endif
// use the new buffer

delete buffer // finished with this variable
```

Here, `nil` tells later code that the variable currently refers to no buffer.
Without clearing it, `buffer == nil` would not detect that the first allocation
has been released. The same applies to an object variable without `autofree`.
Clearing one variable does not clear other variables that refer to the released
allocation.

You can also assign a new allocation directly after `delete`; setting `nil`
first is not required for reassignment. It is useful when later code needs to
check whether the variable currently holds an allocation. Assigning `nil` alone
to a variable without `autofree` does not release its allocation.

## `autofree` Types and Variables

### Declaring Autofree Variables

Use `autofree` for a heap allocation that should be released automatically.
This complete program creates two task objects in `Main` and allocates a
structure and a byte buffer in `ProcessData`. The task destructor prints a
message so you can see when object cleanup happens:

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

struct SPoint:
    x : int
    y : int
endstruct

func ProcessData(fillvalue : int):
    var point : autofree ^SPoint = new SPoint
    point.x = 10
    point.y = 20

    var buffer : autofree ^byte = ^byte(MemAlloc(256))
    if buffer == nil:
        PrintLn("Could not allocate buffer")
        return
    endif
    if fillvalue < 0 or fillvalue > 255:
        PrintLn("Invalid fill value: {}", [fillvalue])
        return
    endif
    MemSet(buffer, fillvalue, 256)

    PrintLn("Point: ({}, {})", [point.x, point.y])
    PrintLn("Buffer filled with {}", [fillvalue])
    PrintLn("Leaving ProcessData")

    // no manual free is required for point and buffer
endfunc

func *Main():
    var first = new autofree OTask("download")
    var second : autofree OTask = new OTask("resize")
    PrintLn("Tasks: {}, {}", [first.name, second.name])

    ProcessData(42)
    PrintLn("Back in Main")
endfunc
```

Output:

```text
Tasks: download, resize
Point: (10, 20)
Buffer filled with 42
Leaving ProcessData
Back in Main
Finished: resize
Finished: download
```

`first` uses the `new autofree` shorthand. `second` declares its autofree type
explicitly and assigns a newly created object to it. Both forms arrange for the
object to be destroyed automatically.

`point` has type `autofree ^SPoint` and releases the structure's heap storage
automatically. `buffer` has type `autofree ^byte` and releases its 256-byte
allocation automatically. The cast `^byte(...)` gives the raw allocation a
byte-pointer type. `MemSet(buffer, fillvalue, 256)` fills all 256 bytes with the requested value.

When `ProcessData` returns, its local autofree variables release their
allocations in reverse declaration order: first `buffer`, then `point`. This
happens after `Leaving ProcessData` is printed and before `Main` prints
`Back in Main`. Releasing these non-object allocations produces no destructor
messages; their storage is already freed when execution returns to `Main`.
If `fillvalue` is outside 0 to 255, the early `return` also releases both
allocations automatically.

The task objects remain available until `Main` ends. Then `second` is destroyed
before `first`, producing `Finished: resize` and `Finished: download`.
No `delete` or `MemFree` calls are needed in either function.

An autofree variable must refer to a compatible heap allocation. Do not give it
a local variable's address, an address inside another allocation, or an external
resource handle such as a file handle.

When the initializer already determines the pointed-to type, you can use
`autofree ?` to avoid writing that type twice:

```dq
var buffer : autofree ? = ^byte(MemAlloc(256))
var point : autofree ? = new SPoint
```

The compiler infers `^byte` for `buffer` and `^SPoint` for `point`. The
`autofree` type attribute is never inferred: you must write it explicitly.
Here, `autofree ?` combines an inferred pointer type with explicitly requested
automatic cleanup. The `new autofree` shorthand used for `first` also specifies
`autofree` explicitly.

### Cleanup

An autofree variable releases its current allocation when it is overwritten or
leaves its scope. Assigning `nil` releases it early:

```dq
var task = new autofree OTask("download")
task = new OTask("resize") // destroys download before taking the new object
task = nil                 // destroys resize; task is now empty
```

For objects, cleanup calls `*Destroy` and releases the heap storage. For
`autofree ^T` and `autofree pointer`, it releases the allocation without calling
an object destructor. An autofree variable set to `nil` has nothing to release.

`delete` also works on an autofree variable and leaves it `nil`:

```dq
var task = new autofree OTask("download")
delete task // task is now nil; leaving the scope will not delete it again
```

Local autofree variables start as `nil`, and local values are cleaned up in
reverse construction order. Managed values, embedded objects, and autofree
variables receive cleanup on normal block exit, `return`, loop transfers, and
exception propagation. Autofree fields are cleaned up in reverse declaration
order when their containing object is destroyed.

Global autofree variables start as `nil`. Replacing them releases their old
values, but DQ does not automatically destroy globals at program shutdown.
Release global resources explicitly.

### Borrowing and Moving Ownership

Only one autofree variable should be responsible for a given allocation.
Assigning one autofree variable to another **moves ownership**: the destination
becomes responsible for releasing the allocation, and the source becomes `nil`:

```dq
var first = new autofree OTask("download")
var second : autofree OTask = first
// first is now nil; second owns the download task
```

Assigning an autofree variable to an ordinary reference instead **borrows** the
object:

```dq
var task = new autofree OTask("download")
var borrowed : OTask = task
borrowed.name = "resize" // changes the same object; task still handles cleanup
```

The borrow becomes invalid when the autofree variable releases the object. Do
not delete the borrowed reference separately or assign it to another autofree
variable. DQ does not track every borrowed reference and clear it when the
object is destroyed; a non-`nil` borrow can still be invalid.

Moving ownership does not move the object in memory. Existing borrows still
refer to that object, but its lifetime now depends on the destination autofree
variable. The source becomes `nil`, so check or replace it before using it again.

| Source | Assign to `autofree T` | Assign to ordinary `T` |
| --- | --- | --- |
| Ordinary `T` | Take ownership; the source remains an alias. | Copy the reference. |
| `autofree T` variable | Move ownership; set the source to `nil`. | Borrow; the source still handles cleanup. |

Replacing an autofree variable first releases its previous allocation.
Assigning an autofree variable to itself leaves it unchanged.

### Autofree Variables and Functions

An ordinary object or pointer parameter borrows its argument, including when
the caller passes an autofree variable. The call does not clear that variable
or transfer responsibility for cleanup:

```dq
func Rename(task : OTask, name : strslice):
    task.name = name
endfunc
```

Calling `Rename(task, "resize")` changes the object's name while leaving
ownership with the caller.

Use `ref autofree T` when the function needs to replace the caller's autofree
variable:

```dq
func ReplaceTask(task : ref autofree OTask):
    task = new OTask("save") // releases the caller's previous task
endfunc
```

A `ref autofree T` parameter requires an autofree variable; a `ref T` parameter
requires ordinary reference storage. Their `autofree` qualifications must match.
To return a new object, use an ordinary object result and let the caller assign
it to an autofree variable. `autofree` return types are not supported.

#### Returning a New Object

A factory is a function that creates a value for its caller. It can return a
new heap object directly:

```dq
func CreateTask(name : strslice) -> OTask:
    return new OTask(name)
endfunc

func *Main():
    var task : autofree OTask = CreateTask("download")
    Rename(task, "resize")
    ReplaceTask(task)
    PrintLn("Working on {}", [task.name])
endfunc
```

Put this code after `OTask`, `Rename`, and `ReplaceTask` from the examples above.
It prints:

```text
Finished: resize
Working on save
Finished: save
```

`Rename` borrows the object. `ReplaceTask` changes the caller's autofree variable,
destroying the renamed object before storing the new one. The final object is
destroyed when `Main` ends.

The ordinary `OTask` result type does not say whether a function returns a new
object or borrows an existing one. Document that distinction for callers. Never
return a local embedded object, or an object still owned by a local `autofree`
variable: local cleanup would destroy it before the caller could use it.

### Autofree Fields

An object can take responsibility for another object through an autofree field:

```dq
object OWorker:
    current : autofree OTask
endobject

func *Main():
    var worker <- OWorker()
    var task = new autofree OTask("download")
    worker.current = task // moves ownership into the field; task becomes nil
    PrintLn("Working on {}", [worker.current.name])
endfunc
```

With the earlier `OTask` definition and `use print`, this prints
`Working on download` followed by `Finished: download`. Destroying `worker`
automatically cleans up `current`; `OWorker` needs no custom destructor.
Use an ordinary `OTask` field for a borrowed link, such as a reference back to
a containing object. Giving two fields ownership of the same allocation would
cause both to attempt to release it.

### Arrays

An array with an autofree element type automatically releases its objects:

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
ordinary reference or move a source of an autofree type. `.Pop()` and
`.PopFirst()` remove an object without destroying it and return an ordinary
reference. Assign the result to an autofree variable or delete it explicitly:

```dq
var tasks : [*]autofree OTask
tasks.Append(new OTask("download"))
var next : autofree OTask = tasks.Pop() // next now owns the removed task
```

Use `for item : ref in tasks` to replace array elements in place. The reference
keeps the element's autofree type, so replacement frees the previous object.
See [Dynamic Array `autofree` Elements](080-arrays-and-slices.md#dynamic-array-autofree-elements)
for more examples.

#### Array Copying Restrictions

Assigning a fixed array of autofree elements moves every element: each
destination's previous value is released, and each source element becomes `nil`.
Self-assignment leaves the array unchanged.

Dynamic arrays of autofree elements cannot use `.Clone()` or copy a range with
`.Append()`, `.Prepend()`, `.Insert()`, or the slice variants. Copying those
objects into separate autofree elements would duplicate ownership. Borrowed
`[]autofree T` views can still be indexed, iterated, and passed to read-only
APIs. Single-element insertion can move an element from the same array even
when the operation moves the array's storage.

### Placement Restrictions and Type Inference

`autofree` can qualify an object type, a typed pointer, or `pointer`. It can be
used for variables, object fields, fixed- or dynamic-array elements, and `ref`
parameters. It cannot be used directly on numbers, booleans, characters,
structures, unions, function references, or whole array types.

It is also not supported on embedded `<-` fields, structure or union fields,
properties or their indices, function results, or `refin` and `refout`
parameters. Type aliases may name autofree types, with the same placement rules.

Type inference never adds the `autofree` attribute. Use `autofree ?` to infer
the underlying type while specifying automatic cleanup explicitly. Bare `:?`
produces a type without `autofree`, even when the source is an autofree variable.
For the `new autofree T` declaration shorthand, omit `:?` as in the examples
above. The same distinction applies to inferred `for ... in` variables.

#### Inference Example

```dq
var task = new autofree OTask("download")
var borrowed :? = task           // inferred OTask; task is unchanged
var next : autofree ? = task     // inferred OTask, explicit autofree; task becomes nil
```

`borrowed` still refers to the same object, now owned by `next`. Prefer an
explicit autofree type at a transfer point if it makes the responsibility easier
to see.
