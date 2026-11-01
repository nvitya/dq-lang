# Arrays and Slices

An array keeps several values of the same type together, such as a list of
names, a byte buffer, or a week's temperature readings. Use a **fixed array**
when the number of values is known in advance, or a **dynamic array** when you
need to add and remove values. A **slice** lets you work with part of an array
without copying its elements.

Here is a small program that builds a shopping list of strings and prints it:

```dq
use print

func PrintList(list : []str):
    for item :? in list:
        PrintLn("{}", [item])
    endfor
endfunc

func *Main():
    var shopping : [*]str = ["bread", "milk"]
    shopping.Append("apples")
    shopping.Append(["potatoes", "tomatoes"])
    var i : int = shopping.IndexOf("potatoes")
    if i >= 0:
        shopping[i] = "rice"
    endif
    PrintList(shopping)
endfunc
```

This prints:

```text
bread
milk
apples
rice
tomatoes
```

## Array Forms

In the types below, `T` stands for the element type (e.g. `int`, `float`, `str` etc.),
`N` is the number of elements in a fixed array.

| Form | Meaning | When to use it |
| --- | --- | --- |
| `[N]T` | A fixed array of `N` elements | The size is known in advance, such as seven daily readings. |
| `[?]T` | A fixed array whose size comes from its initial values | You want the compiler to count the values for you. |
| `[]T` | A slice that borrows existing elements | You want to pass an array or part of it to a function. |
| `[*]T` | A dynamic array that can grow or shrink | You collect values as the program runs. |

Every form has a `.length` property that returns the number of the array elements.

## Array Properties and Methods

Use this table to find an operation, then follow its link for examples.
**D** means dynamic arrays, **F** means fixed arrays, and **S** means slices.
A result of `—` means the method returns no value; `T` is the element type.

