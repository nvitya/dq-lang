# DQ Arrays

Status: draft
Scope: static arrays, inferred-length static arrays, array slices, dynamic/heap arrays, storage modes, runtime handling

---

## Definitions

An array is a contiguous memory block containing elements of the same element type.

DQ has four array-related type forms:

```dq
[N]T     // owning fixed-size static array with N elements
[?]T     // owning fixed-size static array, length inferred from the initializer
[]T      // mutable non-owning array slice/view, pointer + length
[*]T     // owning dynamic heap array, implemented through a refcounted manager object
```

The type forms have different ownership rules:

| Type form | Owns element storage | Length | Storage location | Can structurally resize |
|---|---:|---:|---|---:|
| `[N]T` | yes | fixed compile-time length | inline/static/local/object storage | no |
| `[?]T` | yes | inferred compile-time length | inline/static/local/object storage | no |
| `[]T` | no | runtime length | view into existing storage | no |
| `[*]T` | yes | runtime length | heap storage managed by a heap object | yes |

`[]T` is a slice type. It is a non-owning view of existing array storage. It is internally equivalent to a fat pointer:

```dq
struct hidden_slice_T:
  dataptr : ^T;
  length  : uint;
endstruct
```

A slice does not own the referenced elements, does not keep the source array alive, and does not prevent a dynamic array from reallocating. Therefore slices have pointer-like lifetime dangers.

A dynamic array `[*]T` always owns its element storage. Assigning or initializing a dynamic array from another array or slice copies the elements into dynamic-array-owned storage, except whole dynamic-array assignment from another `[*]T`, which shares the dynamic array manager by reference.

---

## Static Arrays

The length and storage of a static array are fixed.

```dq
var arr1 : [3]int;             // fixed-size array, zero-initialized
var arr2 : [3]int = [1, 2, 3]; // fixed-size array with initialization
```

The array length is part of the type:

```dq
[3]int  // different type than [4]int
```

A static array cannot be structurally resized. Calling dynamic-array methods such as `.Append()`, `.Insert()`, `.Delete()`, `.Reserve()`, `.Compact()`, or `.Clear()` on a static array is a compile-time error.

---

## Inferred-Length Static Arrays

`[?]T` declares a fixed-size static array whose length is inferred from the initializer.

```dq
var arr : [?]int = [10, 20, 30];
```

This is equivalent to:

```dq
var arr : [3]int = [10, 20, 30];
```

The initializer is mandatory:

```dq
var arr : [?]int;   // compile error: inferred-length static array requires initializer
```

An empty initializer for `[?]T` is invalid in the initial language version:

```dq
var arr : [?]int = [];  // compile error: cannot infer a non-zero static array length
```

If zero-length static arrays are supported later, this rule may be relaxed.

---

## Dynamic Arrays

Dynamic arrays are heap arrays. The user-visible variable is internally a nullable reference to a refcounted dynamic array manager object.

The manager object and the element storage are both heap allocated.

```dq
var darr1 : [*]int;               // empty dynamic array, no manager, no element storage
var darr2 : [*]int = [];          // empty dynamic array, no manager, no element storage
var darr3 : [*]int = [1, 2, 3];   // dynamic array, length = 3, capacity = 3
var darr4 : [*]int = [7, 11];     // dynamic array, length = 2, capacity = 2
```

A non-initialized dynamic array and a dynamic array initialized with `[]` are both represented as a null manager. At the language level this means an empty array.

```dq
var arr : [*]int;

arr.length   // 0
arr.capacity // 0
arr == []    // true
arr <> []    // false
```

Direct null comparison is not allowed at the language level:

```dq
arr == null  // compile error
arr <> null  // compile error
```

Use length or empty-array comparison instead:

```dq
arr.length == 0
arr == []
arr <> []
```

Indexing an empty dynamic array is an out-of-bounds runtime error:

```dq
var arr : [*]int;
arr[0] = 5;  // runtime bounds error
```

---

## Array Literals

Array literals can initialize owning arrays:

```dq
var sarr : [3]int  = [1, 2, 3];
var iarr : [?]int  = [1, 2, 3];
var darr : [*]int  = [1, 2, 3];
```

An array literal cannot initialize a stored slice variable directly, because the slice would not own the literal storage:

```dq
var s : []int = [1, 2, 3];  // compile error: slice cannot own an array literal
```

An array literal may be passed directly to a function expecting `[]T`. In that case the compiler creates a temporary array valid for the duration of the call:

