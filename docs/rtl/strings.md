# String Methods

DQ has three main text forms:

| Type | Meaning |
| --- | --- |
| `str` | dynamic heap-managed string |
| `strview` | non-owning string view |
| `cstring(n)` | fixed-size zero-terminated storage |

`str` is copy-on-write. Assigning a string shares storage until one copy is
mutated.

```dq
var a : str = "abc"
var b : str = a
b[0] = 'X'  // a is still "abc"
```

## Common Operations

Dynamic strings expose:

| Member | Meaning |
| --- | --- |
| `.length` | current character length |
| `.capacity` | allocated capacity |
| `Set(text)` | replace contents |
| `Append(value)` / `Add(value)` | append text or character |
| `AppendChar(ch)` | append one character |
| `Prepend(value)` | insert at the beginning |
| `Insert(index, value)` | insert text or character |
| `Delete(index, count = 1)` | remove characters |
| `SetLength(length, fill)` | resize, filling new characters |
| `Truncate(length)` | shrink only |
| `Pop(count)` / `PopFirst(count)` | remove and return text |
| `Pop()` / `PopFirst()` | remove and return one character |
| `Reserve(capacity)` | ensure capacity |
| `Compact()` | set capacity to current length |
| `Clear(free_storage = false)` | clear contents, optionally free storage |
| `Clone()` | force an independent copy |
| `AddFmt(fmt, args)` | append formatted text |

```dq
var s : str = "abc"
s.Append("def")
s.Insert(0, '>')
s.Delete(0)
s.AddFmt(" {}", [123])
```

Indices are normalized by the runtime operations. For example, deleting past the
end is clamped, and inserting with `$end` appends.

## `cstring`

`cstring(n)` owns fixed storage of `n` characters plus the zero terminator.
Appending and assigning truncate to fit.

```dq
var cs : cstring(5) = "abc"
cs.Append("def")  // stores "abcde"
```

`cstring` supports many of the same mutation methods as `str`, including `Set`,
`Append`, `Add`, `AppendChar`, `Prepend`, `Insert`, `Delete`, `Clear`, and
`AddFmt`. It does not support dynamic capacity operations such as `Reserve` or
`Compact`.

An unsized `cstring` is an alias/view over existing C string storage, commonly
used for parameters.

