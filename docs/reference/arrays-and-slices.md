# Arrays and Slices

This page defines DQ array types and their ownership rules. See the
[Types guide](../language/types.md) for introductory examples.

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

## Dynamic Arrays

`[*]T` is a managed dynamic array. Assignment shares its manager/storage; a
structural mutation may allocate, detach, or update the shared manager according
to the runtime operation. Use `Clone()` when an independent copy is required.

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
elements. Capacity is never less than length. `Reserve` only grows capacity;
`Compact` reduces it to the current length; `Clear(true)` may release storage.

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

`$end` denotes the end position in index/slice syntax. Negative and end-relative
indices are normalized according to the operation. Operations that require an
existing element reject the one-past-end position; insertion may accept it to
append.

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
are required. Pass `[*]T` by value to share its managed value; use a reference
mode when a function must replace the caller's dynamic array variable.

Array equality is not a general implicit operation. Compare lengths and elements
explicitly when sequence equality is needed.