```dq
function Sum(a : []int) -> int:
  result = 0;
  for var i : int = 0 count a.length:
    result += a[i];
  endfor
endfunc

var x : int = Sum([1, 2, 3]);  // OK, temporary call-lifetime array view
```

---

## Array Slices

`[]T` is a mutable non-owning view of existing array storage.

Slices can be created from static arrays, inferred-length static arrays, dynamic arrays, or other slices:

```dq
var sarr : [5]int = [10, 20, 30, 40, 50];
var darr : [*]int = [10, 20, 30, 40, 50];

var s1 : []int = sarr[1:4];  // view of sarr[1], sarr[2], sarr[3]
var s2 : []int = darr[1:4];  // view of dynamic array element storage
var s3 : []int = s1[1:];     // sub-slice view
```

Writing through a slice modifies the original array storage:

```dq
var arr : [5]int = [10, 20, 30, 40, 50];
var s   : []int = arr[1:4];

s[0] = 99;  // modifies arr[1]

// arr is now [10, 99, 30, 40, 50]
```

Slice assignment copies only the view, not the elements:

```dq
var s1 : []int = arr[1:4];
var s2 : []int = s1;      // s1 and s2 point to the same element storage

s2[0] = 77;               // modifies arr[1]
```

A slice has no capacity and cannot structurally modify the referenced storage:

```dq
s.Append(1);     // compile error
s.Insert(0, 1);  // compile error
s.Delete(0, 1);  // compile error
s.Clear();       // compile error
s.Reserve(100);  // compile error
s.Compact();     // compile error
```

### Slice Lifetime and Invalidation

A slice is a non-owning view. It has the same lifetime risks as a pointer.

A slice becomes invalid if the referenced storage becomes invalid or moves. This can happen when:

- the source static array leaves scope,
- the source object containing the static array is destroyed,
- the source dynamic array releases its last owning manager reference,
- the source dynamic array reallocates its element storage,
- the source dynamic array is structurally modified by `.Append()`, `.Insert()`, `.Delete()`, `.Clear()`, `.Compact()`, `.Reserve()`, `.SetCapacity()`, or whole-array assignment,
- the underlying memory is otherwise freed or moved.

Using an invalid slice has undefined behavior in unchecked builds. Checked/debug builds may detect some invalid uses and report a runtime error, but the language does not guarantee lifetime safety for slices.

Example of a dangling slice:

```dq
function Bad() -> []int:
  var arr : [5]int = [1, 2, 3, 4, 5];
  return arr[1:3];   // dangerous: returns a slice into local stack storage
endfunc
```

Example of dynamic-array reallocation danger:

```dq
var darr : [*]int = [1, 2, 3, 4];
var s    : []int = darr[1:3];

darr.Append(5);  // may reallocate the dynamic array storage
s[0] = 99;       // dangerous: s may now be invalid
```

---

## Array Indexing

Array indexing is strict. The index must refer to an existing element.

Accessing an invalid index causes a runtime bounds error.

```dq
var arr : [?]int = [10, 20, 30, 40, 50];
var i : int;

i = arr[0];      // OK
i = arr[4];     // OK
i = arr[-1];    // runtime bounds error
i = arr[5];     // runtime bounds error
i = arr[$end];  // runtime bounds error, $end is one position after the last element
```

The same rule applies to static arrays, dynamic arrays, and slices.

---

## Array Slicing Syntax

DQ supports two slice interval forms:

```dq
arr[start:end]    // half-open interval, end is excluded
arr[start::end]   // closed interval, end is included
```

The `:` form is useful for lengths, offsets, splitting, and algorithms.

The `::` form is useful for human-oriented inclusive ranges.

```dq
var arr : [5]int = [10, 20, 30, 40, 50];

arr[0:1]   // = [10]
arr[0::0]  // = [10]
arr[1:3]   // = [20, 30]
arr[1::3]  // = [20, 30, 40]
```

Empty bounds mean the start or end of the array:

```dq
arr[:]      // whole array
arr[2:]     // from index 2 to the end
arr[:3]     // from the beginning to index 3 excluded
arr[2::]    // from index 2 to the last element, inclusive form
```

The slices may be empty:

```dq
arr[3:3]    // empty slice
arr[4:2]    // empty slice after normalization/clamping
```

The compiler might issue an empty slice error when at `[start:end]` both 'start' and 'end' are constants.

---

## Context-Specific Values in Indexing and Slicing

Identifiers prefixed with `$` are context-specific predefined values.

A `$name` value is only valid in contexts where the compiler defines that name. Outside such a context, using `$name` is a compile-time error.

