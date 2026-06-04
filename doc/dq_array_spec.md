# DQ Arrays

Status: draft
Scope: arrays, array slices, dynamic/heap arrays, storage modes

---

## Definitions

An array or an array slice is a continuous memory block of same element types.

## Static arrays

The length and the storage of the static array is fixed.

## Dynamic arrays

The length and the storage of the static array is variable.

## Core Syntax

### Static Array Declarations

```dq

var arr1 : [3]int;  // fixed size array, zero initialized
var arr2 : [3]int = [1, 2, 3];  // fixed size array with initialization
var arr3 : []int  = [1, 2, 3];  // fixed size array with mandatory initialization
                                // array length is set by the initialization

```

### Array Slicing

DQ supports two slice interval forms:

   `arr[start:end]`:    half-open interval, end is excluded (Python compatible)
   `arr[start::end]`:   closed interval, end is included

* The inclusive `::` form is recommended when writing human-oriented index ranges.

* The exclusive `:` form is useful for lengths, offsets, splitting, and algorithms.

* Empty slice compiler warning might be generated on [x:x] expressions

Negative start or end references to the array length: -1 = length - 1

#### Context-Specific Values

Identifiers prefixed with `$` are context-specific predefined values.

A `$name` value is only valid in contexts where the compiler defines that name.
Outside of such a context, using `$name` is a compile-time error.

For array indexing and slicing, these predefined values are available:

`$last`: index of the last element, same as array.length - 1
`$end`: index position after the last element, same as array.length

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
var arr : []int = [10, 20, 30, 40, 50];

arr[$end-1]      // 50
arr[$end-2]      // 40

arr[$end-2:]     // [40, 50]
arr[:$end-1]     // [10, 20, 30, 40]
arr[$end-2::$end-1]  // [40, 50]
arr[$end-2:$end-1]   // [40]

```

## Invalid Array Indexes

Array indexing is strict. The index must refer to an existing element of the array.
Accessing an invalid position of an array leads to a runtime error.

```dq
var arr : []int = [10, 20, 30, 40, 50];
var i : int;

i = arr[-1];     // Runtime error
i = arr[5];      // Runtime error
i = arr[$end];   // Runtime error, $end is one position after the last element
i = arr[$last];  // OK, last element
```

When defining slices of an array, invalid slice bounds do not generate runtime errors.
The slice bounds are clamped to the actual array bounds.

```dq
var arr : []int = [10, 20, 30, 40, 50];

var s1 : []int = arr[-1:];   // start position clamped to 0 -> whole array = [10, 20, 30, 40, 50]
var s2 : []int = arr[:8];    // end position clamped to length -> whole array = [10, 20, 30, 40, 50]
var s3 : []int = arr[8:];    // start position clamped to length -> empty slice = []
var s3 : []int = arr[$end:]; // start position clamped to length -> empty slice = []
```

### Static Array Operations

```dq

var arr1 : [3]int = [1, 2, 3];

arr1[0] = 4;
var i : int;
i = Len(arr1);
i = arr1.length;

var slice1 : []int = arr1[1::2];    // copy of [ arr1[1], arr1[2] ]
var slice2 : []int = arr1[1::1];
var slice3 : []int = arr1[1:arr1.length];  //= [ arr1[1], arr1[2] ]
var slice4 : []int = arr1[1:6];            //= [ arr1[1], arr1[2] ] (overindexing)

i = slice2.length; // = 1

var slice5 : []int = arr1[-1:];   // last element
var slice5 : []int = arr1[-1::];  // last element
var slice5 : []int = arr1[-2:];   // last two elements
var slice5 : []int = arr1[-2::];  // last two elements

var slice5 : []int = arr1[:-1];   // all except the last element
var slice5 : []int = arr1[::-1];  // all except the last element
var slice5 : []int = arr1[:-2];   // all except the last two elements
var slice5 : []int = arr1[::-2];  // all except the last two elements


```

### Dynamic Array Declarations

```dq
var darr1 : [*]int;               // dynamic/heap array, empty, no allocated element storage
var darr2 : [*]int = [];          // dynamic/heap array, empty, no allocated element storage
var darr3 : [*]int = [1, 2, 3];   // dynamic array with initialization, capacity = 3 (element storage for 3 elements)
var darr4 : [*]int = [7, 11];     // capacity = 2
darr4 = darr3;                    // darr4 and darr3 now point to the same array
                                  // the original darr4 storage is released

darr3 = [];                       // makes the array empty, reserved storage might be kept

```

A non-initialized or empty (`[]`) initialized dynamic array is empty and has no allocated element storage.

### Dynamic Array Operations

```dq

darr.Append(1);         // single element
darr.Append([4,5]);     // array slice
darr.Append(arr2[2..]);  // array slice
darr.Append(darr3);     // array slice, (append a whole dynamic array)

darr.Delete(0, 1);      // deletes [0], the first element
darr.Delete(1, 2);      // deletes [1] and [2]

darr.Insert(0, 1);         // inserts 1:int to the [0], to the front
darr.Insert(0, [-1, 0]);   // inserts -1 and 0 to the front

darr.Reserve(1000);  // reserve storage exacly for 1000 elements
darr.Compact();      // Free unused storage
darr.Clear();        // sets the length to 0
darr.Clear(true);    // sets the length to 0, and frees the unused storage

var i : int;

i = darr.length;   // actual length of the array, works for static arrays too