| Property or method | Types | Result | Purpose |
| --- | --- | --- | --- |
| [`.Append(value)`](#append-and-prepend)<br>[`.Append(values)`](#append-and-prepend) | D | — | Add one or more elements at the end. |
| [`.AppendSlice(values)`](#append-and-prepend) | D | — | Append the elements of another array or slice. |
| [`.capacity`](#capacity) | D | `int` | Space available before more storage is needed. |
| [`.Clear(free_storage = false)`](#clear) | D | — | Remove everything; optionally release storage. |
| [`.Clone()`](#clone) | D | `[*]T` | Make an independent copy. |
| [`.Compact()`](#setcapacity-and-compact) | D | — | Reduce capacity to match the length. |
| [`.Delete(index, count = 1)`](#delete) | D | — | Remove one or more elements. |
| [`.IndexOf(value)`](#indexof) | D, F, S | `int` | Find the first matching element, or return `-1`. |
| [`.Insert(index, value)`](#insert-and-insertslice)<br>[`.Insert(index, values)`](#insert-and-insertslice) | D | — | Add one or more elements before a position. |
| [`.InsertSlice(index, values)`](#insert-and-insertslice) | D | — | Insert the elements of another array or slice. |
| [`.length`](#length) | D, F, S | `int` | Number of elements. |
| [`.Pop()`](#pop-and-popfirst) | D | `T` | Remove and return the last element. |
| [`.PopFirst()`](#pop-and-popfirst) | D | `T` | Remove and return the first element. |
| [`.Prepend(value)`](#append-and-prepend)<br>[`.Prepend(values)`](#append-and-prepend) | D | — | Add one or more elements at the beginning. |
| [`.refcount`](#refcount-reference-count) | D | `int` | Number of references sharing the array. |
| [`.Reserve(capacity)`](#reserve) | D | — | Make room for later additions. |
| [`.SetCapacity(capacity)`](#setcapacity-and-compact) | D | — | Set storage size, removing excess elements if needed. |
| [`.SetLength(length)`](#setlength) | D | — | Change the number of elements. |

## Fixed Arrays

Use a fixed array when the number of elements will stay the same. Write the
size inside brackets before the element type:

```dq
var readings : [7]float = {}              // seven readings, initially all zero
var header : [3]byte = [0x44, 0x51, 0x01] // exactly three bytes
var colors : [?]str = ["red", "green", "blue"] // the compiler infers [3]str
```

`[?]T` needs an array literal as its initializer so the compiler can count the
elements. The length is part of a fixed array's type: `[3]int` and `[4]int` are
different types. You can replace individual elements, but cannot add or remove
any.

Assigning a fixed array to another compatible fixed array copies its elements:

```dq
var original : [3]float = [18.5, 21, 19.5]
var copy : [3]float = original
copy[0] = 22.5 // original[0] is still 18.5
```

Fixed arrays store their elements directly with the variable or containing
structure. For arrays of `autofree` elements, assignment first frees the
destination's previous objects, then transfers each source element and sets the
source elements to `nil`. Assigning an array to itself leaves it unchanged. See the
[automatic cleanup rules](130-memory-management-and-ownership.md#arrays).

## Dynamic Arrays

Use `[*]T` when the number of elements can change. Dynamic arrays are initialized empty
at the declaration, you can append elements using the `.Append()` method:

```dq
var names : [*]str
names.Append("Amy")
names.Append("Lin")
names.Append("Sam") // names is ["Amy", "Lin", "Sam"]
```

Assigning a dynamic array to another variable shares the same array. Changes to
the elements, length, or capacity are visible through both variables. Use
`.Clone()` when you need a separate copy:

```dq
var names : [*]str = ["Amy", "Lin"]
var shared : [*]str = names
shared[0] = "Sam"     // names is now ["Sam", "Lin"]
shared.Append("Jo")   // names is now ["Sam", "Lin", "Jo"]

var copy : [*]str = names.Clone()
copy[0] = "Alex"      // names[0] is still "Sam"
```

Assigning a different array to `shared` changes which array that variable uses;
it does not make `names` use the new array.

Dynamic arrays allocate their element storage as needed and manage its lifetime
automatically. An empty array can exist without allocating any storage.

Arrays of managed types, such as `str`, need extra runtime handling when elements
are copied or removed. The compiler adds this handling automatically.

## Indexing

Use `array[index]` to read or replace one element. Positions start at **zero**:
the first element is at `0`, the second at `1`, and so on.

```dq
var names  : [*]str = ["Amy", "Lin", "Sam"]
var first  : str = names[0]    // "Amy"
var second : str = names[1]    // "Lin"
names[1] = "Jo"                // names is now ["Amy", "Jo", "Sam"]
var last : str = names[$last]  // "Sam"
```

`$last` means `length - 1`, the position of the last element. `$end` means
`length`, the position just after it. Use `$end` for a slice bound or to insert
at the end; it does not select an existing element.

Make sure the array is nonempty before using `$last`, and keep element indices
between `0` and `length - 1`. Negative indices do not count backward from the
end: use `array[$last]` or `array[$end - 1]` for the last element. Indexing does
not adjust an invalid index to fit the available range.

Dynamic arrays report a runtime range error for invalid indices. Fixed arrays
and slices currently omit runtime bounds checks for speed. An invalid access
can read unrelated memory, corrupt data, or crash the program.

## Slices

A slice is a view of existing elements. Use one to process just part of an array
or to pass arrays of different sizes to the same function. Write `[]T` for its
type and `array[start:end]` to select a range. Slicing works on fixed arrays,
dynamic arrays, and existing slices, with the same range rules as strings.

The start position is included and the end position is excluded. For example,
`data[1:3]` selects elements `1` and `2`:

```dq
var data   : [*]byte = [0x10, 0x20, 0x30, 0x40]
var middle : []byte  = data[1:3]  // [0x20, 0x30]
middle[0] = 0xFF                  // data is now [0x10, 0xFF, 0x30, 0x40]
```

Changing a slice element changes the original array because no elements were
copied. A slice has its own length, but cannot add or remove elements from its
source array.

Omit a bound to select from the beginning or through the end. Use `::` instead
of `:` when you want to include the element at the end position:

```dq
var data : [*]byte = [0x10, 0x20, 0x30, 0x40]
var all  : []byte = data[:]         // [0x10, 0x20, 0x30, 0x40]
var head : []byte = data[:2]        // [0x10, 0x20]
var tail : []byte = data[2:]        // [0x30, 0x40]
var last_two  : []byte = data[$end - 2:] // [0x30, 0x40]
var inclusive : []byte = data[1::3] // [0x20, 0x30, 0x40]
```

Slice bounds are limited to the available range: a negative bound becomes zero
and a bound past the end becomes `length`. Negative slice bounds do not count
backward from the end. If the resulting end is before the start, the slice is
empty.

```dq
var data   : [4]int = [10, 20, 30, 40]
var all    : []int  = data[-5:100] // [10, 20, 30, 40]
var empty  : []int  = data[3:1]  // no elements
var at_end : []int  = data[$end:] // no elements
var negative_end : []int  = data[:-1] // no elements; -1 becomes zero
```

A slice borrows its source; it does not keep that source alive. Keep the original
array alive while using the slice. Adding or removing elements, resizing, or
releasing a dynamic array's storage can make existing slices and element
pointers invalid. Create a new slice after such a change, even if the change
seems to affect another part of the array. You also cannot keep a slice of a
temporary array that disappears before the slice does.

## Function Parameters

Use a slice parameter, `[]T`, when a function needs to read or update elements
without resizing the array. The caller can pass a fixed array, a dynamic array,
or a slice with a compatible element type. Passing an array automatically gives
the function a view of all its elements:

```dq
func Sum(values : []float) -> float:
    result = 0
    for value :? in values:
        result += value
    endfor
endfunc

func *Main():
    var fixed   : [3]float = [18.5, 21, 19.5]
    var growing : [*]float = [20.5, 22]
    var total   : float = Sum(fixed)     // 59
    var other   : float = Sum(growing)   // 42.5
    var part    : float = Sum(fixed[1:]) // 40.5
endfunc
```

A slice parameter can also change the caller's elements, just like the `middle`
slice in the previous section. For a function that adds or removes elements,
use a dynamic array passed by reference:

```dq
func AddName(names : ref [*]str, name : str):
    names.Append(name)
endfunc

func *Main():
    var names : [*]str = ["Amy", "Lin"]
    AddName(names, "Sam") // names is now ["Amy", "Lin", "Sam"]
endfunc
```

Dynamic-array parameters require a reference mode; `names : [*]str` by value
is not supported. A `ref` parameter can resize the array or replace the caller's
array variable. Other parameter modes and their read/write restrictions are
explained in [Functions](060-functions.md#parameter-modes).

## Iteration

Use `for ... in` to work with each element in order. `:?` asks the compiler to
infer the loop variable's type:

```dq
var readings : [*]float = [18.5, 21, 19.5]
var total    : float = 0
for reading :? in readings:
    total += reading
endfor
// total is 59
```

The ordinary loop variable receives each element's value. Use `ref` when you
want to change the array elements themselves:

```dq
var readings : [*]float = [18.5, 21, 19.5]
for reading : ref in readings:
    reading += 0.5
endfor
// readings is now [19, 21.5, 20]
```

A `ref` loop variable refers directly to the current array element and has its
exact type. Do not add or remove elements or otherwise resize the dynamic array
while using that reference: the element may move or disappear.

When the array stores `autofree` elements, an ordinary loop variable refers to
each object without removing it from the array or freeing it after the iteration.
Use a `ref` loop variable to replace an element in place; this automatically
frees the previous object. Elements can be `nil`, so check before using their
fields or methods. See [Dynamic Array `autofree` Elements](#dynamic-array-autofree-elements).

## Array Properties

Properties let you inspect an array. They are read-only; use methods such as
`.SetLength()` and `.Reserve()` to change its size or storage.

### `.length`

`.length` counts elements, not bytes. Use it to check whether an array is empty
or to find out how many values a slice contains. It works on every array form.

```dq
var data   : [4]int = [10, 20, 30, 40]
var middle : []int = data[1:3]
var total  : int = data.length   // 4
var count  : int = middle.length // 2
var empty  : [*]int
var is_empty : bool = empty.length == 0 // true
```

### `.capacity`

`.capacity` tells you how many elements fit in a dynamic array's current storage.
It is at least as large as `.length`. Extra capacity is spare room for future
additions; it does not create extra elements that you can index.

```dq
var scores : [*]int = [10, 20, 30]
scores.Reserve(100)
var count : int = scores.length         // 3
var has_room : bool = scores.capacity >= 100 // true
```

When growth exceeds the capacity, the array obtains more storage automatically.
An empty array with no allocated storage has capacity zero.

### `.refcount`: Reference Count

`.refcount` is mainly useful for checking how many references share a dynamic
array. Assigning it to another variable adds a reference; creating a slice does
not. Temporary values can also add references. An array with no allocated
storage has reference count zero.

```dq
var scores : [*]int = [10, 20, 30]
var shared : [*]int = scores
var references : int = scores.refcount // 2
```

## Dynamic Array Methods

See [Array Properties and Methods](#array-properties-and-methods) for a quick overview.

Call these methods on a dynamic array, such as `names.Append("Amy")`. Methods that
change the array affect every variable sharing it. `.Clone()` returns a separate
array, and `.Pop()` and `.PopFirst()` return the element they remove. The other
methods return no value.

The prototypes below use `T` for the array's element type: for `[*]str`, `T`
means `str`. A `values : []T` parameter accepts a fixed array, a dynamic array,
or a slice with a compatible element type. These prototypes describe the
built-in methods; you do not need to declare them yourself.

You can pass part of the same array as the source for an append or insert. The
method preserves the source values even if it needs to move the array's storage:

```dq
var values : [*]int = [10, 20, 30]
values.Append(values[:2]) // [10, 20, 30, 10, 20]
```

After a method changes the array's size or storage, previously created slices for
this array might become invalid; see [Slices](#slices). Arrays of `autofree` elements have additional move
and copy restrictions; see [Dynamic Array `autofree` Elements](#dynamic-array-autofree-elements).

### .Append and .Prepend

```text
.Append(value : T)
.Append(values : []T)
.AppendSlice(values : []T)
.Prepend(value : T)
.Prepend(values : []T)
```

Use `.Append()` to build a list one value at a time or add a whole group at once.
Use `.Prepend()` to put values at the beginning. When adding several elements,
both methods keep their original order.

```dq
var names : [*]str = ["Lin"]
names.Append("Sam")            // ["Lin", "Sam"]
names.Prepend(["Amy", "Jo"])   // ["Amy", "Jo", "Lin", "Sam"]
names.AppendSlice(["Alex"])    // ["Amy", "Jo", "Lin", "Sam", "Alex"]
```

`.AppendSlice()` explicitly adds the source's elements. This is useful for
arrays whose elements are themselves arrays, where `.Append()` may instead
add the source as one element.

Prepending moves the existing elements to make room. For a large list built
one element at a time, appending usually does less work.

### .Insert and .InsertSlice

```text
.Insert(index : int, value : T)
.Insert(index : int, values : []T)
.InsertSlice(index : int, values : []T)
```

Use `.Insert()` to put values before an existing position. Elements at that
position and after it move to make room. `.InsertSlice()` explicitly inserts
all the source elements, like the range form of `.Insert()`.

```dq
var stops : [*]str = ["home", "office"]
stops.Insert(1, ["bakery", "school"]) // ["home", "bakery", "school", "office"]
stops.Insert($end, "park")           // ["home", "bakery", "school", "office", "park"]
```

`$end` inserts after the last element, like `.Append()`. A negative insertion
position inserts at the beginning, and a position past the end inserts at the
end. Inserting near the beginning of a large array moves more elements than
inserting near its end.

### .Delete

```text
.Delete(index : int, count : int = 1)
```

Use `.Delete(index)` to remove one element, or supply `count` to remove several.
The elements after the removed range move to close the gap and keep their order.

```dq
var scores : [*]int = [10, 20, 30, 40]
scores.Delete(1, 2) // [10, 40]: remove elements at positions 1 and 2
scores.Delete(1)    // [10]
scores.Delete(100)  // [10]: nothing at this position
```

A negative position starts deleting at the beginning. A count larger than the
remaining length removes everything from that position onward. A non-positive
count or a position at or past the end leaves the array unchanged.

### .Pop and .PopFirst

```text
.Pop() -> T
.PopFirst() -> T
```

Use `.Pop()` to take the last element, for example when treating an array as a
stack. Use `.PopFirst()` to take the first element, such as the next job in a
small queue. Both remove the element and return its value.

```dq
var jobs : [*]str = ["download", "resize", "save"]
var next : str = jobs.PopFirst() // "download"; jobs is ["resize", "save"]
var last : str = jobs.Pop()      // "save"; jobs is ["resize"]
```

Check `.length` before popping if the array might be empty: both methods raise
a runtime error on an empty array. `.PopFirst()` moves all remaining elements
toward the beginning, so repeatedly using it on a large queue can be expensive.

### .SetLength

```text
.SetLength(length : uint)
```

Use `.SetLength()` when you know how many elements you need, for example before
filling a byte buffer by index. Growing adds initialized elements: new numbers
are zero and new strings are empty. Shrinking removes elements from the end
but keeps the spare storage for later use.

```dq
var packet : [*]byte
packet.SetLength(3) // [0, 0, 0]; length and capacity are 3
packet[0] = 0x44
packet[1] = 0x51
packet[2] = 0x01
packet.SetLength(2) // [0x44, 0x51]; capacity stays 3
```

The requested length must be non-negative. If it exceeds the current capacity,
`.SetLength()` allocates space for exactly that many elements. Existing spare
capacity is retained when the requested length already fits.

When you append an element to an array with no spare space, the runtime doubles
its allocated capacity, leaving room for later additions. Appending to an array
with no allocated storage starts with room for one element.

### .Reserve

```text
.Reserve(capacity : uint)
```

Use `.Reserve()` before collecting many values when you can estimate the final
size. It makes room for at least that many elements, which can avoid repeatedly
moving the array into larger storage. It does not add elements, change existing
values, or reduce capacity.

```dq
var readings : [*]float
readings.Reserve(100) // still empty, with room for at least 100 readings
readings.Append(18.5)
readings.Append(21.5) // [18.5, 21.5]; the reserved space is being used
```

Capacity must be non-negative. You can keep appending beyond the reserved amount;
the array will grow automatically.

### .SetCapacity and .Compact

```text
.SetCapacity(capacity : uint)
.Compact()
```

Use `.Compact()` after building an array if you want to release its unused
space. It reduces capacity to the current length without removing elements.

`.SetCapacity()` gives you direct control over the storage size. If the requested
capacity is smaller than the length, elements at the end are removed to fit.
Increasing capacity leaves the elements and length unchanged.

```dq
var values : [*]int = [10, 20, 30]
values.SetCapacity(2) // [10, 20]; length and capacity are 2
values.Reserve(100)
values.Compact()      // [10, 20]; capacity is 2 again
```

Capacity must be non-negative. Use `.Reserve()` when you only want to make room
for later additions without risking removal of existing elements.

### .Clear

```text
.Clear(free_storage : bool = false)
```

Use `.Clear()` to empty a list before filling it again. It keeps the storage so
later additions can reuse it. Pass `true` when you also want to release the
storage. Both forms leave the array empty.

```dq
var messages : [*]str = ["Connected", "Ready"]
messages.Clear()             // length is 0; capacity is retained
messages.Append("Restarted") // ["Restarted"]: reuse the existing storage
messages.Clear(true)         // length and capacity are 0
```

### .Clone

```text
.Clone() -> [*]T
```

Use `.Clone()` to keep an independent copy before changing an array. Assigning
the result to a new variable lets you edit that copy without changing the
original array's elements.

```dq
var names : [*]str = ["Amy", "Lin", "Sam"]
var saved : [*]str = names.Clone()
names[0].Append(" Lovelace") // saved[0] is still "Amy"
```

For `str` elements, later edits to a string in either array leave the other
array's string unchanged, as shown above.

Cloning copies the elements according to their types. Object references still
point to the same objects, and nested dynamic arrays still share their inner
arrays. Clone those inner arrays too if you need separate copies at every level.
Cloning arrays of `autofree` elements is not supported; see
[Dynamic Array `autofree` Elements](#dynamic-array-autofree-elements).

## Searching and Comparison

### .IndexOf

```text
.IndexOf(value : T) -> int
```

Use `.IndexOf()` to find an element in a fixed array, dynamic array, or slice.
It returns the first matching position, starting at zero, or `-1` if the value
is absent. Check for `-1` before using the result as an index.

```dq
var scores : [*]int = [10, 20, 30, 20]
var position : int = scores.IndexOf(20) // 1: the first match
if position >= 0:
    scores[position] = 25              // [10, 25, 30, 20]
endif
var missing : int = scores.IndexOf(99) // -1
```

For a slice, the result counts from the start of that slice:

```dq
var data : [4]int = [10, 20, 30, 40]
var middle : []int = data[1:3]          // [20, 30]
var position : int = middle.IndexOf(30) // 1, rather than 2
```

For `str` elements, `.IndexOf()` compares the text contents, just like `==`.
The comparison is case-sensitive and works on fixed arrays, dynamic arrays,
and slices of `str`. Separate strings containing the same text match:

```dq
var names : [*]str = ["Amy", "Lin", "Sam", "Lin"]
var wanted : str = "Lin"
var position : int = names.IndexOf(wanted) // 1: the first matching name
var missing : int = names.IndexOf("lin")  // -1: case matters
```

For other element types, the search compares the bytes stored for each element.
This works directly for values such as integers and bytes.
Object references match only when they point to the same object; the search
does not compare object contents. For structures, floating-point values, or
other types that need a custom comparison, use a loop
with the comparison you want. Structure padding and different floating-point
bit patterns can otherwise affect the result.

### Equality

For dynamic arrays and array slices, `== []` checks whether the length is zero
and `<> []` checks whether there are any elements:

```dq
var jobs : [*]str
var empty : bool = jobs == [] // true
jobs.Append("download")
var has_jobs : bool = jobs <> [] // true

var pending : []str = jobs[:]
var has_pending : bool = pending <> [] // true
var finished : []str = jobs[1:]
var all_done : bool = finished == [] // true
```

To compare the contents of two arrays, compare their lengths and then their
elements in a loop. General array equality is not provided automatically.

## Multidimensional Arrays

An array can contain other arrays. Use this to represent a grid or table. For
example, `[2][3]int` has two rows, each containing three integers:

```dq
var matrix : [2][3]int = [ [1, 2, 3], [4, 5, 6] ]
var value : int = matrix[1][2] // 6: second row, third column
matrix[0][1] = 20             // first row is now [1, 20, 3]
```

You can also nest dynamic arrays when rows need to grow separately. Each row
has its own storage. Cloning the outer array copies the row references, so the
copy still shares those rows; see [`.Clone()`](#clone).

## Dynamic Array `autofree` Elements

Use an element type such as `autofree OTask` when the array should automatically
free the objects stored in it. Replacing an element frees its previous object.
Removing elements with `.Delete()`, `.Clear()`, or a smaller length or capacity
also frees the removed objects. Any remaining objects are freed when the last
reference to the dynamic array is released. Assigning the array to another
variable still shares the same array, rather than copying its objects.

New `autofree` elements start as `nil`, including those added by `.SetLength()`.
Assigning `nil` to an element frees its current object and leaves that position
empty, without changing the array's length. A `nil` element has nothing to free.

Assigning an `autofree` variable to an array element transfers the object and
sets the source variable to `nil`. Single-element `.Append()`, `.Prepend()`, and
`.Insert()` do the same:

```dq
object OTask:
    name : str
    func *Create(aname : strslice):
        name = aname
    endfunc
endobject

func *Main():
    var tasks : [*]autofree OTask
    tasks.SetLength(2) // [nil, nil]

    var task = new autofree OTask('download')
    tasks[0] = task    // task is now nil; tasks[0] refers to the object
    tasks[0] = nil     // frees the object; tasks is [nil, nil], length still 2

    tasks.Append(new OTask('list'))
    var next : autofree OTask = tasks.Pop() // removes the object without freeing it
endfunc // next is freed automatically
```

`.Pop()` and `.PopFirst()` remove an element without freeing its object. They
return an ordinary object reference: assign it to an `autofree` variable, as
above, or explicitly delete it when finished. Popping a `nil` element returns
`nil`; popping an empty array is still an error. Likewise, check for `nil`
before accessing an element's fields or calling its methods.

Copying a range of `autofree` elements and calling `.Clone()` are not supported,
because two separate arrays must not automatically free the same objects. See
the [automatic cleanup rules](130-memory-management-and-ownership.md#arrays)
for more details.