For array indexing and slicing, these predefined values are available:

```dq
$last  // signed index of the last element, same as int(array.length) - 1
$end   // signed index position after the last element, same as int(array.length)
```

For an empty array:

```dq
$last == -1
$end  == 0
```

This keeps slicing convenient, because slice bounds are clamped:

```dq
empty[$last:]  // empty[-1:] -> clamped to []
```

Examples:

```dq
arr[$last]          // last element
arr[$last - 1]      // element before the last one

arr[$last:]         // slice containing only the last element
arr[:$end]          // whole array
arr[:$last]         // all except the last element
arr[::$last]        // whole array
```

```dq
var arr : [?]int = [10, 20, 30, 40, 50];

arr[$end - 1]             // 50
arr[$end - 2]             // 40

arr[$end - 2:]            // [40, 50]
arr[:$end - 1]            // [10, 20, 30, 40]
arr[$end - 2::$end - 1]   // [40, 50]
arr[$end - 2:$end - 1]    // [40]
```

---

## Invalid Slice Bounds

Array indexing is strict, but slicing is forgiving.

When defining a slice, invalid slice bounds do not cause runtime bounds errors. Slice bounds are clamped to the actual array bounds.

```dq
var arr : [?]int = [10, 20, 30, 40, 50];

var s1 : []int = arr[-1:];    // start clamped to 0      -> [10, 20, 30, 40, 50]
var s2 : []int = arr[:8];     // end clamped to length   -> [10, 20, 30, 40, 50]
var s3 : []int = arr[8:];     // start clamped to length -> []
var s4 : []int = arr[$end:];  // start clamped to length -> []
```

For the half-open form:

```dq
arr[start:end]
```

normalization is:

```text
start = clamp(start, 0, length)
end   = clamp(end,   0, length)

if end < start:
  end = start
```

For the inclusive form:

```dq
arr[start::end]
```

normalization is equivalent to converting the inclusive end to a half-open end:

```text
arr[start::end] == arr[start : end + 1]
```

The conversion must avoid integer overflow in the compiler/runtime implementation.

---

## Static Array Operations

```dq
var i : int;
var arr : [3]int = [1, 2, 3];

i = arr[1];
arr[0] = 4;

i = Len(arr);
i = arr.length;

var slice1 : []int = arr[1::2];        // view of arr[1], arr[2]
var slice2 : []int = arr[1::1];        // view of arr[1]
var slice3 : []int = arr[1:arr.length]; // view of arr[1], arr[2]
var slice4 : []int = arr[1:6];         // view of arr[1], arr[2], end clamped

slice1[0] = 99;  // modifies arr[1]

i = slice2.length; // 1

if slice1 <> []:  // equivalent to slice1.length <> 0
  printf('slice1 is not empty!');
endif

var last1 : []int = arr[$last:];    // last element
var last2 : []int = arr[$end - 2:]; // last two elements

var all_except_last1 : []int = arr[:$end - 1];     // all except the last element
var all_except_last2 : []int = arr[::$last - 1];   // all except the last element
var all_except_two1  : []int = arr[:$end - 2];     // all except the last two elements
var all_except_two2  : []int = arr[::$last - 2];   // all except the last two elements
```

---

## Dynamic Array Assignment and Aliasing

Whole dynamic-array assignment from another dynamic array shares the manager object:

```dq
var a : [*]int = [1, 2, 3];
var b : [*]int = a;

b[0] = 99;    // a[0] is also 99
b.Append(4);  // a also sees the appended value
```

Whole-array assignment from an array literal creates a new manager and rebinds the variable:

```dq
b = [7, 8, 9];  // b points to a new manager; a is unchanged
```

Whole-array assignment from a slice or static array copies elements into newly owned dynamic-array storage:

```dq
var sarr : [?]int = [10, 20, 30, 40, 50];
var view : []int  = sarr[1:4];

var copy1 : [*]int = sarr;       // copies all elements
var copy2 : [*]int = view;       // copies view elements
var copy3 : [*]int = sarr[1:4];  // copies slice elements
```

This creates independent dynamic storage:

```dq
view[0] = 99;    // modifies sarr[1]
copy2[0] = 77;   // modifies only copy2
```

Assignment to `[]` releases the current manager reference and returns to the canonical empty/null-manager state:

```dq
var arr : [*]int = [1, 2, 3];
arr = [];        // release manager reference, arr becomes empty/null-manager
```

This is different from `.Clear()`:

