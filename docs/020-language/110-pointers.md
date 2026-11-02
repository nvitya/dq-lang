# Pointers

A **pointer** stores a memory address, which identifies a location in memory.
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
var u32value : uint32     // an ordinary variable whose storage is managed by the compiler
u32value = 0x04030201     // assigns a value to the variable

var byteptr : ^byte       // declares a pointer variable of type `^byte`
byteptr = ^byte(%u32value) // points to the memory location where `u32value` is stored
byteptr^ = 0x55           // updates the first byte at that address

PrintLn('u32value = 0x{:08X}', [u32value])  // = "u32value = 0x04030255"
```

On a little-endian system, the first byte is the least significant byte of
`u32value`, so the example prints the result shown above.

Pointers are widely used and can improve performance significantly.

Using pointers can also be dangerous. There are many ways to end up with an invalid
pointer (one that points to an invalid memory location). Accessing data
through an invalid pointer can return invalid data or cause the application to crash.

Pointers are useful when working with C libraries, raw buffers, or hardware.
For an ordinary function that updates a
caller's variable, a `ref` parameter is usually simpler.

For a more practical example for using pointers see the
[Example: Processing a Sensor Packet](#example-processing-a-sensor-packet).

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

## Typed Pointers and Casting

A pointer's type tells the compiler how to access the data at its address:
how many bytes to read or write, which structure fields are available, and
how far to move during pointer arithmetic.

The type after `^` describes the pointed-to value. For example, `^byte` accesses
a single byte, `^int32` accesses a 4-byte signed integer, and `^float` accesses a
floating-point number occupying 4 or 8 bytes, depending on the target. The size
of the pointer itself is the same for all three types on a given target.

**Casting** converts a value from one type to another, or tells the compiler
to treat a value as a different type.

For pointers, casting usually changes the **pointer type** without changing
the address stored in the pointer. The same memory address is then interpreted
as pointing to a different data type.

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
    var byteptr  : ^byte = %buffer[0]
    var intptr   : ^int32 = ^int32(%buffer[0])
    var floatptr : ^float = ^float(%buffer[0])
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
order. The global buffer starts with zeroes. The loop writes the pattern through
`byteptr[i]^`, which accesses the byte at offset `i` without changing `byteptr`.
The following `byteptr^ = 42` changes only the first byte of that pattern.
The `int32` write changes the first 4 bytes to the two's-complement representation
of `-1000`, leaving `D4 D5 D6 D7` unchanged. On Linux x64, values are stored in
little-endian byte order, with the least significant byte first. The
floating-point write uses 8 bytes and shows the `float64` representation of
`1.5`. On targets with a 4-byte `float`, that write changes only the first
4 bytes. The final `uint64` write
always uses 8 bytes: `08 07 06 05 04 03 02 01` in little-endian byte order.

The global buffer has eight-byte alignment, requested by `[[align(8)]]`, so it
can hold any of these values at its starting address. `%buffer[0]` takes the
address of its first byte. The casts `^int32(%buffer[0])`, `^float(%buffer[0])`,
and `^uint64(%buffer[0])` keep that address and give it different pointer types.

All four pointers access the same storage. Each write overwrites the bytes
used by that type, and each read interprets those bytes according to the
pointer's type. Reading through `intptr^` after the floating-point write would
interpret the floating-point representation as an integer; it would not
convert `1.5` to an integer.

A generic `pointer` carries no information about the data it points to.
Cast it to a typed pointer before accessing that data:

```dq
var value : uint32 = 10
var raw   : pointer = %value

var address :? = ^uint32(raw)
address^ = 20 // value is now 20
```

Here, `^uint32(raw)` produces a pointer of type `^uint32`. The `:?` declaration
infers that type for `address`, so it does not need to be written twice.
The cast tells the compiler that the address refers to a `uint32`; it does
not check whether that is actually true.

A cast is also needed when changing between unrelated typed pointers. For
example, a `^uint32` cannot be assigned directly to a `^byte` variable. Cast
it explicitly to access individual bytes of the same storage:

```dq
var value : uint32 = 0x04030201
var wordptr : ^uint32 = %value
var byteptr :? = ^byte(wordptr)

byteptr^ = 0x55 // changes the first byte of value
```

`wordptr` and `byteptr` hold the same address, but `wordptr^` accesses 4 bytes
while `byteptr^` accesses 1 byte. On a little-endian system, `value` becomes
`0x04030255`. This changes the original variable's storage; the cast does not
create a separate byte variable.

You must ensure that the target storage is valid, large enough, and
properly aligned for the type used to access it. Casting does not extend the
storage's lifetime or give the pointer ownership of it.

Assigning a pointer copies its address. Both pointers then refer to the same
value, but either pointer can later be redirected independently:

```dq
var first  : int = 10
var second : int = 20
var current : ^int = %first
var other   : ^int = current

other^ = 15        // first is now 15; current^ also reads 15
current = %second  // redirects current; other still points to first
current^ = 25      // second is now 25; first remains 15
```

The declared type of `current` remains `^int` when its address changes. Using
a different pointed-to type requires an explicit cast.

For a structure pointer, the type also determines which fields can be accessed.
The dot operator dereferences the pointer automatically:

```dq
struct SReading:
    channel : byte
    value   : float32
endstruct

var reading : SReading = {channel: 1, value: 21.5}
var sensor  : ^SReading = %reading

var channel : byte = sensor.channel // reads reading.channel: 1
sensor.value = 22                   // changes reading.value to 22
```

Here, `sensor.value` is equivalent to `sensor^.value`. The compiler knows
the field's location and type because `sensor` has type `^SReading`.
See [Pointer Member Access](#pointer-member-access) for more details.

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

The pointed-to type also determines the step size of
[pointer arithmetic and indexing](#pointer-arithmetic-and-indexing): adding
1 to a `^byte` moves by 1 byte, while adding 1 to a `^uint32` moves by 4 bytes.

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
