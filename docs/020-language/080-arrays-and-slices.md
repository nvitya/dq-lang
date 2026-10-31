# Arrays and Slices

An array holds a sequence of values of the same element type. A fixed array
stores a known number of elements directly. A dynamic array can grow or shrink
and allocates storage for its elements on the heap.
An array slice borrows part of an existing array without copying it.

```dq
use print

func PrintArray(caption : strslice, arr : []int):
    Print("{:<30}: [", [caption])
    for i : int = 0 count arr.length:
        if i > 0:
            Print(", ")
        endif
        Print("{}", [arr[i]])
    endfor
    PrintLn("]")
endfunc

func *Main():
    var values : [*]int = [10, 20, 30]
    PrintArray("Initial", values)
    PrintArray("Slice [1:]", values[1:])

    var alias : [*]int = values
    alias[0] = 99
    PrintArray("After alias[0] = 99", values)

    alias.Append(40)
    PrintArray("After alias.Append(40)", values)

    var copy : [*]int = values.Clone()
    PrintArray("Cloned copy", copy)

    copy[0] = 7
    PrintArray("After copy[0] = 7, copy",     copy)
    PrintArray("After copy[0] = 7, original", values)
endfunc
```

This prints:

```text
Initial                       : [10, 20, 30]
Slice [1:]                    : [20, 30]
After alias[0] = 99           : [99, 20, 30]
After alias.Append(40)        : [99, 20, 30, 40]
Cloned copy                   : [99, 20, 30, 40]
After copy[0] = 7, copy       : [7, 20, 30, 40]
After copy[0] = 7, original   : [99, 20, 30, 40]
```

Assignment shares the dynamic array; `Clone` makes an independent array.
`PrintArray` borrows each array as a slice, so the same function can print fixed
arrays, dynamic arrays, and slices.

## Array Forms

| Form | Ownership | Length | Resizable |
| --- | --- | --- | --- |
| `[N]T` | owns inline element storage | compile-time `N` | no |
| `[?]T` | owns inferred inline storage | inferred from initializer | no |
| `[]T` | borrows contiguous storage | runtime | no |
| `[*]T` | owns managed heap storage | runtime | yes |

All forms contain elements of one element type. `.length` is the number of
elements, never the number of bytes. Dynamic arrays additionally expose
`.capacity`.

## Array Properties and Methods