```dq
arr.Clear();     // destroys current elements but keeps manager and capacity
arr = [];        // releases manager reference and becomes empty/null-manager
```

---

## Dynamic Array Operations

```dq
var darr : [*]int = [10, 20, 30];
var arr2 : [?]int = [40, 50, 60];
var darr3 : [*]int = [70, 80];

darr.Append(1);          // append single element
darr.Append([4, 5]);     // append array literal elements
darr.Append(arr2[1:]);   // append slice elements
darr.Append(darr3);      // append all elements of another dynamic array

darr.Delete(0, 1);       // delete the first element
darr.Delete(1, 2);       // delete two elements: index 1 and index 2

darr.Insert(0, 1);       // insert one element at the front
darr.Insert(0, [-1, 0]); // insert two elements at the front

darr.Reserve(1000);      // ensure capacity >= 1000, length unchanged
darr.Compact();          // reduce capacity to length
darr.Clear();            // set length to 0, keep allocated capacity
darr.Clear(true);        // set length to 0 and set capacity to 0

var i : int = darr.length;
var c : int = darr.capacity;
```

Calling `.Append()`, `.Insert()`, `.Delete()`, `.Reserve()`, `.Compact()`, or `.Clear()` on a static array or slice is a compile-time error.

### Capacity Rules

`Reserve(n)` ensures that the capacity is at least `n`. It does not promise exact capacity.

```dq
darr.Reserve(1000);  // capacity >= 1000
```

`SetCapacity(n)` sets the capacity exactly. If `n < length`, the array is truncated and the removed elements are destroyed.

```dq
darr.SetCapacity(10);  // exact capacity = 10
```

`Compact()` sets capacity to the current length.

```dq
darr.Compact();        // capacity = length
```

### Overlapping Source Rules

Dynamic array operations must correctly handle overlapping source and destination ranges.

```dq
var a : [*]int = [1, 2, 3];

a.Append(a);       // [1, 2, 3, 1, 2, 3]
a.Append(a[1:]);   // valid
a.Insert(1, a);   // valid
```

Runtime helpers must detect overlap when needed and use a temporary copy or move-safe algorithm. This is especially important when an operation may reallocate the destination storage.

---

## Arrays as Function Arguments

A function parameter of type `[]T` accepts a mutable view of any compatible array storage.

```dq
var sarr : [5]int = [10, 20, 30, 40, 50];
var iarr : [?]int = [10, 20, 30, 40, 50];
var darr : [*]int = [10, 20, 30, 40, 50];

function SumIntArray(aarr : []int) -> int:
  result = 0;
  for var i : int = 0 count aarr.length:
    result += aarr[i];
  endfor
endfunc

function *Main() -> int:
  var i : int;

  i = SumIntArray(sarr);
  i = SumIntArray(iarr);
  i = SumIntArray(darr);
  i = SumIntArray(darr[1:4]);

  return 0;
endfunc
```

Because `[]T` is mutable, element writes through the parameter modify the original array:

```dq
function SetFirst(aarr : []int):
  aarr[0] = 99;
endfunc

var arr : [?]int = [1, 2, 3];
SetFirst(arr);

// arr is now [99, 2, 3]
```

A function parameter of type `[*]T` expects a dynamic array reference. Static arrays and slices are not accepted unless explicitly copied into a dynamic array.

```dq
var sarr : [?]int = [10, 20, 30, 40, 50];
var darr : [*]int = [10, 20, 30, 40, 50];

function AppendSum(aarr : [*]int):
  var sum : int = 0;
  for var i : int = 0 count aarr.length:
    sum += aarr[i];
  endfor
  aarr.Append(sum);
endfunc

function *Main() -> int:
  AppendSum(sarr);            // compile error: dynamic array expected
  AppendSum(sarr[1:4]);       // compile error: dynamic array expected
  AppendSum(darr);            // OK, sum appended to darr's manager
  AppendSum([*]int(sarr));    // compiler error: dynamic arrays require clear reference variables
  return 0;
endfunc
```

### Dynamic Array Parameter Passing

A dynamic array parameter is passed as a reference value to the manager object.

```dq
function F(aarr : [*]int):
  aarr.Append(1);  // modifies the shared manager, visible to caller
  aarr = [];       // rebinds only the local parameter variable
endfunc
```

To rebind the caller's dynamic array variable, use `ref` or `refout`:

```dq
function SelectArray(aindex : int, rarr : refout [*]int):
  if 1 == aindex:
    rarr = darr1;
  else:
    rarr = darr2;
  endif
endfunc
```

