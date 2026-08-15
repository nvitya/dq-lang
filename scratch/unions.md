# Proposal: storage unions

## Recommendation

Use `union` for an untagged, C-compatible storage union.  The canonical form
declares a named union type, after which a struct or object uses it like any
other field type:

```dq
union UEpollData:
    ptr : pointer
    fd  : int
    u32 : uint32
    u64 : uint64
endunion

[[packed]] struct SEpollEvent:
    events : uint32
    data   : UEpollData
endstruct
```

Access stays explicit:

```dq
var event : SEpollEvent = {}
event.data.fd = 4
printf("%lu\n", event.data.u64)
```

Inside a struct or object, use the same `union name: ... endunion` form.  In
that context the name is the containing field name:

```dq
[[packed]] struct SEpollEvent:
    events : uint32
    union data:
        ptr : pointer
        fd  : int
        u32 : uint32
        u64 : uint64
    endunion
endstruct
```

The brace-style spelling follows naturally from DQ's other compound blocks:

```dq
struct SValue {
    union data {
        bytes : [8]byte
        u64   : uint64
    }
}
```

The closing word is aligned with the block beginner, like other DQ compound
blocks.  The nested declaration introduces a unique nested union type plus a
field named `data`; it should not have different layout or access rules from
the top-level form.

## Object fields

The same field syntax should work in objects:

```dq
object ODevice:
private
    union value:
        raw   : uint32
        words : [2]uint16
        bytes : [4]byte
    endunion

public
    function SetRaw(v : uint32):
        value.raw = v
    endfunc
endobj
```

Visibility applies to the containing field (`value`).  A union should not add
another object-style visibility system of its own.

## Meaning and layout

A storage union has no active-member tag.  All its fields begin at offset zero
and share the same bytes.

- Each member's offset is zero.
- Alignment is the maximum alignment of its members.
- Size is the maximum member size rounded up to the union alignment.
- The union's placement in a containing struct/object follows the ordinary
  field alignment and `[[packed]]` rules.
- `SizeOf(UEpollData)` and `OffsetOf(UEpollData, fd)` work as expected; every
  `OffsetOf` result for a direct union member is zero.
- Assigning unions of the same type copies the whole storage value.
- `{}` zero-initializes the whole storage, not only one selected member.
- Taking the address of a union or one of its members is allowed.

There is deliberately no runtime check when one member is written and another
is read.  This is the desired operation for C ABI declarations, hardware
register views, and bit reinterpretation.  As with typed pointers and external
ABI declarations, the programmer remains responsible for ensuring that the
bytes are a valid representation of the type being read.

For example:

```dq
union UWord:
    whole : uint64
    words : [2]uint32
    bytes : [8]byte
endunion

var word : UWord = {}
word.whole = 0x1122334455667788

// Element order here is target-endian, intentionally.
printf("%X\n", word.words[0])
```

## Initial restrictions

For the first version, union members should be limited to types with trivial
storage and lifetime:

- integers, floats, characters, enums, pointers, and function references;
- fixed arrays of permitted types;
- structs and other unions containing only permitted types.

Managed values (`str`, dynamic arrays, object references, or structs containing
them) and fixed object storage should be rejected.  Without an active-member
tag the compiler cannot know which value to initialize, retain, or destroy.
This restriction can be relaxed later only together with explicit lifetime
semantics.

I would also leave these out of the first version:

- inheritance;
- methods and properties on a union;
- per-member initializers;
- implicit conversion between a union and one of its members;
- implicit conversion between distinct union types with identical layout.

Keeping union assignment nominal and exact is consistent with DQ's strict type
system and avoids surprising overload resolution.

## Why qualify the members?

I do **not** recommend promoting nested union members into the containing
struct/object namespace:

```dq
// Not recommended for the initial design:
struct SBad:
    union:
        fd  : int
        ptr : pointer
    endunion
endstruct

// This would make value.fd and value.ptr appear to be ordinary SBad fields.
```

A named field such as `data` makes the storage relationship visible at every
use, avoids collisions with other fields and base members, and keeps member
lookup, `OffsetOf`, debug information, and module-interface serialization much
simpler.  It also maps cleanly to C: the extra qualification changes source
spelling, not ABI layout.

Anonymous member promotion could be added later as separate sugar if importing
C headers proves too noisy.  If added, a spelling such as `union _:` is
preferable because the anonymity is explicit:

```dq
struct SCCompatible:
    union _:
        fd  : int
        ptr : pointer
    endunion
endstruct
```

That extension should not be required for the core feature.

## Raw union versus a future tagged union

The keyword `union` should mean only overlapping storage.  It should not also
mean "one of these types" with compiler-tracked state.  A safe sum type has
different construction, matching, copy, and destruction rules and deserves a
separate feature and probably a separate keyword such as `variant`.

This distinction keeps code honest:

- `union` says "these fields reinterpret the same bytes";
- a future `variant` would say "exactly one alternative is currently active".

## Suggested implementation order

1. Add named `union UName: ... endunion` declarations and their layout/type
   representation.
2. Allow named union values as struct fields, object fields, locals, globals,
   parameters, and return values.
3. Add union member access, address-of, zero initialization, copying,
   `SizeOf`, and `OffsetOf`.
4. Serialize named union definitions in module interfaces and emit proper DWARF
   union debug information.
5. Add nested `union field: ... endunion` declarations.

This yields a useful C-interop feature after step 3, while nested declarations
can reuse the same union layout and member-access implementation.
