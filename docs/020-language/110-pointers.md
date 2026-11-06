# Pointers

A **pointer** stores a memory address, which identifies a location in memory.

## Pointer Basics

For a data type `T`, write `^T` to create the corresponding pointer type,
such as `^int` or `^float`.

When working with pointers, it is important to distinguish between **address operations**
and **pointed-data operations**.

**Address operations** work on the memory address stored in the pointer. DQ works on the address
when **neither `^` nor `.`** is written **after the name** of the pointer variable or constant:
<br>&nbsp;&nbsp;`ptr = address`

**Pointed-data operations** work on the data at the address stored in the pointer. DQ works on that
data when **`^` or `.`** is written **after the name** of the pointer variable or constant.
This is also called **dereferencing**:
<br>&nbsp;&nbsp;`ptr^ = value`

A simple example using the `%` to get the address of a variable:

```dq
var u32value : uint32       // an ordinary variable whose storage is managed by the compiler
u32value = 0x04030201       // assigns a value to the variable

var byteptr : ^byte         // declares a pointer variable of type `^byte`
byteptr = ^byte(%u32value)  // points to the memory location where `u32value` is stored
byteptr^ = 0x55             // updates the first byte at that address

PrintLn('u32value = 0x{:08X}', [u32value])  // = "u32value = 0x04030255"
```

On a little-endian system, the first byte is the least significant byte of
`u32value`, so the example prints the result shown above.

Pointers are widely used and can **improve performance** significantly.

Using pointers also **can be dangerous**. There are many ways to end up with an invalid
pointer (one that points to an invalid memory location). Accessing data
through an invalid pointer can return invalid data or cause the application to crash.

Pointers are useful when working with C libraries, raw buffers, or hardware.
For an ordinary function that updates a
caller's variable, a `ref` parameter is usually simpler.