---

## Equality and Empty Checks

Empty comparison is defined for static arrays, dynamic arrays, and slices:

```dq
arr == []   // arr.length == 0
arr <> []   // arr.length <> 0
```

For fixed-size static arrays with non-zero length, `arr == []` is always false and may produce a compile-time warning.

General array equality is not implicitly defined in this draft:

```dq
a == b      // compile error unless both sides are scalar or an explicit equality rule exists
```

Use explicit operations for clarity:

```dq
SameArray(a, b)   // same dynamic manager or same slice data pointer and length
EqualArray(a, b)  // same length and equal elements
```

The exact names of these helper functions are runtime/library design details.

---

## Dynamic Array Runtime Handling

Dynamic arrays use a hidden, refcounted heap manager object:

```dq
object ODynArrMgr:
  refcount : uint;
  dataptr  : pointer;
  length   : uint;
  capacity : uint;

  elemsize : uint;        // cached from type info
  flags    : uint;        // cached element handling flags
  typeinfo : ^SDqTypeInfo;
endobj
```

The user-visible dynamic array variable is internally a nullable reference to `ODynArrMgr`:

```dq
var arr : [*]int;   // internally: manager reference = null
```

A null manager is not an invalid array. It represents an empty dynamic array:

```dq
arr.length   == 0
arr.capacity == 0
```

Operations that need storage allocate a manager automatically using compiler-provided type information.

The element type is described by a static `SDqTypeInfo` structure emitted into read-only/static data by the compiler:

```dq
struct SDqTypeInfo:
  storagesize  : uint;     // array stride, including padding
  flags        : uint;     // copy/destroy/init/move properties
  destroy_func : pointer;  // null = no destruction needed
  copy_func    : pointer;  // null = memcpy copy is enough
endstruct
```

The DQ compiler emits the required `SDqTypeInfo` block for every used dynamic array element type into the `.rodata` segment of the DQ module, with a unique mangled name. Multiple modules may emit equivalent type-info records. Later this can be optimized with COMDAT/linkonce_odr linkage, so the final executable can contain one canonical type-info object for each type.

Runtime helper functions receive both the dynamic array manager reference and the element type info:

```dq
function DqDynArrayAppend(
  amgr    : ref ODynArrMgr,
  atype   : ^SDqTypeInfo,
  dataptr : pointer,
  count   : uint
);
```

If `amgr == null`, the helper allocates a new `ODynArrMgr` and initializes it from `atype`.

The manager caches frequently used type information:

```dq
amgr.elemsize = atype.storagesize;
amgr.flags    = atype.flags;
amgr.typeinfo = &atype;
```

This avoids repeated type-info dereferencing during normal array operations.

---

## Element Lifetime Rules

Dynamic array operations must respect element copy and destruction semantics.

If `destroy_func == null`, elements do not require destruction.

If `copy_func == null`, raw memory copy is sufficient.

The runtime must call element handlers as follows:

| Operation | Required element handling |
|---|---|
| append/insert from source | copy new elements with `copy_func` or raw copy |
| delete/truncate | destroy removed elements |
| clear | destroy all current elements, length becomes 0 |
| manager destruction | destroy all current elements and free element storage |
| assignment to `[]` | release manager reference; if refcount reaches 0, destroy manager contents |
| reallocation | move/copy existing elements safely according to type flags |

Allocation failure or capacity overflow must not silently corrupt the array. The operation must raise a runtime error or exception. The original array should remain unchanged whenever practical.

All calculations of `capacity * storagesize` must be checked for integer overflow.

---

## Recommended Core Rules

1. `[N]T` is an owning fixed-size array.
2. `[?]T` is an owning fixed-size array whose length is inferred from the initializer.
3. `[]T` is a mutable non-owning slice/view. It is bounds-checked but not lifetime-safe.
4. `[*]T` is an owning dynamic heap array implemented through a refcounted manager object.
5. Indexing is strict and produces a runtime bounds error on invalid indexes.
6. Slicing clamps out-of-range bounds and produces a view.
7. Writing through a slice modifies the original element storage.
8. Assigning a slice to another slice copies only the view.
9. Assigning a slice/static array/literal to a dynamic array copies the elements into dynamic-array-owned storage.
10. Assigning one dynamic array to another shares the manager object.
11. Whole-array assignment rebinds dynamic array variables.
12. Element writes and dynamic-array mutating methods modify the referenced manager object.
13. Slices have pointer-like lifetime dangers and may become invalid when the source storage dies or moves.
