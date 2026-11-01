# Pointers

A **pointer** stores the address of a value. It lets you access that value
through its location in memory.

Pointers are used frequently and many ways, they can improve a the running
speed a lot.

Using pointers can be dangerous too, there are many ways to get an invalid
pointer, a pointer pointing to invalid memory location, and accessing data
through this can deliver either invalid data or lead to application crash.

Pointers are useful when working with C libraries, raw buffers, or hardware.
For an ordinary function that updates a
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

This prints `42`.

`address^` and `value` access the same integer.

## Example: Processing a Sensor Packet

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

    var header : ^SDataHeader = ^SDataHeader(data)
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
    var record : ^SDataRec = ^SDataRec(header[1])
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

`^SDataHeader(data)` gives the buffer address a type, allowing `header.version`
and `header.reccount` to access its fields. The cast does not read or copy the
whole header.

To find the records, `header[1]` selects the address one **header** past
`header`, equivalent to `header + 1`. Because `header` has type `^SDataHeader`,
this moves by `SizeOf(SDataHeader)`, or 32 bytes. Pointer indexing returns a
pointer, so `header[1]` does not read a second header; it only computes the
address where the records begin.

Converting that address to `^SDataRec` lets us read a record's fields. Then
`++record` advances by one **record**, or 8 bytes. Both operations use the
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

var point   : SPoint = {x: 10, y: 20}
var address : ^SPoint = %point
address^.x = 30 // changes point.x
address.x  = 30 // changes point.x, . dereferences automatically
```

This convenience does not change who owns the structure or how long it exists.

## Pointer Arithmetic and Indexing

Adding an integer to a typed pointer moves by that many elements. Pointer
indexing also returns a **pointer**, so add `^` to access the selected value:

```dq
var readings : [3]int = [10, 20, 30]
var first    : ^int = %readings[0]
var second   : ^int = first + 1
var value    : int = first[1]^ // 20
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