| Property or method | Avail.\* | Description |
| --- | --- | --- |
| [`.Append(value)`](#append-and-prepend)<br>[`.Append(values)`](#append-and-prepend) | D | Add an element or a range at the end. |
| [`.AppendSlice(values)`](#append-and-prepend) | D | Explicitly append a range of elements. |
| [`.capacity`](#capacity) | D | Number of reserved element slots. |
| [`.Clear(free_storage = false)`](#clear) | D | Remove all elements; optionally release storage. |
| [`.Clone()`](#clone) | D | Create independent array storage. |
| [`.Compact()`](#setcapacity-and-compact) | D | Reduce capacity to the current length. |
| [`.Delete(index, count = 1)`](#delete) | D | Remove a clamped range of elements. |
| [`.IndexOf(value)`](#indexof) | DFS | First matching element index, or `-1`. |
| [`.Insert(index, value)`](#insert-and-insertslice)<br>[`.Insert(index, values)`](#insert-and-insertslice) | D | Insert an element or a range before a position. |
| [`.InsertSlice(index, values)`](#insert-and-insertslice) | D | Explicitly insert a range of elements. |
| [`.length`](#length) | DFS | Current number of elements. |
| [`.Pop()`](#pop-and-popfirst) | D | Remove and return the last element. |
| [`.PopFirst()`](#pop-and-popfirst) | D | Remove and return the first element. |
| [`.Prepend(value)`](#append-and-prepend)<br>[`.Prepend(values)`](#append-and-prepend) | D | Add an element or a range at the front. |
| [`.refcount`](#reference-count) | D | Number of references sharing the array manager. |
| [`.Reserve(n)`](#reserve) | D | Ensure room for at least `n` elements. |
| [`.SetCapacity(n)`](#setcapacity-and-compact) | D | Set capacity, truncating length if necessary. |
| [`.SetLength(n)`](#setlength) | D | Grow with initialized elements or remove trailing elements. |

\* **D** = dynamic arrays (`[*]T`), **F** = fixed arrays (`[N]T` and `[?]T`),
**S** = array slices (`[]T`). `DFS` means all three forms.

## Fixed Arrays

`[N]T` is a value type. Its size is part of its type and its storage follows the
containing variable or aggregate.

```dq
var zeros : [4]int = {}
var values : [3]int = [10, 20, 30]
var inferred : [?]int = [1, 2, 3]
```

`[?]T` is valid only where an initializer supplies an array literal from which
the length can be determined. Different fixed lengths are different types.

Assignment between compatible fixed arrays copies the elements. Fixed arrays
cannot change length or capacity.

For `[N]autofree T`, whole-array assignment instead moves each owning element.
See [`autofree` Ownership Types](130-memory-management-and-ownership.md) for the
element and self-assignment rules.

## Dynamic Arrays

`[*]T` is a managed dynamic array. Assignment shares its manager and storage.
Element writes and changes of length or capacity are visible through aliases of
that manager. Rebinding one variable to a different array does not rebind the
others. Use `Clone()` when an independent copy is required; dynamic arrays do
not have string-style copy-on-write.

```dq
var values : [*]int = [1, 2, 3]
values.Append(4)
var independent : [*]int = values.Clone()
```

An empty dynamic array may be represented by just a `nil` pointer, with no heap
allocation. Nonempty dynamic array variables contain an object pointer to the
reference-counted dynamic array manager object, which separately manages the
storage for the array elements.

### Owning Elements

`[*]autofree T` still shares its dynamic-array manager on array assignment, but
each element is uniquely owned by that manager. Its single-element move methods,
range-copy restrictions, and `Pop`/`PopFirst` extraction behavior are defined in
[`autofree` Ownership Types](130-memory-management-and-ownership.md).

## Array Properties

Properties are read-only. Change dynamic-array length and capacity with the
methods described below.

### Length

`.length` is the number of elements, never the number of bytes. For a fixed
array it is the declared or inferred length; for a slice it counts only the
viewed range. An empty dynamic array has length zero, even without an allocated
manager.

```dq
var data : [4]int = [10, 20, 30, 40]
var middle : []int = data[1:3]
var total : uint = data.length   // 4
var count : uint = middle.length // 2
```

### Capacity

`.capacity` is the number of element slots allocated for a dynamic array. It
is never less than `.length`. Reserving extra slots allows later growth without
reallocating until that capacity is exceeded. A dynamic array without an
allocated manager has capacity zero.

```dq
var values : [*]int = [1, 2, 3]
values.Reserve(100)
var count : uint = values.length          // 3
var room : bool = values.capacity >= 100 // true
```

### Reference Count

`.refcount` reports how many references share the dynamic-array manager.
Assignment shares that manager; a borrowed slice does not retain it. A dynamic
array without an allocated manager has reference count zero. Temporary values
can also retain references.

```dq
var values : [*]int = [1, 2, 3]
var alias : [*]int = values
var shared : int = values.refcount // 2
```

## Dynamic Array Methods

These methods modify the shared array, so changes are visible through its
aliases. `.Clone()` instead returns an independent array; `.Pop()` and
`.PopFirst()` return the removed element. The other methods return no value.

Methods accepting a source range support fixed arrays, dynamic arrays, and
slices with compatible element types. The source may overlap the destination,
including when growth reallocates storage: the runtime preserves the logical
source values before overwriting overlapping storage. Structural changes can
invalidate existing slices and element pointers; see [Slices](#slices).
Arrays of `autofree` elements do not support range-copy methods; see
[Owning Elements](#owning-elements) for their move rules.

### .Append and .Prepend

`.Append(value)` adds one element at the end; `.Prepend(value)` adds it at the
front. Both also accept a range of elements, preserving their order.
`.AppendSlice(values)` explicitly selects the range form, which is useful when
the element type is itself an array.

```dq
var values : [*]int = [20, 30]
values.Append(40)            // [20, 30, 40]
values.Prepend([0, 10])      // [0, 10, 20, 30, 40]
values.AppendSlice([50, 60]) // [0, 10, 20, 30, 40, 50, 60]
```

### .Insert and .InsertSlice

`.Insert(index, value)` inserts one element before `index`; the range form
inserts all source elements in order. `.InsertSlice(index, values)` explicitly
selects the range form. The insertion position is clamped to `[0, length]`:
positions before the beginning prepend, and positions past the end append.
`$end` selects the current end position.

```dq
var values : [*]int = [10, 40]
values.Insert(1, [20, 30]) // [10, 20, 30, 40]
values.Insert($end, 50)    // [10, 20, 30, 40, 50]
```

### .Delete

`.Delete(index, count = 1)` removes elements starting at a clamped position.
The count is limited to the remaining elements. A non-positive count or a
position at or beyond the end removes nothing. Remaining elements keep their
order.

```dq
var values : [*]int = [10, 20, 30, 40]
values.Delete(1, 2) // [10, 40]
values.Delete(1)    // [10]
```

### .Pop and .PopFirst

`.Pop()` removes and returns the last element; `.PopFirst()` removes and
returns the first element. Both require a nonempty array and raise a runtime
error when it is empty.

```dq
var values : [*]int = [10, 20, 30]
var last : int = values.Pop()       // 30; values is [10, 20]
var first : int = values.PopFirst() // 10; values is [20]
```

### .SetLength

`.SetLength(n)` changes the number of elements. Growing initializes the new
elements and increases capacity when needed. Shrinking removes trailing
elements while retaining capacity.

```dq
var values : [*]int = [1, 2]
values.SetLength(4) // [1, 2, 0, 0]
values.SetLength(1) // [1]
```

### .SetCapacity and .Compact

`.SetCapacity(n)` sets the number of allocated element slots. If `n` is smaller
than the current length, trailing elements are removed to fit. Increasing
capacity leaves length unchanged. `.Compact()` reduces capacity to the current
length.

```dq
var values : [*]int = [10, 20, 30]
values.SetCapacity(2) // [10, 20]; length and capacity are 2
values.Reserve(100)
values.Compact()      // [10, 20]; capacity is 2 again
```

### .Reserve

`.Reserve(n)` ensures capacity for at least `n` elements without changing the
length. It never reduces capacity. Use it before repeated appends when the
expected size is known.

```dq
var values : [*]int
values.Reserve(100) // length is 0; capacity is at least 100
values.Append(42)   // uses the reserved storage
```

### .Clear

`.Clear()` removes all elements while retaining capacity for reuse.
`.Clear(true)` also releases the element storage. Both leave the array empty.

```dq
var values : [*]int = [10, 20, 30]
values.Clear()     // length is 0; capacity is retained
values.Clear(true) // length and capacity are 0
```

### .Clone

`.Clone()` returns an array with independent element storage. Elements are
copied according to their types: cloning object references still refers to
the same objects, and cloning nested dynamic arrays shares their row managers.
Cloning an array of `autofree` elements is not supported; see
[Owning Elements](#owning-elements).

```dq
var values : [*]int = [10, 20, 30]
var copy : [*]int = values.Clone()
copy[0] = 99 // values remains [10, 20, 30]
```

## Indexing

`array[index]` selects one element. The index must be in the half-open range
`[0, length)`. An invalid access raises a runtime range error; ordinary indexing
does not clamp.

Negative indices count backward from the end (`-1` is the last element).
`$last` means `length - 1`; `$end` means `length`. Operations that require an
existing element reject the one-past-end position; insertion may accept it to
append.

## Slices

`[]T` is a non-owning pointer-and-length view. Creating a slice does not copy or
retain the owner. Mutating an element through a mutable slice mutates the viewed
storage. A slice cannot resize its owner.

A slice expression works on fixed arrays, dynamic arrays, and existing slices.
`array[start:end]` uses a half-open interval: it contains `start` and excludes
`end`. `array[start::end]` is inclusive of both endpoints. Either bound may be
omitted. `$last` and `$end` have the same meanings as in indexing.

```dq
var data : [*]int = [10, 20, 30, 40]
var all : []int = data[:]          // [10, 20, 30, 40]
var middle : []int = data[1:3]     // [20, 30]
var tail : []int = data[1:]        // [20, 30, 40]
var head : []int = data[:3]        // [10, 20, 30]
var to_end : []int = data[1:$end]  // [20, 30, 40]
var inclusive : []int = data[1::3] // [20, 30, 40]
middle[0] = 99                    // data is now [10, 99, 30, 40]
```

Slice bounds are forgiving: each bound is clamped to `[0, length]`, and a
normalized end before the start produces an empty slice. The inclusive form is
normalized as a half-open end one position after the requested inclusive
endpoint, without integer overflow.

A slice remains valid only while its owner remains alive and the viewed storage
does not move or disappear. Structural changes to a dynamic array can invalidate
all slices and element pointers into it, even when the change appears to affect
a different part of the array.

Array values may convert to a full compatible slice in view contexts, including
function parameters. A slice may not be made from temporary storage whose
lifetime would end before the slice.

## Function Parameters

Use `[]T` for a borrowed mutable view. Apply the appropriate reference parameter
mode when the parameter itself must be rebound or when read/write restrictions
are required. A dynamic-array parameter must use a reference mode, such as
`values : ref [*]int`; passing `[*]T` by value is not supported. A writable
reference lets a function resize the array or replace the caller's array variable.

## Iteration

An inferred `for` variable is an ordinary, non-owning value, even when an array
stores `autofree` elements:

```dq
for item :? in items:
    Use(item)
endfor
```

Use `ref` when the loop body must access each element's storage, including to
replace an owning element in place:

```dq
for item : ref in items:
    item = new OThing()
endfor
```

A reference iteration variable has the element's exact type and writes through
to the array slot. Do not resize, insert into, delete from, or otherwise
structurally modify a dynamic array while its reference iteration variable is in
use: such operations can invalidate the current element address.

## Searching and Comparison

### .IndexOf

`IndexOf(value)` is available on fixed arrays, dynamic arrays, and slices. It
returns the first matching element's index, or `-1` when absent. Its result is
relative to the receiver: a match at the first element of a slice has index `0`.
The runtime compares stored element representations. Object references compare
addresses, and structure padding or distinct floating-point bit patterns can
matter; this is not an overloaded deep-equality operation.

```dq
var data : [4]int = [10, 20, 30, 40]
var middle : []int = data[1:3]
var position : int = data.IndexOf(30)   // 2
var relative : int = middle.IndexOf(30) // 1
var absent : int = middle.IndexOf(40)   // -1
```

### Equality

Dynamic arrays support `== []` and `<> []` as empty/nonempty tests. Array equality
is not a general implicit operation. Compare lengths and elements explicitly
when sequence equality is needed.

## Multidimensional Arrays

Nest array types to store arrays of arrays:

```dq
var matrix : [2][3]int = [[1, 2, 3], [4, 5, 6]]
var value : int = matrix[1][2]
```

The dimensions and element types remain part of normal type checking. A nested
dynamic array has separately managed rows; cloning an outer array does not
recursively clone every managed row.
