# Arrays and Slices

An array holds a sequence of values of the same element type. A fixed array
stores a known number of elements directly. A dynamic array can grow, and an
array slice borrows part of an existing array without copying it.

```dq
use print

func PrintArray(arr : []int):
    Print("[")
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
    Print("Initial values: ")
    PrintArray(values)

    var alias : [*]int = values
    alias[0] = 99
    Print("After alias[0] = 99, values: ")
    PrintArray(values)

    alias.Append(40)
    Print("After alias.Append(40), values: ")
    PrintArray(values)

    var copy : [*]int = values.Clone()
    Print("Cloned copy: ")
    PrintArray(copy)

    copy[0] = 7
    Print("After copy[0] = 7, copy: ")
    PrintArray(copy)
    Print("After copy[0] = 7, values: ")
    PrintArray(values)
endfunc
```

This prints:

```text
Initial values: [10, 20, 30]
After alias[0] = 99, values: [99, 20, 30]
After alias.Append(40), values: [99, 20, 30, 40]
Cloned copy: [99, 20, 30, 40]
After copy[0] = 7, copy: [7, 20, 30, 40]
After copy[0] = 7, values: [99, 20, 30, 40]
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
See [`autofree` Ownership Types](130-memory-management-and-ownership.md) for the element and self-assignment
rules.

## Dynamic Arrays

`[*]T` is a managed dynamic array. Assignment shares its manager and storage. Element writes and changes of length
or capacity are visible through aliases of that manager. Rebinding one variable
to a different array does not rebind the others. Use `Clone()` when an independent
copy is required; dynamic arrays do not have string-style copy-on-write.

```dq
var values : [*]int = [1, 2, 3]
values.Append(4)
var independent : [*]int = values.Clone()
```

The dynamic operations include `Append`, `Prepend`, `Insert`, `Delete`, `Pop`,
`PopFirst`, `SetLength`, `SetCapacity`, `Reserve`, `Compact`, `Clear`, and
`Clone`. Operations preserve element order except where their name explicitly
removes or inserts elements.

Increasing length initializes new elements. Decreasing length removes trailing
elements. `SetCapacity(n)` truncates the length when `n` is smaller than the current
length. Capacity is never less than length. `Reserve` only grows capacity;
`Compact` reduces it to the current length; `Clear(true)` may release storage.

An empty dynamic array may be represented by just a `nil` pointer, with no heap
allocation. Non empty dynamic array variables contain an object pointer to the
reference-counted dynamic array manager object, which separately manages the
storage for the array elements.

## Slices

`[]T` is a non-owning pointer-and-length view. Creating a slice does not copy or
retain the owner. Mutating an element through a mutable slice mutates the viewed
storage.

```dq
var data : [*]int = [10, 20, 30, 40]
var middle : []int = data[1:3]
middle[0] = 99
```

A slice remains valid only while its owner remains alive and the viewed storage
does not move or disappear. Structural changes to a dynamic array can invalidate
all slices and element pointers into it, even when the change appears to affect
a different part of the array. A slice cannot resize its owner.

Array values may convert to a full compatible slice in view contexts, including
func parameters. A slice may not be made from temporary storage whose
lifetime would end before the slice.

## Indexing

`array[index]` selects one element. The index must be in the half-open range
`[0, length)`. An invalid access raises a runtime range error; ordinary indexing
does not clamp.

`$end` denotes the end position in index/slice syntax. Negative indices count backward from the end (`-1` is the last element).
`$last` means `length - 1`; `$end` means `length`. Operations that require an
existing element reject the one-past-end position; insertion may accept it to
append.

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

## Slicing

`array[start:end]` uses a half-open interval: it contains `start` and excludes
`end`. `array[start::end]` is inclusive of both endpoints. Either bound may be
omitted.

```dq
data[:]        // whole array
data[1:]       // from element 1
data[:3]       // elements 0, 1, 2
data[1:$end]   // element 1 through the last element
data[1::3]     // elements 1, 2, 3
```

For array storage, a slice expression produces a view. Slice bounds are forgiving:
each bound is clamped to `[0, length]`, and a normalized end before the start
produces an empty slice. The inclusive form is normalized as a half-open end one
position after the requested inclusive endpoint, without integer overflow.

## Aliasing and Overlap

Operations accepting elements or a source slice must behave correctly when the
source aliases the destination, including after reallocation. The runtime
preserves the logical source values before overwriting overlapping storage.

## Function Parameters

Use `[]T` for a borrowed mutable view. Apply the appropriate reference parameter
mode when the parameter itself must be rebound or when read/write restrictions
are required. A dynamic-array parameter must use a reference mode, such as
`values : ref [*]int`; passing `[*]T` by value is not supported. A writable
reference lets a function resize the array or replace the caller's array variable.

Array equality is not a general implicit operation. Compare lengths and elements
explicitly when sequence equality is needed.

## Owning Elements

`[*]autofree T` still shares its dynamic-array manager on array assignment, but
each element is uniquely owned by that manager. Its single-element move methods,
range-copy restrictions, and `Pop`/`PopFirst` extraction behavior are defined in
[`autofree` Ownership Types](130-memory-management-and-ownership.md).


## Dynamic Array Methods

| Member | Effect |
| --- | --- |
| `.length`, `.capacity` | Number of elements and reserved element slots. |
| `Append(value)` / `Append(values)` | Add an element or an array/view range. |
| `Prepend(value)` / `Prepend(values)` | Add at the front. |
| `Insert(index, value)` / `Insert(index, values)` | Insert before a normalized position. |
| `Delete(index, count = 1)` | Remove a clamped range. |
| `Pop()` / `PopFirst()` | Remove and return the last/first element. |
| `SetLength(n)` | Grow with initialized elements or remove trailing elements. |
| `SetCapacity(n)` | Set capacity, truncating length if necessary. |
| `Reserve(n)` | Ensure at least `n` slots without reducing capacity. |
| `Compact()` | Reduce capacity to the current length. |
| `Clear(free_storage = false)` | Remove elements; optionally release capacity. |
| `Clone()` | Produce independent managed array storage. |
| `IndexOf(value)` | Find the first matching element, or return `-1`. |

`IndexOf` is also available on fixed arrays and slices. Its result is relative
to the receiver: a match at the first element of a slice has index `0`.
The runtime compares stored element representations. Object references compare
addresses, and structure padding or distinct floating-point bit patterns can
matter; this is not an overloaded deep-equality operation.
`Clone` copies elements according to their types; cloning object references
still refers to the same objects. Owning element types restrict copying as
explained above.

Insertion and deletion normalize their positions and counts. Ordinary element
access still requires an existing element, and `Pop`/`PopFirst` require a
nonempty array. Dynamic arrays support `== []` and `<> []` as empty/nonempty
tests, but not general element-by-element sequence equality.

## Multidimensional Arrays

Nest array types to store arrays of arrays:

```dq
var matrix : [2][3]int = [[1, 2, 3], [4, 5, 6]]
var value : int = matrix[1][2]
```

The dimensions and element types remain part of normal type checking. A nested
dynamic array has separately managed rows; cloning an outer array does not
recursively clone every managed row.
