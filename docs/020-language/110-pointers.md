# Pointers

A pointer stores an address. A typed pointer `^T` points to storage holding a
value of type `T`; the generic `pointer` stores an address without describing
what is there. Pointers do not automatically keep their targets alive.

```dq
use print

func *Main():
    var value : int = 10
    var address : ^int = %value
    address^ = 42
    var values : [3]int = [10, 20, 30]
    var first : ^int = %values[0]
    PrintLn("{} {}", [value, first[1]^])
endfunc
```

This prints `42 20`. `%` obtains an address; postfix `^` accesses the value at
that address. Pointer indexing returns a pointer, so the final `^` is essential.

## Typed Pointers

`^T` points to storage containing `T`. `%value` obtains an address and `pointer^`
dereferences a typed pointer.

```dq
var value : int = 10
var p : ^int = %value
p^ = 11
```

`nil` is compatible with pointer types. Dereferencing requires live, suitably
aligned storage of the correct type. DQ does not generally validate raw pointer
addresses or track their targets' lifetimes; an invalid dereference may fault
or produce undefined behavior.

`autofree ^T` owns a heap allocation represented by a typed pointer and releases
it when the owner is overwritten or destroyed. See
[`autofree` Ownership Types](130-memory-management-and-ownership.md).

## Pointer Member Access

A pointer to a structure is implicitly dereferenced for member access.

```dq
var point : SPoint = {}
var pp : ^SPoint = %point
pp.x = 10       // equivalent target to pp^.x
```

This convenience does not make the pointer own or extend the lifetime of the
structure.

## Pointer Arithmetic and Indexing

Adding or subtracting an integer moves by elements of the pointed-to type.
`p[offset]` returns the pointer at that offset; unlike C indexing, it does not
dereference. Write `p[offset]^` to access the element. Subtracting compatible
pointers produces an element distance.

Pointer arithmetic is valid only within an allocation or its one-past-end
position. The one-past-end pointer may be compared or subtracted but not
dereferenced.

## Generic Pointers

`pointer` carries an address without a pointee type. It must be explicitly cast
to a suitable typed pointer before dereference or scaled arithmetic. No runtime
type or lifetime information is attached.


## Allocation and Lifetime

`new` can allocate typed storage. Initialize it before reading its contents:

```dq
var number : ^int = new int
number^ = 42
delete number = nil
```

`delete value = nil` also clears the variable. Ordinary `delete value` does not
clear every alias to the address. `autofree ^T` and `autofree pointer` add scoped
ownership; see [Memory Management and Ownership](130-memory-management-and-ownership.md).

A pointer obtained from a local, fixed object, array element, string, or view
remains valid only while that storage exists and stays at the same address.
Resizing a dynamic array or growing/detaching a string can invalidate pointers
into it. Neither copying the pointer nor passing it through a function extends
the target's lifetime.

## Pointers, References, and Objects

A local `ref` alias names an existing variable; a pointer is a separate nullable
value. An object variable is also a reference, but carries the object's declared
member interface and object runtime behavior. Casting between these categories
requires the defined explicit conversions and does not create ownership.

For callable addresses use a typed function reference, described in
[Functions](060-functions.md#function-references). For C strings, use a suitable
borrowed `.pchar` or `rostr` conversion; a raw address alone carries neither a
length nor a terminator guarantee.