For a more practical example for using pointers see the
[Example: Processing a Sensor Packet](#processing-a-sensor-packet).

## Pointer Variables

Assigning a pointer variable sets its pointing address. A pointer variable
can be re-assigned to point a different address:

```dq
var first  : int = 10
var second : int = 20
var curptr : ^int

curptr = %first    // setting the address of the pointer
curptr^ = 15       // first is now 15; current^ also reads 15
curptr = %second   // redirects curptr to second
curptr^ = 25       // second is now 25; first remains 15

var otherptr : ^int = current // assignment between two pointer variables
```

## Typed Pointers and Casting

A pointer's type tells the compiler how to access the data at its address:
how many bytes to read or write, which structure fields are available, and
how far to move during pointer arithmetic.

The type after `^` describes the pointed-to value. For example, `^byte` accesses
a single byte, `^int32` accesses a 4-byte signed integer, and `^float` accesses a
floating-point number occupying 4 or 8 bytes, depending on the target. (The size
of the pointer itself is the same for all three types, either 4 or 8 bytes depending
on the target CPU architecture.)

The pointed-to type also determines the step size of
[pointer arithmetic and indexing](#pointer-arithmetic-and-indexing): adding
1 to a `^byte` moves by 1 byte, while adding 1 to a `^uint32` moves by 4 bytes.

**Casting** converts a value from one type to another, or tells the compiler
to treat a value as a different type.

For pointers, casting usually changes the **pointer type** without changing
the address stored in the pointer. The same memory address is then interpreted
as pointing to a different data type.

The cast tells the compiler how to interpret the address; it does not check the
actual value stored there.

Sometimes an address comes from an API without type information, or the same
memory needs to be accessed through a different pointer type. An explicit cast
supplies the required type.

Write `^T(address)` to cast an address to a pointer to `T`. A pointer cast does
not read, copy, or convert the data stored there.

For example, `^byte`, `^int32`, `^float`, and `^uint64` pointers can all point to
the beginning of the same eight-byte buffer. Use `%` to take its address and
a trailing `^` to read or write through each pointer:

```dq
use print

var buffer : [8]byte [[align(8)]]

func ShowBuffer(acaption : strslice):
    Print("{:<28} at {:p}:", [acaption, %buffer[0]])
    for i : int = 0 count buffer.length:
        Print(" {:02X}", [buffer[i]])
    endfor
    PrintLn()
endfunc

func *Main():
    var byteptr  : ^byte   = %buffer[0]
    var intptr   : ^int32  = ^int32(%buffer[0])
    var floatptr : ^float  = ^float(%buffer[0])
    var u64ptr   : ^uint64 = ^uint64(%buffer[0])

    ShowBuffer("Initial")

    for i : int = 0 count buffer.length:
        byteptr[i]^ = 0xD0 + i
    endfor
    ShowBuffer("Set Pattern")

    byteptr^ = 42    // writes 1 byte
    ShowBuffer("byteptr^ = 42")

    intptr^ = -1000  // writes 4 bytes
    ShowBuffer("intptr^ = -1000")

    floatptr^ = 1.5  // writes SizeOf(float) bytes
    ShowBuffer("floatptr^ = 1.5")

    u64ptr^ = 0x0102030405060708 // writes 8 bytes
    ShowBuffer("u64ptr^ = 0x0102030405060708")
endfunc
```

**Example output on Linux x64** (the address varies between runs):

```text
Initial                      at ^555512345678: 00 00 00 00 00 00 00 00
Set Pattern                  at ^555512345678: D0 D1 D2 D3 D4 D5 D6 D7
byteptr^ = 42                at ^555512345678: 2A D1 D2 D3 D4 D5 D6 D7
intptr^ = -1000              at ^555512345678: 18 FC FF FF D4 D5 D6 D7
floatptr^ = 1.5              at ^555512345678: 00 00 00 00 00 00 F8 3F
u64ptr^ = 0x0102030405060708 at ^555512345678: 08 07 06 05 04 03 02 01
```

`ShowBuffer` prints the starting address followed by all eight bytes in memory
order. The global buffer starts with zeroes.

The "Set Pattern" loop writes the pattern using pointer indexing through `byteptr[i]^`,
without changing `byteptr`.

The following `byteptr^ = 42` changes only the first byte of that pattern.
The `int32` write changes the first 4 bytes to the two's-complement representation
of `-1000`, leaving `D4 D5 D6 D7` unchanged. Almost all current CPU architectures
use little-endian byte order, with the least significant byte first. The
floating-point write uses 8 bytes and shows the `float64` representation of
`1.5`. On targets with a 4-byte `float`, that write changes only the first
4 bytes. The final `0x0102030405060708` `uint64` write always uses
8 bytes: `08 07 06 05 04 03 02 01` in little-endian byte order.

The global buffer has eight-byte alignment, requested by `[[align(8)]]`, so it
can hold any of these values at its starting address, as some CPU architectures
cannot read/write multi-byte data types from unaligned addresses.

`%buffer[0]` takes the
address of its first byte. The casts `^int32(%buffer[0])`, `^float(%buffer[0])`,
and `^uint64(%buffer[0])` take the same address and give it different pointer types.

As all four pointers access the same storage, each write overwrites the bytes
used by that type, and each read interprets those bytes according to the
pointer's type. Reading through `intptr^` after the floating-point write would
interpret the low 4 bytes of the floating-point representation as an integer (=0); it would not
convert `1.5` to an integer.

## The `nil` Value

`nil` is the special pointer value meaning **no target**. It is also called a
**null-pointer** and represents address zero. Both typed pointers, such as
`^int`, and generic `pointer` variables can be assigned `nil`.

A `nil` pointer is **always invalid for accessing data**. Dereferencing it to
read or write data, either with `ptr^` or through a structure field such as
`ptr.field`, usually causes the program to crash with a **Segmentation Fault**.
The exact failure depends on the target system; never dereference `nil`.

Assigning, copying, and comparing `nil` are safe address operations. Use `==`
or `<>` to check whether a pointer has a target before accessing its data:

```dq
var value : int = 10
var ptr   : ^int = nil // no target yet

if ptr == nil:
    ptr = %value      // give the pointer a valid target
endif

if ptr <> nil:
    ptr^ += 1         // value is now 11
endif

ptr = nil             // clear the pointer; value remains 11
```

Setting a pointer to `nil` clears its stored address; it does not change or
free the pointed-to data, unless it is an `autofree` pointer.

## Pointers as Function Parameters

Typed pointers can also be function parameters. A pointer may be `nil`, meaning
it has no target, so a function that accepts an optional target must check
before dereferencing it:

```dq
func Increment(target : ^int):
    if target <> nil:
        target^ += 1
    endif
endfunc

func *Main():
    var count : int = 10
    Increment(%count) // count is now 11
    Increment(nil)    // no target, so nothing is changed
endfunc
```

A non-`nil` pointer must still refer to a live value of the correct type.
Copying a pointer does not copy its target or keep that target alive. DQ does
not generally check raw addresses for you.


## Structure Pointers

Pointers tor structures are very useful to manipulate memory structures at arbitrary
positions in the memory. Fields of the pointed structure can be accessed either
with `^.` or `.` only.

```dq
struct SPoint:
    x : int
    y : int
endstruct

var point   : SPoint = {x: 10, y: 20}
var address : ^SPoint = %point
address^.x = 30 // changes point.x
address.x  = 30 // changes point.x, `.` dereferences automatically
```

## Generic Pointers

A generic `pointer` carries no information about the data it points to.
Generic pointers do not support typed element arithmetic until
converted to a suitable typed pointer.

Cast it to a typed pointer before accessing that data:

```dq
var value  : uint32 = 0x01020304
var rawptr : pointer = %value

^byte(rawpr)^ = 0x55 // value is now 0x01020355
```

Here, `^byte(rawptr)` produces a pointer of type `^byte`, so the `^byte(rawpr)^`
expression manipulates the lowest byte of the 4-byte `value`.

## Pointer Size and Memory Addresses

A pointer variable is either **32 bits (4 bytes)** or **64 bits (8 bytes)** wide,
depending on the target CPU architecture. This is the size of the stored address;
the data it points to can have a different size. For example, `^byte` and `^uint64`
have the same size on a given target.

In DQ, `int` and `uint` always have the same size as a pointer:
`SizeOf(pointer)`, `SizeOf(int)`, and `SizeOf(uint)` are equal.

| Target | Pointer size | `int` | `uint` |
| --- | --- | --- | --- |
| ARM_M (Arm Cortex-M) | 32 bits (4 bytes) | `int32` | `uint32` |
| Linux x64 (`x86_64-linux`) | 64 bits (8 bytes) | `int64` | `uint64` |

Memory addresses are usually written in **hexadecimal**, where each digit
represents 4 bits. A full-width 32-bit address has 8 hexadecimal digits, while
a full-width 64-bit address has 16. Leading zeroes are often omitted.

For example, addresses might look like this:

| Target | Example address | Meaning |
| --- | --- | --- |
| ARM_M | `0x20001234` | A location in the SRAM region |
| ARM_M | `0x40001000` | A location in the peripheral region |
| Linux x64 | `0x0000555512345678` | A location in a process's virtual address space |
| Linux x64 | `0x00007FFC1234ABCD` | Another possible user-space virtual address |

On ARM_M, addresses refer directly to memory or hardware registers. The
[Cortex-M memory map](https://support.arm.com/documentation/100230/0100/Programmers-Model/Memory-model)
defines regions such as SRAM and peripherals; the exact locations available
depend on the microcontroller.

On Linux x64, an ordinary program uses **virtual addresses**, which the operating
system maps to physical memory. Addresses can vary between runs. A pointer still
occupies 8 bytes even when its address uses fewer than 16 hexadecimal digits;
the CPU and operating system do not make every possible 64-bit address usable.
You can see the [Linux x64 memory map](https://docs.kernel.org/arch/x86/x86_64/mm.html)
for more details.

DQ's `PrintLn` displays pointer addresses in hexadecimal with a leading `^`
instead of `0x`:

```dq
var value   : int = 42
var address : ^int = %value
PrintLn("{:p}", [address])
```

Example output could be `^20001234` on ARM_M or `^7FFC1234ABCD` on Linux x64.
These are illustrative addresses; the actual address depends on where `value`
is stored.


## Pointer Arithmetic and Indexing

Pointer arithmetic counts **elements of the pointed-to type**. For a pointer
`ptr : ^T`, adding `n` moves the address by `n * SizeOf(T)` bytes.

| Operation | Meaning | Changes `ptr`? |
| --- | --- | --- |
| `ptr + n` | Address `n` elements forward | No |
| `ptr - n` | Address `n` elements backward | No |
| `++ptr` / `--ptr` | Move forward / backward by one element | Yes |
| `ptr += n` / `ptr -= n` | Move forward / backward by `n` elements | Yes |
| `ptr[n]` | Same address as `ptr + n` | No |

Pointer indexing returns a **pointer**, so `ptr[n]^` reads or writes the
selected value. For a structure pointer, use `ptr[n]^.x` to access a field
of the selected structure.

### Stepping Through the Same Buffer

This example uses a fixed `[16]byte` buffer. It first advances a byte pointer,
then casts its current address to `^uint32` and continues in 4-byte steps.
The structure writes start at byte offset 8. Here, `SPoint` uses `int16`
coordinates so that each point occupies 4 bytes on both 32-bit and 64-bit targets.

| Pointer type | Element size | Bytes moved by one `++` | Byte offset selected by `[1]` from the start |
| --- | --- | --- | --- |
| `^byte` | 1 byte | 1 | 1 |
| `^uint32` | 4 bytes | 4 | 4 |
| `^SPoint` | 4 bytes | 4 | 4 |

The same 16 bytes can therefore hold 16 bytes, four `uint32` values, or four
`SPoint` values. These are different views of the same storage; writes through
any of the pointers change `buffer`.

The `uint32` writes below use unaligned addresses, so run this example on a
target that supports them, such as Linux x64.

```dq
use print

struct SPoint:
    x : int16
    y : int16
endstruct

var buffer : [16]byte [[align(8)]]

func ShowBuffer(caption : strslice):
    Print("{:<18}:", [caption])
    for i : int = 0 count buffer.length:
        Print(" {:02X}", [buffer[i]])
    endfor
    PrintLn()
endfunc

func *Main():
    var byteptr  : ^byte   = %buffer[0]
    for i : int = 0 count buffer.length:
        byteptr[i]^ = i // indexing leaves byteptr at the start
    endfor
    ShowBuffer("Initial")

    byteptr^ = 0xAA // writes 1 byte at offset 0
    ++byteptr       // moves 1 byte: now at buffer[1]
    byteptr^ = 0xBB // writes 1 byte at offset 1
    ++byteptr       // moves 1 byte: now at buffer[2]
    ShowBuffer("After ++byteptr")

    var u32ptr : ^uint32 = ^uint32(byteptr) // keeps byte offset 2
    u32ptr^ = 0xD1D2D3D4 // writes 4 bytes at offsets 2..5
    ++u32ptr             // moves 4 bytes: now at buffer[6]
    u32ptr^ = 0xE1E2E3E4 // writes 4 bytes at offsets 6..9
    ++u32ptr             // moves 4 bytes: now at buffer[10]
    ShowBuffer("After ++u32ptr")

    // Start at offset 8 so that two 4-byte points fit in the buffer.
    var pointptr :? = ^SPoint(%buffer[8])
    pointptr.x = 11 // writes 2 bytes at offsets 8..9
    pointptr.y = 21 // writes 2 bytes at offsets 10..11
    ++pointptr      // moves 4 bytes: now at buffer[12]
    pointptr.x = 12 // writes 2 bytes at offsets 12..13
    pointptr.y = 22 // writes 2 bytes at offsets 14..15
    ShowBuffer("After ++pointptr")

    // Reset all three pointers to the beginning before comparing indexing.
    byteptr  = %buffer[0]
    u32ptr   = ^uint32(%buffer[0])
    pointptr = ^SPoint(%buffer[0])

    byteptr[2]^ = 0xCC       // byte offset 2
    u32ptr[1]^ = 0xA1A2A3A4  // byte offset 4: the second uint32
    pointptr[2]^.x = 0x5353  // writes 2 bytes at offsets 4..5: the second point's x
    pointptr[2]^.y = 0x5252  // writes 2 bytes at offsets 6..7: the second point's y
    ShowBuffer("Indexed writes")

    PrintLn("Second point: ({}, {})", [pointptr[1]^.x, pointptr[1]^.y])

    var fourth : ^SPoint = pointptr + 3 // byte offset 12: the fourth point
    PrintLn("Distance: {} points, {} bytes",
        [fourth - pointptr, ^byte(fourth) - ^byte(pointptr)])
endfunc
```

Example output on Linux x64 (little-endian):

```text
Initial           : 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
After ++byteptr   : AA BB 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F
After ++u32ptr    : AA BB D4 D3 D2 D1 E4 E3 E2 E1 0A 0B 0C 0D 0E 0F
After ++pointptr  : AA BB D4 D3 D2 D1 E4 E3 0B 00 15 00 0C 00 16 00
Indexed writes    : AA BB CC D3 A4 A3 A2 A1 53 53 52 52 0C 00 16 00
Second point: (41892, 41378)
Distance: 3 points, 12 bytes
```

Each `++` changes only the pointer's address; the assignments through the
pointers change the buffer. Casting `byteptr` to `^uint32` keeps its current
byte offset, 2; the subsequent `++u32ptr` operations step to offsets 6 and 10.
The structure writes at offsets 8..15 overwrite the last two bytes of the
previous `uint32` write as well as the remaining initial bytes.

After the reset, the `[]` expressions leave all three pointers at the beginning.
For example, `u32ptr[1]^` writes the same four bytes as `(u32ptr + 1)^`.
`pointptr[1]^.x` selects the second point, dereferences it, and accesses its `x` field.
The indexed `SPoint` writes replace the same four bytes at offsets 4..7 that
were just written through `u32ptr[1]^`.

Subtracting two pointers of the same type gives their distance in **elements**.
The two point addresses above are three `SPoint` elements apart. Casting both
to `^byte` before subtracting gives the distance in bytes: 12.

The buffer starts at an address aligned with `[[align(8)]]`, but offsets 2 and 6
are still unaligned for `uint32`. On targets that require aligned accesses,
start those writes at offsets divisible by 4. A cast alone does not provide
alignment or make more storage available. Unlike array indexing, pointer indexing carries
no length information and does not check bounds. Keep each entire value you
read or write within the buffer. You may form a pointer just past the last
element, but must not dereference it; subtract pointers only within the same
allocation or its one-past-end position.

## Allocation and Lifetime

Use `new` when the pointed-to value needs separately allocated storage, and
initialize it before reading it. Use `delete` when finished:

```dq
struct SPoint:
    x : int
    y : int
endstruct

var point : ^SPoint = new SPoint
point^ = {x: 10, y: 20}
delete point = nil // release the allocation and clear this variable
```

Other pointers to the same allocation become invalid after `delete`; they are
not cleared automatically. To release an allocation automatically when leaving
a scope, use `autofree ^T`. See
[Memory Management and Autofree](130-memory-management-and-ownership.md).

Pointers into local variables or fixed arrays are valid only while that storage
exists. Resizing a dynamic array or growing or changing a string's storage can
also invalidate pointers into it. Obtain a fresh pointer after such a change.
Never return a pointer to a function's local variable.

## Dangling Pointers

A non-`nil` address does not always guarantee
that a pointer is valid: it might refer to memory that has already been freed
or to a variable that has gone out of scope, like a local variable.
Such pointers are called **dangling pointers**.

Using `SPoint` from the allocation example above:

```dq
var point : ^SPoint = new SPoint
point^ = {x: 10, y: 20}
var borrowed : ^SPoint = point // both pointers refer to the same allocation

delete point = nil          // free the allocation and clear point
// borrowed is still non-nil, but is now dangling:
// borrowed.x = 30          // invalid: the target has already been freed
borrowed = nil              // clear the dangling pointer
```

**Never dereference `nil` or dangling pointers.**


## Pointer Examples

### A Linked List with First and Last Pointers

A **linked list** stores each value in a node with a pointer to the next node.
The nodes can be allocated separately. Unlike an array, the list is traversed
by following `next`, rather than by incrementing a pointer.

`SList` keeps two addresses: `first` selects the first node and `last` selects
the final node. Both are `nil` for an empty list. Keeping `last` lets `Append`
add a node without walking through all the existing nodes. The last node's
`next` is always `nil`.

```dq
use print

struct SNode:
    value : int
    next  : ^SNode
endstruct

struct SList:
    first : ^SNode
    last  : ^SNode

    func Append(value : int):
        var node : ^SNode = new SNode
        node.value = value
        node.next = nil

        if last == nil:
            first = node // adding the first node to an empty list
        else:
            last.next = node
        endif
        last = node
    endfunc

    func PopFirst(value : ref int) -> bool:
        if first == nil:
            return false
        endif

        var node : ^SNode = first
        value = node.value
        first = node.next // save the next address before releasing the node
        if first == nil:
            last = nil // removing the only remaining node empties both ends
        endif
        delete node
        return true
    endfunc

    func Clear():
        var value : int = 0
        while PopFirst(value):
        endwhile
    endfunc
endstruct

func *Main():
    var list : SList = {} // both first and last start as nil
    list.Append(10)
    list.Append(20)
    list.Append(30)
    PrintLn("First: {}, last: {}", [list.first.value, list.last.value])

    Print("Values:")
    var current : ^SNode = list.first
    while current <> nil:
        Print(" {}", [current.value])
        current = current.next // follow the link; nodes need not be adjacent
    endwhile
    PrintLn()

    var value : int = 0
    while list.PopFirst(value):
        PrintLn("Removed: {}", [value])
    endwhile
    PrintLn("Empty: {}", [list.first == nil and list.last == nil])

    list.Append(40) // the emptied list can be used again
    PrintLn("Reused: first={}, last={}", [list.first.value, list.last.value])
    list.Clear() // release every node still owned by the list
    PrintLn("Cleared: {}", [list.first == nil and list.last == nil])
endfunc
```

Example output:

```text
First: 10, last: 30
Values: 10 20 30
Removed: 10
Removed: 20
Removed: 30
Empty: true
Reused: first=40, last=40
Cleared: true
```

When appending to an empty list, the new node becomes both `first` and `last`.
Otherwise, `last.next` links the old tail to the new node, and `last` moves to
that node. Appending changes links, without moving any existing nodes.

`PopFirst` returns `false` for an empty list. Otherwise, it copies the first
node's value into the caller's variable, advances `first` to `node.next`, and
then deletes the removed node. It clears `last` when the last remaining node
is removed, so a later `Append` correctly starts a new list. Both appending
and removing the first node take a constant number of pointer operations.

The list owns its allocated nodes. `Clear` reuses `PopFirst` to release all of
them; deleting one node does not automatically follow and delete its `next`.
Call `Clear` before the list leaves scope if it still contains nodes. Copying
an `SList` structure copies only its two addresses, so it does not create an
independent list and must not create a second owner of the same nodes. Any
borrowed pointer to a removed node becomes invalid when that node is deleted.

### Processing a Sensor Packet

This is a longer, somewhat practical example demonstrating using pointers.

Suppose a sensor station sends a binary packet containing a header followed by
measurement records. The header tells us the format version, measurement time,
location, and number of records. Each record identifies a measurement type and
channel, and stores a measured value. In this example, measurement type `1` means temperature
in degrees Celsius and type `2` means relative humidity in percent.

A file or network API may supply this packet as a buffer address and a byte
count. Pointers let us read the header and records directly in that buffer,
without copying each record into a separate variable.

For safer data handling structures are used in practice. Here two structures
are used to group the data fields: `SDataHeader` and `SDataRec`.
The `SDataHeader` describe the 32-byte header and the `SDataRec` the 8-byte data records.


```text
Byte offset:  0                  32             40              48
              | SDataHeader      | SDataRec      | SDataRec      |
              | version, count,  | type, channel,| type, channel,|
              | time, location,  | pad, value    | pad, value    |
              | reserved space   |               |               |
```

The sender must use the same field layout, byte order as the receiver.

The `_pad` fields in this example align the data fields manually and/or reserve
space for future expansions. The `_pad` fields initialized with zeroes.

Here, `Main` builds a packet in
memory with that layout so the program can run without a file or network
connection. `SExamplePacket` only supplies the sample storage; `PrintData` uses
the address and byte count and can handle any number of records that fits.


```dq
use print
use time

struct SDataHeader:
    version  : uint32     // format identifier
    reccount : uint32     // data record count
    mtime    : float64    // measurement time: @time.Time()
    location : uint32     // measurement location
    _pad     : [3]uint32  // reserved bytes for future expansion
endstruct

struct SDataRec:
    mtype      : byte  // measurement type: 1 = temperature, 2 = humidity
    channel    : byte  // one station can have multiple sensors
    _pad       : [2]byte
    value      : float32  // measurement value
endstruct

func PrintData(data : pointer, size : int):
    if data == nil or size < SizeOf(SDataHeader):
        PrintLn("Incomplete header")
        return
    endif

    var header :? = ^SDataHeader(data)
    if header.version <> 1:
        PrintLn("Unsupported data version")
        return
    endif

    var capacity : int = (size - SizeOf(SDataHeader)) div SizeOf(SDataRec)
    if header.reccount > capacity:
        PrintLn("Incomplete records")
        return
    endif

    PrintLn("Location {}, time {}, record count: {}",
        [header.location, FormatTime1('yyyy-mm-dd hh:nn:ss', header.mtime), header.reccount])

    // header[1] points just past the header, where the records begin.
    var record :? = ^SDataRec(header[1])
    for i : uint32 = 0 count header.reccount:
        PrintLn("  mtype={}, channel={}, value={:.1f}",
            [record.mtype, record.channel, record.value])
        ++record
    endfor
endfunc

// Supplies an aligned buffer containing a header and two records.
struct SExamplePacket:
    header  : SDataHeader
    records : [2]SDataRec
endstruct

func *Main():
    var packet : SExamplePacket = {
        header: {version: 1, reccount: 2, location: 7, ?},
        records: [
            {mtype: 1, channel: 0, value: 21.5, ?},
            {mtype: 2, channel: 0, value: 45, ?}
        ]
    }
    // The structure literal can contain constants only. This is not a constant:
    packet.header.mtime = Time()

    PrintData(%packet, SizeOf(packet))
endfunc
```

Example output (the time varies because `Time()` supplies the current time):

```text
Location 7, time 2026-10-31 19:30:00, record count: 2
  mtype=1, channel=0, value=21.5
  mtype=2, channel=0, value=45.0
```

In `var header :? = ^SDataHeader(data)`, the cast converts the generic buffer
address to `^SDataHeader`, and `:?` tells the compiler to infer `header`'s type
from that expression. This allows `header.version` and `header.reccount` to
access the header's fields. The cast does not read or copy the whole header.

To find the records, `header[1]` selects the address one **header** past
`header`, equivalent to `header + 1`. Because `header` has type `^SDataHeader`,
this moves by `SizeOf(SDataHeader)`, or 32 bytes. Pointer indexing returns a
pointer, so `header[1]` does not read a second header; it only computes the
address where the records begin.

Similarly, `var record :? = ^SDataRec(header[1])` converts that address to
`^SDataRec` and infers that pointer type for `record`, letting us read a record's
fields. Then `++record` advances by one **record**, or 8 bytes. Both operations use the
pointed-to type's size: `header[1]` skips a header, while `record[1]` would
point to the next record.

Before accessing records, the function divides the remaining byte count by
`SizeOf(SDataRec)` to find how many complete records fit. It checks the announced
count against that capacity. Dividing the available space avoids multiplying
an unchecked record count, which could overflow. Try passing
`SizeOf(packet) - 1` to `PrintData`: it prints `Incomplete records` without
reading either record.

The caller must still supply a valid buffer of at least `size` bytes; the
function cannot verify that an arbitrary address refers to that much memory.
The pointers borrow `packet`'s storage and are used only during the call, while
`packet` is alive. No allocation or `delete` is needed. A binary format with a
different byte order or floating-point representation needs explicit decoding
rather than these direct pointer casts.

### Double Pointers and an argv-Style Dynamic List

A **double pointer**, such as `^^char`, points to a variable or array element
that itself holds a `^char`. This is useful for a list of pointers, such as a
C-style argument table (`argv`): each entry points to a zero-terminated string.
The strings can have different lengths and do not need to be adjacent in memory.

This example builds the strings in a dynamic array, then builds a separate
dynamic array of pointers to their text. It passes the address of the first
pointer slot as `^^char`, together with an argument count (`argc`). A final
`nil` entry also allows traversal without using the count.

```dq
use print

func PrintArgs(argc : int, argv : ^^char):
    for i : int = 0 count argc:
        var text : rostr = argv[i]^ // read a ^char from the pointer table
        PrintLn("argv[{}] = {}", [i, text])
    endfor

    var cursor : ^^char = argv
    Print("First characters:")
    while cursor^ <> nil:
        var textptr : ^char = cursor^
        Print(" {}", [textptr^])
        ++cursor // advance to the next ^char slot, not the next character
    endwhile
    PrintLn()
endfunc

func *Main():
    var arguments : [*]str = ["demo", "--color"]
    arguments.Append("blue")

    var pointers : [*]^char
    for i : int = 0 count arguments.length:
        pointers.Append(arguments[i].pchar)
    endfor
    pointers.Append(nil) // the final slot marks the end of the list

    var argv : ^^char = %pointers[0]
    PrintArgs(arguments.length, argv)

    var replacement : str = "green"
    argv[2]^ = replacement.pchar // replace a pointer in the table
    var selected : rostr = pointers[2]
    PrintLn("After replacement: {}", [selected])
    PrintLn("Original string: {}", [arguments[2]])
endfunc
```

Example output:

```text
argv[0] = demo
argv[1] = --color
argv[2] = blue
First characters: d - b
After replacement: green
Original string: blue
```

There are two levels of storage between `argv` and a character:

| Expression | Type | Meaning |
| --- | --- | --- |
| `argv` | `^^char` | Address of the first pointer slot |
| `argv[i]` | `^^char` | Address of pointer slot `i` |
| `argv[i]^` | `^char` | String address stored in slot `i` |
| `textptr^` | `char` | One character at the selected string address |

`++cursor` moves by `SizeOf(^char)`: one pointer slot, or 4 or 8 bytes depending
on the target. It does not move through the string's characters. To do that,
increment the inner `^char` pointer instead.

Assigning `argv[2]^` replaces the pointer stored in `pointers[2]`. It does not
copy text, modify the string `"blue"`, or replace `arguments[2]`. The `rostr`
variables borrow the selected zero-terminated text so `PrintLn` can print it
as a string rather than as an address.

The dynamic arrays manage their own storage. The pointer table borrows the
strings; it does not own them. Keep `arguments` and `replacement` alive while
their text pointers are used, and obtain fresh `.pchar` values after changing
the source strings. Finish building `pointers` before taking `%pointers[0]`:
resizing it can invalidate `argv` and `cursor`. The function expects `argc`
valid string pointers followed by a `nil` slot; a raw double pointer does not
carry the table's length.

### Using Double Pointers for Return Values

A function can return a value normally and supply another result through a
pointer parameter. To return a `^SList` this way, the parameter must be
`^^SList`: the address of the caller's pointer variable.

For example, `CollectHighReadings` builds a list of temperature readings at or
above a threshold. It returns the number of matching readings and writes the
new list's address through its last parameter, `outlist`.

Reuse `SNode` and `SList` from
[A Linked List with First and Last Pointers](#a-linked-list-with-first-and-last-pointers),
and replace that example's `Main` with the following code:

```dq
func CollectHighReadings(readings : []int, threshold : int, outlist : ^^SList) -> int:
    if outlist == nil or outlist^ <> nil:
        return -1 // require an output pointer whose current value is nil
    endif

    var list : ^SList = new SList
    list^ = {} // initialize first and last to nil
    var count : int = 0
    for reading : int in readings:
        if reading >= threshold:
            list.Append(reading)
            ++count
        endif
    endfor

    outlist^ = list // update the caller's pointer variable
    return count
endfunc

func *Main():
    var readings : [?]int = [22, 31, 24, 35]
    var alerts : ^SList = nil
    var count : int = CollectHighReadings(readings, 30, %alerts)
    if count < 0:
        PrintLn("Invalid output pointer")
        return
    endif

    PrintLn("High readings: {}", [count])
    var current : ^SNode = alerts.first
    while current <> nil:
        PrintLn("Reading: {}", [current.value])
        current = current.next
    endwhile

    alerts.Clear()      // release the list's nodes
    delete alerts = nil // release the SList itself and clear the pointer
endfunc
```

Example output:

```text
High readings: 2
Reading: 31
Reading: 35
```

`alerts` has type `^SList`, so `%alerts` has type `^^SList`. Inside the function,
`outlist^` accesses the caller's `alerts` variable. Assigning `outlist^ = list`
therefore changes `alerts` itself. A parameter of type `^SList` would only
receive a copy of its address; reassigning that parameter would not update
the caller's pointer.

The caller must initialize `alerts` to `nil`. The function returns `-1` without
changing it if the output parameter is `nil` or the pointer slot is already
occupied, avoiding overwriting an existing list. On success, the caller owns
the new list and its nodes, even when no readings match and the list is empty.
Call `Clear` before deleting the list, because deleting an `SList` alone does
not free its nodes.
