# Pointers

A **pointer** stores the address of a value. It lets you access that value
through its location in memory. Pointers are useful when working with C
libraries, raw buffers, or hardware. For an ordinary function that updates a
caller's variable, a `ref` parameter is usually simpler.

Write `^T` for a pointer to a value of type `T`. Use `%` to get a variable's
address and a trailing `^` to read or change the value at that address:

```dq
use print

func *Main():
    var value : int = 10
    var address : ^int = %value
    address^ = 42
    PrintLn("{}", [value])
endfunc
```

This prints `42`: `address^` and `value` access the same integer.

## Typed Pointers

The type after `^` tells the compiler what the pointer points to. For example,
`^int` points to an integer, and `^byte` points to a byte.

A pointer may be `nil`, meaning it has no target. Check for `nil` before using
a pointer that might be empty:

```dq
var value : int = 10
var address : ^int = %value
if address <> nil:
    address^ += 1 // value is now 11
endif
```

A non-`nil` pointer must still refer to a live value of the correct type.
Copying a pointer does not copy its target or keep that target alive. DQ does
not generally check raw addresses for you.

## Pointer Member Access

A pointer to a structure can access fields directly with a dot:

```dq
struct SPoint:
    x : int
    y : int
endstruct

var point : SPoint = {x: 10, y: 20}
var address : ^SPoint = %point
address.x = 30 // changes point.x; equivalent to address^.x = 30
```

This convenience does not change who owns the structure or how long it exists.

## Pointer Arithmetic and Indexing

Adding an integer to a typed pointer moves by that many elements. Pointer
indexing also returns a **pointer**, so add `^` to access the selected value:

```dq
var readings : [3]int = [10, 20, 30]
var first : ^int = %readings[0]
var second : ^int = first + 1
var value : int = first[1]^ // 20
second^ = 25                // readings is now [10, 25, 30]
var distance : int = second - first // 1 element
```

`first[1]` selects the address of the second integer; `first[1]^` reads it.
Unlike array indexing, pointer indexing carries no length information and does
not check bounds. Keep accesses within the original allocation.

## Allocation and Lifetime

Use `new` when the pointed-to value needs separately allocated storage, and
initialize it before reading it. Use `delete` when finished:

```dq
var number : ^int = new int
number^ = 42
delete number = nil // release the allocation and clear this variable
```

Other pointers to the same allocation become invalid after `delete`; they are
not cleared automatically. To release an allocation automatically when leaving
a scope, use `autofree ^T`. See
[Memory Management and Ownership](130-memory-management-and-ownership.md).

Pointers into local variables or fixed arrays are valid only while that storage
exists. Resizing a dynamic array or growing or changing a string's storage can
also invalidate pointers into it. Obtain a fresh pointer after such a change.
Never return a pointer to a function's local variable.

## Generic Pointers

The type `pointer` stores an address without saying what is there. Use it when
an external API accepts an unspecified address. Cast it to the correct typed
pointer before reading or writing:

```dq
var value : int = 10
var raw : pointer = %value
var address : ^int = ^int(raw)
address^ = 20 // value is now 20
```

The cast tells the compiler how to interpret the address; it does not check the
actual value stored there. Generic pointers do not support typed element
arithmetic until converted to a suitable typed pointer.

## Pointers, References, and Objects

A local `ref` alias gives another name to an existing variable. A pointer is a
separate value that can be changed to point elsewhere or set to `nil`. An
object variable is also a reference, with the fields and methods of its object
type. Casting between these forms requires the supported explicit conversions
and does not create ownership.

Use a typed [function reference](060-functions.md#function-references) to store
a callable function. For a C string, follow the
[string conversion rules](200-c-interoperability.md#strings-buffers-and-borrowing):
an address alone does not tell you the string's length or guarantee a terminator.

## Low-Level Details

The target storage must have the alignment required by its type. An incorrect
address or type can read unrelated memory, corrupt data, or crash the program.
A `nil` check alone cannot establish that an address is safe to use.

Pointer arithmetic and subtraction must stay within the same allocation or its
one-past-end position. You may form, compare, or subtract a pointer just past
the last element, but must not dereference it.

`autofree pointer` also supports scoped release of raw storage. Its address
must come from a compatible allocator; a local variable's address or an address
inside another allocation cannot be made an owner this way.
