# Memory and Pointers

DQ has manual memory management, typed pointers, untyped pointers, object
references, and embedded object storage.

## Pointer Types

Typed pointer types are written with `^`.

```dq
var p : ^int = null
```

`pointer` is an untyped generic pointer. Cast it to a typed pointer before
dereferencing.

```dq
var raw : pointer = &value
var p : ^int = ^int(raw)
```

## Address and Dereference

`&` takes the address of an addressable value.

```dq
var value : int = 10
var p : ^int = &value
```

`^` dereferences a typed pointer.

```dq
p^ = 11
```

`null` is the null pointer value.

```dq
if p == null:
    return
endif
```

## Pointer Arithmetic

Typed pointers support pointer arithmetic.

```dq
var p : ^cchar = &text[0]
p += 1
var next : ^cchar = p + 1
```

Pointer difference is also used by low-level code.

```dq
var len : int = endp - startp
```

## Pointer Indexing

Typed pointer indexing returns a pointer value. It does not dereference the
element like C.

```dq
var p : ^cchar = &buf[0]
var second_ptr : ^cchar = p[1]
var second_char : cchar = p[1]^
```

## Struct Pointers

Pointers to structs may use either explicit dereference or automatic
dereference for member access.

```dq
sm^.id = 5
sm.id = 5
```

Both forms address the same field.

## Object References

Object variables are references.

```dq
var obj : OThing = null
obj = new OThing()
```

Object references can be assigned, compared with `null`, and passed to
functions. Assigning an object reference does not copy the object.

## Heap Allocation

Use `new` to allocate an object on the heap.

```dq
var obj : OThing = new OThing(1)
```

Use `delete` to destroy and release a heap object.

```dq
delete obj
```

When deleting through a base object reference, virtual destruction follows the
object type information supported by the compiler/runtime.

## Embedded Object Allocation

`<-` creates an embedded object in the containing storage location.

```dq
var local <- OThing()
```

Embedded objects are used for stack objects, global objects, and object members.

```dq
object OOwner:
    child <- OChild()
endobj
```

Embedded object references are valid while their containing storage is alive.

## C Strings

`cstring(n)` stores a fixed-size zero-terminated character buffer.

```dq
var buf : cstring(64) = "hello"
var p : ^cchar = &buf[0]
```

This type is intended for C interoperability and low-level text handling.

## Dynamic Storage Types

`str` and `[*]T` are heap-managed runtime types. They provide methods for
capacity management and mutation. They should normally be manipulated through
their methods instead of by treating their internals as raw memory.

## Manual Memory Rule of Thumb

Use:

- value types and structs for direct storage;
- `str` and `[*]T` for managed dynamic data;
- embedded objects with `<-` for owned object substructure;
- `new` and `delete` when heap object lifetime must be explicit;
- typed pointers for low-level code and C interoperability.