```

Calling .Append(), .Delete() on a static array is a compiler error.

## Dynamic Arrays

Dynamic arrays are like DQ objects. The user visible variable is a pointer (reference) to the (refcounted) array handler object. This object manages the element storage, which is dynamically allocated on the heap.

The dynamic array manager objects are always allocated on the heap, as they can receive multiple references.

```dq

var darr1 : [*]int = [10, 20, 30, 40, 50];
var darr2 : [*]int = [2, 3, 5, 7, 11, 13];

function *Main() -> int:
  var arr : [*]int;

  SelectArray(0, arr);  // arr points to darr2, the dynamic array handler object of the original arr is deleted,
                        // because of the reference count reached 0 for that

  arr[0] = 99;          // --> arr[0] == 99  and  darr2[0] == 99

  arr = [1, 2, 3];      // re-assignment, arr detaches from darr2, creates a new dynamic array handler object

  return 0;
endfunc

```


## Array as function arguments

Array slices `[]T` can be created from any type of arrays.

```dq

var sarr : [5]int = [10, 20, 30, 40, 50];
var darr : [*]int = [10, 20, 30, 40, 50];

function SumIntArray(aarr : []int):
  result = 0;
  for var i : int = 0 count aarr.length:
    result += aarr[i];
  endfor
endfunc

function *Main() -> int:
  var i : int;
  i = SumIntArray(sarr);
  i = SumIntArray(darr);
  return 0;
endfunc

```

Functions expecting dynamic arrays cannot be called with array slices. Dynamic array in function arguments can be modified.

```dq

var sarr : [5]int = [10, 20, 30, 40, 50];
var darr : [*]int = [10, 20, 30, 40, 50];

function AppendSum(aarr : [*]int):
  var sum : int = 0;
  for var i : int = 0 count aarr.length:
    sum += aarr[i];
  endfor
  aarr.Append(sum);
endfunc

function *Main() -> int:
  var i : int;
  AppendSum(sarr);  // compile error, dynamic array expected
  AppendSum(darr);  // sum appended to the end of the array
  return 0;
endfunc

```


```dq

var darr1 : [*]int = [10, 20, 30, 40, 50];
var darr2 : [*]int = [2, 3, 5, 7, 11, 13];

function SelectArray(aindex : int, refout rarr : [*]int):
  if 1 == aindex:
    rarr = darr1;
  else:
    rarr = darr2;
  endif;
endfunc

function *Main() -> int:
  var arr : [*]int;
  SelectArray(0, arr);  // arr points to darr2, the dynamic array handler object of the original arr is deleted,
                        // because of the reference count reached 0 for that
  return 0;
endfunc

```

## Dynamic Array Handling

Dynamic arrays use a hidden, refcounted heap manager object:

```dq
object ODynArrMgr:
  refcount : uint;
  dataptr  : pointer;
  length   : uint;
  capacity : uint;

  elemsize : uint;        // cached from type info
  flags    : uint;        // cached element handling flags
  typeinfo : ^ODqTypeInfo;
endobj
```

The user-visible dynamic array variable is internally a nullable reference to `ODynArrMgr`.

```dq
var arr : [*]int;   // internally: arr manager = null
```

A null manager is not an invalid array. It represents an empty dynamic array:

```
arr.length ==  0
arr.capacity == 0
```

Indexing an empty/null dynamic array is still an out-of-bounds error:

```
arr[0] = 5;      // runtime bounds error
```

The element type is described by a static SDqTypeInfo structure, emitted into read-only/static data by the compiler:

```
struct SDqTypeInfo:
  kind         : uint;
  size         : uint;
  align        : uint;
  flags        : uint;

  init_func    : pointer;
  copy_func    : pointer;
  move_func    : pointer;
  destroy_func : pointer;
endobj
```

Runtime helper functions receive both the dynamic array manager and the element type info:

```
function DqDynArrayAppend(amgr : ref ODynArrMgr, atype : refin SDqTypeInfo, dataptr : pointer, count : uint);
```

If amgr == null, the helper allocates a new ODynArrMgr and initializes it from atype.

The manager caches frequently used type information:

```
amgr.elemsize = atype.size;
amgr.flags    = atype.flags;
amgr.typeinfo = &atype;
``

This avoids repeated type-info dereferencing during normal array operations.

Whole-array assignment rebinds the dynamic array variable:

```
arr = [1, 2, 3];
```

This creates a new dynamic array manager, releases the old manager reference, and makes arr point to the new manager. It does not modify another array that shared the old manager.

Element writes and mutating methods modify the referenced manager object:

```
var a : [*]int = [1, 2, 3];
var b : [*]int = a;

b[0] = 99;       // a[0] is also 99
b.Append(4);     // a also sees the appended value
```

Assignment detaches:

```
b = [7, 8, 9];   // b now points to a new manager; a is unchanged
```

Empty assignment releases the current manager and returns to the canonical empty/null state:

```
arr = [];        // release manager, set arr manager to null
```

This is different from Clear():

```
arr.Clear();     // keeps the existing manager and capacity
arr = [];        // releases the manager and becomes null/empty
```

Recommended core rule:

A dynamic array variable may internally hold a null manager. At the language level, this means an empty array. Operations that need storage allocate a manager automatically using the compiler-provided ODqTypeInfo. Whole-array assignment rebinds the variable, while element writes and mutating methods modify the referenced manager.

## Questions

