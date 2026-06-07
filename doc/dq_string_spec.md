# DQ Strs

Status: draft
Scope: dynamic strings, string views, fixed-size C-style texts, character types, literals, indexing, slicing, mutation, copy-on-write handling, common string helpers, runtime representation

---

## Definitions

DQ has these string-related character/text forms in this draft:

```dq
char          // uint32 character value
cchar         // uint8 C-compatible character value
str           // dynamic refcounted copy-on-write character string
strview       // read-only non-owning view of character data
cstring(N)   // fixed-size C-compatible zero-terminated cchar storage
cstring      // non-owning mutable bounded C-string alias / fat pointer
^cchar        // pointer to zero-terminated C-compatible cchar storage
```

`char` is the normal DQ character type.

```dq
char == uint32
```

`cchar` is the C-compatible 8-bit character type.

```dq
cchar == uint8
```

A `str` stores `char` values. Its public length is measured in characters, not bytes.

A `strview` is a read-only, non-owning view of existing character data. Its public length is also measured in characters. A `strview` may refer to dynamic str storage, fixed `cstring(N)` storage, str literal storage, temporary compiler-generated source data, or external C-compatible zero-terminated storage after scanning.

A `cstring(N)` stores at most `N` logical `cchar` characters and has one hidden zero terminator byte. Therefore its actual storage size is `N + 1` bytes.

For a C-style buffer with raw storage size `maxlen`, the corresponding DQ type is `cstring(maxlen - 1)`, because one byte is reserved for the zero terminator.

```dq
var cs : cstring(31);  // 31 usable cchars, 32 bytes of storage
```

Unsized `cstring` is not a storage type. It is a non-owning mutable bounded C-string alias represented as a fat pointer / descriptor. It carries at least a data pointer, a maximum logical length, character width/encoding information, flags, and possibly a valid current length.

```dq
function Process(cs : cstring):  // receives a bounded mutable C-string alias
  cs.Append("x")
endfunc
```

`str` is the normal owning DQ text type.

`strview` is the normal non-owning read-only string source type for high-performance APIs.

`cstring(N)`, unsized `cstring`, and `^cchar` exist mainly for C interoperability.

## Type Overview

| Type form | Owns storage | Length | Storage location | Character width | Can resize | Mutability |
|---|---:|---:|---|---|---:|---|
| `str` | yes | runtime | heap manager, null for empty | 1, 2, or 4 bytes per character | yes | mutable with copy-on-write |
| `strview` | no | runtime | borrowed/external/static | 1, 2, or 4 bytes per character | no | read-only |
| `cstring(N)` | yes | runtime, max `N` | inline/static/local/object storage | 1 byte | up to max length | mutable |
| `cstring` | no | known or lazily scanned, max carried | borrowed bounded C-string storage | 1 byte in this draft | no | mutable alias when writable |
| `^cchar` | no | zero-terminated | external/static/C-owned storage | 1 byte | no | pointer may target mutable or read-only storage |

A `str` is internally a nullable reference to a dynamic string manager object.

A null string manager is not an invalid string. It represents the canonical empty string.

```dq
var s1 : str;
var s2 : str = "";
var s3 : str = '';

s1.length == 0
s2.length == 0
s3.length == 0
s1 == ""
s2 == ""
s3 == ""
```

Direct null comparison is not allowed at the language level:

```dq
s == null   // compile error
s <> null   // compile error
```

Use empty-string comparison or length instead:

```dq
s == ""
s <> ""
s.length == 0
```

A `strview` is not nullable as a language value. An empty `strview` has length zero.

```dq
var v : strview = "";

v.length == 0
v == ""
```

The internal pointer of an empty `strview` may be null or may point to static empty literal storage. User code must not depend on that pointer value.

## Dynamic `str` Semantics

A `str` is a refcounted, copy-on-write, mutable character sequence.

String assignment shares the manager object:

```dq
var a : str = "abc";
var b : str = a;
```

After assignment, both variables may reference the same internal manager:

```text
a.manager == b.manager
manager.refcount == 2
```

Before any modification, the runtime ensures that the target string variable has unique writable storage.

```dq
var a : str = "abc";
var b : str = a;

b[0] = 'X';

// a == "abc"
// b == "Xbc"
```

Therefore, unlike dynamic arrays, modifying one string variable does not modify another string variable previously assigned from it.

Visible semantic rule:

```text
String assignment is cheap and shares storage.
String modification is value-like and detaches when the manager is shared.
```

---

## Dynamic String Storage Width

A dynamic string manager stores all characters using one fixed storage width per manager:

```text
charwidth = 1  if every stored character value fits into uint8
charwidth = 2  if every stored character value fits into uint16
charwidth = 4  otherwise
```

The public `char` type remains `uint32` regardless of the internal storage width.

```dq
var s : str = "abc";  // may use charwidth = 1
var ch : char = s[0];    // returns uint32 character value
```

Indexing is O(1), because the string uses fixed-width character storage internally.

Appending or assigning a wider character may widen the internal storage:

```dq
var s : str = "abc";  // charwidth = 1

s.Append('€');           // may widen to charwidth = 2
s.Append(char(0x1F600)); // may widen to charwidth = 4
```

If the manager is uniquely owned, widening may happen in place. If the manager is shared, widening happens while detaching to a new manager.

Narrowing should not happen automatically after every modification. Explicit compaction may narrow:

```dq
var s : str = "abc€";  // charwidth = 2
s.Delete(3);              // "abc", still may be charwidth = 2
s.Compact();              // may shrink capacity and narrow to charwidth = 1
```

---

## Length and Capacity

For `str`, both `length` and `capacity` are measured in characters.

```dq
var s : str;

s.length    // 0
s.capacity  // 0
```

For an allocated dynamic string manager:

```text
allocated bytes = capacity * charwidth
```

`Reserve(n)` ensures that the string has capacity for at least `n` characters.

```dq
var s : str;

s.Reserve(4096);
s.Append("abc");
```

After `Reserve()` on an empty string, the string may have an allocated manager with zero length and non-zero capacity:

```text
s.length   == 0
s.capacity >= 4096
```

At the language level, both of these are empty strings:

```text
null manager, length 0, capacity 0
allocated manager, length 0, capacity > 0
```

---

## String Literals

String literals are compile-time string source values.

A non-empty string literal is emitted as static read-only character data in `.rodata`. The character data always contains a trailing zero character after the logical characters. The trailing zero is not included in the literal character length.

The compiler also emits a valid static read-only `SDqTextInfo` descriptor for a literal when the literal is used as a `str`, `strview`, `cstring(N)`, or string-helper source.

```dq
var s  : str      = "asdf";
var v  : strview     = "asdf";
var cs : cstring(31) = "asdf";
```

These uses can lower to the literal view and do not require zero-terminator scanning.

```text
dq_lit_asdf_data:
  'a' 's' 'd' 'f' 0

align pointer
dq_lit_asdf_view:
  dataptr = &dq_lit_asdf_data
  charlen = 4
  info    = maxlen(4) | WIDTH1 | READONLY | ZEROTERM | LENGTH_VALID
```

The view descriptor may be placed separately from the character data so that it can be pointer-aligned. The data object and view object may both be pooled and deduplicated by the compiler/linker.

A string literal used as `^cchar` points directly to the zero-terminated character data:

```dq
var pc : ^cchar = "asdf";  // points to read-only .rodata character data
```

This is valid only when the literal can be represented with `charwidth == 1`. Wider literals require explicit conversion before they can be passed as `^cchar`.

```dq
var pc1 : ^cchar = "abc";  // OK
var pc2 : ^cchar = "€";    // compile error or explicit conversion required
```

For wider literals, the compiler chooses the minimum required storage width:

```text
charwidth = 1  if every literal character value fits into uint8
charwidth = 2  if every literal character value fits into uint16
charwidth = 4  otherwise
```

A wider literal still has a trailing zero codepoint in its own storage width, but that storage is not a C `char*` string.

Literal storage is read-only. Modifying a `str` value that was created from a literal never modifies `.rodata`.

Empty string literals used as `str` values use the null-manager representation:

```dq
var s1 : str = "";
var s2 : str = '';
```

Empty string literals used as `strview` values use a canonical empty view:

```dq
var v : strview = "";
```

As `^cchar` values, empty string literals point to a static zero byte in `.rodata`:

```dq
var pc : ^cchar = "";  // points to read-only storage containing one zero byte
```

A non-empty literal assigned to `str` may be copied into a normal dynamic string manager immediately, or the runtime may assign from the static literal view.

```dq
var s : str = "abc";  // assign from static literal view
s.Append("def");        // helper receives static literal view, no scan
```

A later optimization may represent string literals as static read-only string managers. Any modification of a read-only/static manager detaches first.

```dq
var s : str = "abc";
s[0] = 'X';  // creates writable storage first if the value references static/read-only storage
```

A `char` value can be used as a one-character string source where a string source is accepted:

```dq
var s : str = 'a';
s.Append('b');
s.Insert(1, 'x');
```

A one-character `char` source may be passed internally as a temporary `strview`/`SDqTextInfo` whose lifetime is limited to the generated helper call.

## String Indexing

String indexing is strict. The index must refer to an existing character.

```dq
var s : str = "abc";

var c : char;

c = s[0];      // OK, 'a'
c = s[2];      // OK, 'c'
c = s[-1];     // runtime bounds error
c = s[3];      // runtime bounds error
c = s[$end];   // runtime bounds error
```

For a string with length `L`, valid indexes are:

```text
0 .. L - 1
```

`$last` and `$end` have the same meaning as for arrays:

```text
$last = int(s.length) - 1
$end  = int(s.length)
```

For an empty string:

```text
$last == -1
$end  == 0
```

Therefore:

```dq
var s : str = "";
s[$last];  // runtime bounds error
s[$end];   // runtime bounds error
```

---

## `str` Character Assignment

A character of `str` is assignable.

```dq
var s : str = "abc";

s[0] = 'X';  // "Xbc"
s[2] = 'Y';  // "XbY"
```

Character assignment follows strict indexing rules:

```dq
s[-1] = 'X';    // runtime bounds error
s[s.length] = 'X'; // runtime bounds error
s[$end] = 'X';  // runtime bounds error
```

Character assignment may detach, widen, or reallocate the string manager before writing:

```dq
var a : str = "abc";
var b : str = a;

b[0] = 'X';

// a == "abc"
// b == "Xbc"
```

Writing a wider character may widen storage:

```dq
var s : str = "abc";
s[1] = char(0x1F600);  // may widen to charwidth = 4
```

---

## String Slicing

Normal `str` slicing returns a new `str` value.

DQ also has public read-only `strview` values. A string slice expression may produce a `strview` only when the target/context explicitly requires `strview`.

```dq
var s : str = "abcdef";

var x : str  = s[1:4];   // copies: "bcd"
var v : strview = s[1:4];   // view: no copy, read-only, non-owning
```

Slicing syntax follows the array slicing syntax:

```dq
s[start:end]    // half-open interval, end is excluded
s[start::end]   // closed interval, end is included
```

Empty bounds mean the start or end of the string:

```dq
s[:]      // copy/view of the whole string, depending on target/context
s[2:]     // from index 2 to the end
s[:3]     // from the beginning to index 3 excluded
s[2::]    // from index 2 to the last character, inclusive form
```

Slicing is forgiving. Invalid slice bounds are clamped to the actual string bounds.

```dq
var s : str = "abcde";

s[-5:]      // "abcde"
s[:100]     // "abcde"
s[100:]     // ""
s[4:2]      // ""
s[$end:]    // ""
s[$last:]   // "e"
```

For the half-open form:

```dq
s[start:end]
```

normalization is:

```text
start = clamp(start, 0, length)
end   = clamp(end,   0, length)

if end < start:
  end = start
```

For the inclusive form:

```dq
s[start::end]
```

normalization is equivalent to converting the inclusive end to a half-open end:

```text
s[start::end] == s[start : end + 1]
```

The conversion must avoid integer overflow in the compiler/runtime implementation.

A `strview` slice returns another `strview`, because the receiver is already non-owning:

```dq
var v1 : strview = "abcdef";
var v2 : strview = v1[1:4];  // view of "bcd"
```

A slice expression passed directly to a helper or to a parameter of type `strview` may be lowered as a temporary `strview` without creating a dynamic `str`:

```dq
function ParseToken(tok : strview):
  ...
endfunc

var s : str = "abcdef";

ParseToken(s[1:4]);  // temporary strview
s.Append(s[1:4]);    // source is passed as temporary strview, no intermediate str required
```

The temporary view is read-only and is valid only for the duration guaranteed by the call site.

## Dynamic String Assignment and Aliasing

Whole string assignment shares the string manager:

```dq
var a : str = "abc";
var b : str = a;
```

Modification detaches if needed:

```dq
b.Append("def");

// a == "abc"
// b == "abcdef"
```

Assignment from a string source creates or shares a string value depending on the source:

```dq
var s1 : str = "abc";     // str from literal
var s2 : str = s1;        // shares manager
var s3 : str = s1[1:3];   // new str "bc"
```

Assignment to the empty string releases the current manager reference and returns to the canonical null-manager empty string:

```dq
var s : str = "abc";
s = "";  // release manager reference, s becomes null-manager empty
```

This is different from `Clear()`:

```dq
s.Clear();      // length = 0, keep capacity if uniquely owned
s.Clear(true);  // release storage, return to null-manager empty
s = "";         // release manager reference, return to null-manager empty
```

---

## Dynamic String Operations

Dynamic strings support mutating operations similar to dynamic arrays, but with copy-on-write semantics. Source arguments are normally lowered to read-only `strview` values.

```dq
var s : str = "abc";

// add characters
s.Append('d');        // "abcd"
s.Append("ef");      // "abcdef"
s.Append(strview_value); // source view is copied into s
s.Prepend('X');      // "Xabcdef"
s.Prepend("--");     // "--Xabcdef"
s.Insert(1, 'Y');    // insert before normalized index 1
s.Insert(2, "zz");   // insert string source

// remove characters
s.Delete(0, 1);       // delete first character
s.Delete(1, 2);       // delete two characters starting at index 1

var tail  : str = s.Pop(5);      // remove and return up to 5 trailing characters
var head  : str = s.PopFirst(5); // remove and return up to 5 leading characters
var last  : char = s.Pop();         // optional shorthand: remove and return last character
var first : char = s.PopFirst();    // optional shorthand: remove and return first character

// common non-mutating helpers
var t  : str = s.Trim();
var lp : str = s.LPad(10, ' ');
var rp : str = s.RPad(10, '.');
var ix : int    = s.IndexOf("bc");

// size and storage management
s.SetLength(10, ' '); // resize, growing with explicit fill character
s.Truncate(5);        // equivalent to reducing length to at most 5
s.Reserve(4096);      // ensure capacity >= 4096 characters
s.Compact();          // capacity = length, may narrow charwidth
s.Clear();            // set length to 0, keep capacity when possible
s.Clear(true);        // release storage and become null-manager empty

// explicit copy
var copy : str = s.Clone();

var i : int = s.length;
var c : int = s.capacity;
```

All mutating operations first ensure unique writable storage.

```text
if manager == null:
  allocate a manager if the operation creates non-empty content or reserves capacity

if manager.refcount > 1 or manager is read-only/static:
  allocate a new manager and copy the required characters

if required charwidth > manager.charwidth:
  widen storage

if required capacity > manager.capacity:
  grow storage

perform the operation
```

### Append and Prepend

`Append(source)` adds characters to the end of the string.

```dq
var s : str = "ab";

s.Append('c');     // "abc"
s.Append("def");  // "abcdef"
```

`Prepend(source)` adds characters to the beginning of the string.

```dq
var s : str = "cd";

s.Prepend('b');    // "bcd"
s.Prepend("a");   // "abcd"
```

### Insert and Delete Index Normalization

`Insert()` and `Delete()` do not produce index-bounds runtime errors. Their index argument is clamped to the valid insertion/deletion position range:

```text
index = clamp(index, 0, length)
```

`Insert(index, source)` inserts before the normalized index. Inserting before index `0` prepends. Inserting before index `length` appends.

```dq
var s : str = "abc";

s.Insert(0, 'X');      // "Xabc"
s.Insert(100, 'Y');    // "abcY"
s.Insert(-5, 'Z');     // "Zabc"
s.Insert($end, '!');   // "abc!"
```

`Delete(index, count = 1)` removes characters from the normalized index. If `count <= 0`, the operation does nothing. If the requested range extends past the end of the string, only the existing tail characters are deleted.

```text
index        = clamp(index, 0, length)
count        = max(count, 0)
actual_count = min(count, length - index)
```

Examples:

```dq
var s : str = "abcde";

s.Delete(1);       // "acde"
s.Delete(100);     // unchanged
s.Delete(1, 100);  // "a"
s.Delete(-5, 1);   // "bcde"
s.Delete(2, 0);    // unchanged
```

### SetLength and Truncate

`SetLength(new_length, fillchar)` changes the logical length of the string.

Growing requires an explicit fill character:

```dq
var s : str = "abc";

s.SetLength(5, ' ');  // "abc  "
s.SetLength(2, ' ');  // "ab"
s.SetLength(0, ' ');  // ""
```

There is intentionally no hidden default fill character.

`Truncate(new_length)` only shrinks the string. If `new_length >= length`, it does nothing.

```dq
var s : str = "abcdef";

s.Truncate(3);   // "abc"
s.Truncate(100); // unchanged
```

### Capacity Rules

`Reserve(n)` ensures that capacity is at least `n` characters. It does not promise exact capacity.

```dq
s.Reserve(1000);  // capacity >= 1000
```

`Compact()` sets capacity to the current length and may narrow the character storage width.

```dq
s.Compact();      // capacity = length, charwidth may become smaller
```

`Clear()` sets length to zero and keeps the current capacity when the manager is uniquely owned.

`Clear(true)` releases storage and returns to the null-manager empty string.

### Clone

`Clone()` returns an independent dynamic string with copied characters.

```dq
var a : str = "abc";
var b : str = a.Clone();

b[0] = 'X';

// a == "abc"
// b == "Xbc"
```

Because strings already use copy-on-write, `Clone()` is usually not needed for correctness. It is useful when the programmer explicitly wants independent storage.

Rules:

- `Clone()` copies `length` characters.
- The result has `capacity == length`.
- The result may use the minimum required `charwidth`.
- `Clone()` of an empty string returns the canonical null-manager empty string.

### Pop and PopFirst

`Pop(count)` removes and returns up to `count` trailing characters as a new `str`.

`PopFirst(count)` removes and returns up to `count` leading characters as a new `str`.

The `count` argument is measured in characters, not bytes.

```dq
var s : str = "abcdef";

var tail : str = s.Pop(2);       // tail = "ef", s = "abcd"
var head : str = s.PopFirst(3);  // head = "abc", s = "d"
```

Counted pop operations are clamped and do not produce index-bounds runtime errors:

```text
count        = max(count, 0)
actual_count = min(count, length)
```

Therefore:

```dq
var s : str = "abc";

s.Pop(0);       // returns "", s unchanged
s.Pop(-5);      // returns "", s unchanged
s.Pop(100);     // returns "abc", s becomes ""
```

The returned substring preserves the original character order.

The no-argument forms may be supported as shorthand single-character operations:

```dq
var s : str = "abc";

var last  : char = s.Pop();       // last = 'c', s = "ab"
var first : char = s.PopFirst();  // first = 'a', s = "b"
```

Calling the no-argument `Pop()` or `PopFirst()` on an empty string is a runtime error, because there is no character to return.

`PopFirst(count)` and `PopFirst()` are O(n) operations because the remaining characters are moved toward the beginning of the string.

---

## String Utility Methods

DQ strings should support a small standard helper set inspired by common JavaScript text operations, but adapted to DQ's character-based indexing and mutable/copy-on-write string model.

These helpers are non-mutating unless explicitly documented otherwise. They return new `str` values or scalar results. To modify a variable, assign the result back to it:

```dq
var s : str = "  abc  ";
s = s.Trim();  // "abc"
```

The methods are valid on `str` and `strview`. Read-only helper methods may also be valid on `cstring(N)` and `^cchar` sources where the receiver can be converted to a `strview`.

### Trim, LTrim, and RTrim

`Trim()` removes leading and trailing whitespace and returns the result as a new `str`.

`LTrim()` removes only leading whitespace.

`RTrim()` removes only trailing whitespace.

```dq
var s : str = "  abc  ";

s.Trim();   // "abc"
s.LTrim();  // "abc  "
s.RTrim();  // "  abc"
```

The default whitespace set is the ASCII whitespace set:

```text
' ', '\t', '\n', '\r', '\v', '\f'
```

Overloads with an explicit trim-character set may be supported:

```dq
var s : str = "---abc---";

s.Trim("-");   // "abc"
s.LTrim("-");  // "abc---"
s.RTrim("-");  // "---abc"
```

The explicit trim set is interpreted as a set of characters, not as a substring pattern.

### LPad and RPad

`LPad(target_length, fill)` pads the left side of the string until the result reaches `target_length` characters.

`RPad(target_length, fill)` pads the right side of the string until the result reaches `target_length` characters.

```dq
var s : str = "abc";

s.LPad(5, ' ');   // "  abc"
s.RPad(5, '.');   // "abc.."
s.LPad(2, ' ');   // "abc", already long enough
```

The target length is measured in characters.

The fill source may be a `char` or a string source:

```dq
var s : str = "abc";

s.LPad(8, "01");  // "01010abc"
s.RPad(8, "01");  // "abc01010"
```

If the fill source is longer than the required padding, it is repeated and then truncated to the exact required padding length.

An empty fill source is a runtime error, because padding cannot make progress.

### IndexOf and Related Search Helpers

`IndexOf(needle, start = 0)` returns the index of the first occurrence of `needle`, or `-1` when not found.

```dq
var s : str = "abcdefabc";

s.IndexOf('c');       // 2
s.IndexOf("abc");    // 0
s.IndexOf("abc", 1); // 6
s.IndexOf("xyz");    // -1
```

The `start` argument is measured in characters and is clamped to `0 .. length`.

An empty search string is found at the normalized start position:

```dq
var s : str = "abc";

s.IndexOf("", 0);    // 0
s.IndexOf("", 2);    // 2
s.IndexOf("", 100);  // 3
```

Additional search helpers are recommended as library-level operations:

```dq
s.LastIndexOf(needle);       // last occurrence, or -1
s.Contains(needle);          // bool
s.StartsWith(prefix);        // bool
s.EndsWith(suffix);          // bool
```

All search indexes and lengths are character-based.

---

## Overlapping Source Rules

String operations must correctly handle overlapping source and destination ranges.

```dq
var s : str = "abc";

s.Append(s);       // "abcabc"
s.Append(s[1:]);   // valid
s.Prepend(s);      // "abcabc"
s.Insert(1, s);    // valid
```

A `strview` source expression may reference the same manager as the destination.

Before any possible detach, widening, or reallocation, the runtime/compiler must preserve enough information to reconstruct the source range after the destination storage changes.

Recommended internal model:

```text
If the source belongs to the destination manager:
  save source character index and source character length before mutation.

After detach/reallocation/gap opening:
  reconstruct the source pointer from the current destination data pointer and saved index,
  or use a split-copy algorithm when the source range crosses the insertion point.
```

For `Insert()` where the source range overlaps the insertion point, the runtime may use the same split-copy strategy as dynamic arrays.

Example:

```dq
var s : str = "abcd";

s.Insert(2, s[1:3]);  // "abbccd"
```

---

## `cstring(N)` and Unsized `cstring`

`cstring(N)` is fixed-size inline C-compatible zero-terminated storage.

The `N` in `cstring(N)` is the maximum logical text length, not the raw storage size.

For a raw C buffer with `maxlen` bytes, the matching DQ type is `cstring(maxlen - 1)`, because one byte is reserved for the zero terminator.

```dq
var cs : cstring(31);
```

This means:

```text
maximum logical length = 31 cchars
actual storage size    = 32 bytes
storage[N]             = hidden zero terminator position when full
```

A `cstring(N)` stores `cchar` values. `cchar` is `uint8`.

```dq
cchar == uint8
```

A `cstring(N)` always maintains zero termination.

```dq
var cs : cstring(5) = "abc";
```

Possible storage:

```text
'a' 'b' 'c' 0 ? ?
```

The bytes after the first terminator are not part of the logical string. The implementation may zero them or leave them unspecified.

Unsized `cstring` is different from `cstring(N)`:

```text
cstring(N) = fixed inline storage, N usable cchars, N + 1 bytes total
cstring    = non-owning mutable bounded C-string alias / fat pointer
```

Unsized `cstring` may be used as a function parameter and may also be used as a local alias initialized from an existing `cstring(N)` or another unsized `cstring` value.

```dq
function Process(cs : cstring):
  cs.Append("x");
endfunc

function Example():
  var storage : cstring(31);
  var alias   : cstring = storage;  // alias/fat pointer, no new storage

  alias.Append("abc");             // modifies storage
endfunc
```

A standalone unsized `cstring` declaration has no storage and is invalid:

```dq
var a : cstring(31);     // OK: creates storage
var b : cstring = a;     // OK: alias to existing storage
var c : cstring;         // compile error: no target storage
var d : cstring = "abc"; // compile error: no writable target storage
```

Assignment between unsized `cstring` values creates another alias to the same descriptor/buffer. It does not copy text and does not create an independent fixed buffer.

### cstring ABI Descriptor

The ABI descriptor for unsized `cstring` and for internal string-source views is `SDqTextInfo`:

```dq
struct SDqTextInfo:
  dataptr : pointer;
  charlen : uint32;
  info    : uint32;  // maxlen + width/encoding + flags
endstruct
```

Recommended `info` layout:

```text
bits  0..23  maxlen       // maximum logical writable length, max 16_777_215
bits 24..25  width        // 0 = width1, 1 = width2, 2 = width4, 3 = reserved/utf8
bits 26..31  flags
```

Recommended flags:

```text
LENGTH_VALID  // charlen may be trusted
READONLY      // storage must not be modified
WRITABLE      // storage may be modified
ZEROTERM      // a zero terminator is required/maintained
STATIC        // points to static program storage
TEMPORARY     // valid only for the current helper call
```

For a `cstring` descriptor, `dataptr`, `maxlen`, width, and zero-termination contract must be valid. `charlen` may be trusted only when `LENGTH_VALID` is set.

If `LENGTH_VALID` is not set, helpers that need the current length must scan from `dataptr` until the first zero terminator or until `maxlen` is reached. After scanning, they update `charlen` and set `LENGTH_VALID`.

```text
if not flags has LENGTH_VALID:
  charlen = bounded_zero_scan(dataptr, maxlen)
  flags += LENGTH_VALID
```

Mutating helpers that change the logical length update `charlen`, preserve zero termination, and keep `LENGTH_VALID` set.

### cstring Properties

```dq
var cs : cstring(31) = "abc";

cs.length       // 3
cs.maxlength    // 31
cs.storage_size // 32
```

`length` is the number of cchars before the first zero terminator, limited to `maxlength`.

If the implementation has a valid descriptor with `LENGTH_VALID`, `length` is O(1). Otherwise it is computed by bounded scanning.

`maxlength` is the declared maximum logical length `N` for `cstring(N)` or the carried maximum length for unsized `cstring`.

`storage_size` is `N + 1` for `cstring(N)`.

### Hidden cstring Descriptor Optimization

A `cstring(N)` value is still only fixed inline storage in the visible data layout. The compiler may maintain a hidden unsized `cstring` / `SDqTextInfo` descriptor for standalone local `cstring(N)` variables when useful.

```dq
function Example():
  var cs : cstring(31);  // initialized to empty string

  cs.Append("one");
  cs.Append(" two");
  cs.Append(" three");
endfunc
```

This may lower to:

```text
cs_storage[0] = 0

cs_info.dataptr = &cs_storage[0]
cs_info.charlen = 0
cs_info.maxlen  = 31
cs_info.flags   = WRITABLE | ZEROTERM | WIDTH1 | LENGTH_VALID

CStrAppend(ref cs_info, "one")
CStrAppend(ref cs_info, " two")
CStrAppend(ref cs_info, " three")
```

The repeated append operations do not need to rescan the buffer because the descriptor length is known and kept valid.

This hidden descriptor is not part of the public layout of `cstring(N)`. It should not be added permanently to struct fields, object fields, array elements, or externally visible storage, because that would enlarge records and make raw copying incorrect.

For `cstring(N)` fields and array elements, the compiler normally creates a temporary descriptor at the use site:

```dq
struct STestRec:
  id   : int32;
  name : cstring(31);
endstruct

function ProcessName(ref tr : STestRec):
  tr.name.Append(" two");
endfunc
```

The call inside `ProcessName()` may lower to:

```text
tmp.dataptr = &tr.name[0]
tmp.charlen = bounded_zero_scan(tmp.dataptr, 31)
tmp.maxlen  = 31
tmp.flags   = WRITABLE | ZEROTERM | WIDTH1 | LENGTH_VALID

CStrAppend(ref tmp, " two")
```

If a standalone local `cstring(N)` buffer is passed to unknown external C code through a raw `^cchar` pointer, the compiler must assume that the external code may modify the contents and therefore clear `LENGTH_VALID` in the hidden descriptor.

```dq
SomeCFunction(&cs[0]);  // may modify the bytes
cs.Append("x");        // refresh length first if LENGTH_VALID was cleared
```

External functions may later be annotated as read-only or length-preserving to avoid invalidation.

### cstring Assignment

Assignment copies source text into the fixed storage, silently truncates to `maxlength`, and always writes the zero terminator.

```dq
var cs : cstring(5);

cs = "abcdefghi";

// logical content: "abcde"
// storage bytes:   'a' 'b' 'c' 'd' 'e' 0
```

Assignment from `^cchar` copies until the source zero terminator or until `maxlength` characters have been copied.

```dq
var pc : ^cchar;
var cs : cstring(31);

pc = "asdf";
cs = pc;  // copies from zero-terminated cchar storage
```

Assignment from `str` copies character values into `cchar` storage. Characters that do not fit into `cchar` are a runtime conversion error in this draft. Length overflow is silently truncated.

```dq
var s  : str = "abcdef";
var cs : cstring(3);

cs = s;  // "abc"
```

A future library may provide explicit lossy conversion helpers if desired.

### cstring Indexing

`cstring(N)` and unsized `cstring` indexing use the same logical indexing rules as normal strings.

The hidden zero terminator is not part of the logical string and is not reachable through normal indexing.

```dq
var cs : cstring(31) = "abc";

cs[0]       // OK, 'a'
cs[2]       // OK, 'c'
cs[$last]   // OK, 'c'

cs[-1]      // runtime bounds error
cs[3]       // runtime bounds error
cs[$end]    // runtime bounds error
```

`cs[31]` is not valid merely because the storage has 32 bytes. Indexing is based on logical length, not raw storage size.

```dq
var cs : cstring(31) = "abc";

cs[31];  // runtime bounds error
```

When indexing an unsized `cstring` whose descriptor does not have `LENGTH_VALID`, the runtime first scans up to `maxlen` to recover the logical length.

### cstring Character Assignment

A `cstring(N)` or unsized `cstring` character is assignable inside the current logical length.

```dq
var cs : cstring(31) = "abc";

cs[0] = 'X';  // "Xbc"
cs[2] = 'Y';  // "XbY"
```

Character assignment follows strict indexing rules:

```dq
cs[3] = 'Z';     // runtime bounds error
cs[$end] = 'Z';  // runtime bounds error
```

Assigning a zero cchar terminates the string at that position:

```dq
var cs : cstring(31) = "abcdef";

cs[3] = cchar(0);

// logical content: "abc"
// length == 3
```

A character assigned to `cstring(N)` or unsized `cstring` must fit into `cchar`. Otherwise it is a runtime conversion error.

### cstring Slicing

`cstring(N)` and unsized `cstring` slicing use the same slicing rules as normal strings.

A `cstring(N)` slice expression returns a new `str` by default. In an explicit `strview` context it may return a read-only non-owning view into the fixed cstring storage.

```dq
var cs : cstring(31) = "abcdef";

var s1 : str  = cs[1:4];       // copies "bcd"
var s2 : str  = cs[:];         // copies "abcdef"
var s3 : str  = cs[4:100];     // copies "ef"
var v1 : strview = cs[1:4];    // view of "bcd"
```

Slice bounds are clamped exactly like string slice bounds:

```dq
cs[-5:]      // whole logical cstring as str/view, depending on context
cs[:100]     // whole logical cstring as str/view, depending on context
cs[100:]     // ""
cs[$last:]   // last logical cchar as str/view, depending on context
```

When slicing an unsized `cstring` whose descriptor does not have `LENGTH_VALID`, the runtime first scans up to `maxlen` to recover the logical length.

### cstring Mutating Methods

`cstring(N)` and unsized `cstring` may support a restricted `str`-like API.

Growth operations silently truncate to `maxlength` and always maintain zero termination.

```dq
var cs : cstring(5) = "abc";

cs.Append('d');      // "abcd"
cs.Append("efghi"); // "abcde"
cs.Prepend('X');     // "Xabcd", tail truncated if needed
cs.Insert(1, 'Y');   // insert before index 1, tail truncated if needed
cs.Delete(1, 2);     // delete two logical characters
cs.Clear();          // empty cstring, storage[0] = 0
```

Read-only helper methods such as `Trim()`, `LTrim()`, `RTrim()`, `LPad()`, `RPad()`, `IndexOf()`, `Contains()`, `StartsWith()`, and `EndsWith()` may also be supported on `cstring(N)` and unsized `cstring` through `strview` conversion. Methods that produce text return a new `str`, not a `cstring(N)`.

```dq
var cs : cstring(8) = "  abc";

cs.Trim();        // returns string "abc"
cs.IndexOf('b');  // returns 3
```

`Reserve()`, `Compact()`, and dynamic capacity operations are not valid on `cstring(N)` or unsized `cstring`, because their storage size is fixed by the target buffer.

```dq
cs.Reserve(100);  // compile error
cs.Compact();     // compile error
```

---

## `^cchar` C String Pointers

`^cchar` is a pointer to zero-terminated 8-bit C-compatible character storage.

```dq
var pc : ^cchar;
pc = "asdf";
```

Assigning a string literal to `^cchar` points to static read-only zero-terminated literal character data in `.rodata`. This is valid only for literals with `charwidth == 1`.

```dq
var pc : ^cchar = "asdf";
```

Copying from `^cchar`, or converting `^cchar` to `strview`, scans until the first zero terminator.

```dq
var s  : str;
var cs : cstring(31);
var pc : ^cchar = "asdf";

s  = pc;  // scans and creates dynamic string "asdf"
cs = pc;  // scans and copies into fixed cstring storage
var v : strview = pc;  // scans once to create a view
```

Passing a null `^cchar` where a valid C string is required is a runtime error in this draft.

```dq
var pc : ^cchar = null;
var s : str = pc;  // runtime error
```

A future standard library may provide helpers that treat null C-style texts as empty strings if desired.

---

## Equality and Empty Checks

String equality compares logical character content.

```dq
var a : str = "abc";
var b : str = "abc";

if a == b:
  // true
endif
```

Empty comparison:

```dq
s == ""   // s.length == 0
s <> ""   // s.length <> 0
```

`cstring(N)` and `strview` equality with `str` compares logical content:

```dq
var s  : str = "abc";
var cs : cstring(31) = "abc";
var v  : strview = "abc";

s == cs  // true
cs == s  // true
s == v   // true
v == s   // true
```

Comparison with `^cchar` may be supported by scanning the zero-terminated C string:

```dq
var pc : ^cchar = "abc";

s == pc  // true, if ^cchar comparison is enabled
```

If enabled, comparison with null `^cchar` is a runtime error.

---

## Strings as Function Arguments

A function parameter of type `str` receives an owning string value. Passing an `str` shares the manager and increments the refcount if needed.

```dq
function PrintName(name : str):
  printf(name);
endfunc
```

Inside the function, modifying the parameter detaches locally and does not modify the caller's variable:

```dq
function F(s : str):
  s.Append("x");  // modifies only local parameter value
endfunc

var a : str = "abc";
F(a);

// a == "abc"
```

To allow a function to modify the caller's string variable, pass it by `ref` or `refout`:

```dq
function AppendSuffix(s : ref str):
  s.Append("_suffix");
endfunc

var a : str = "abc";
AppendSuffix(a);

// a == "abc_suffix"
```

A function parameter of type `strview` receives a read-only non-owning view. This avoids allocation and is the recommended parameter type for read-only string processing.

```dq
function ParseName(name : strview):
  ...
endfunc

var s  : str = "Alice";
var cs : cstring(31) = "Bob";
var pc : ^cchar = "Carol";

ParseName("Alice");  // static literal view, no scan
ParseName(s);        // view of dynamic string storage
ParseName(s[1:4]);   // temporary slice view
ParseName(cs);       // view of fixed cstring storage
ParseName(pc);       // scans zero-terminated cchar storage to form a view
```

A `strview` parameter must not be stored beyond the lifetime guaranteed by the caller unless the function explicitly documents that the caller must provide persistent storage.

A function should use `strview` for read-only text, unsized `cstring` for mutable bounded C-compatible buffers, and `^cchar` for raw C APIs that only accept a zero-terminated pointer.

```dq
function ReadText(s : strview):
  ...
endfunc

function EditCBuffer(cs : cstring):
  cs.Append("x");
endfunc

function CallC(cs : ^cchar):
  SomeCFunction(cs);
endfunc
```

A `cstring` parameter receives a non-owning descriptor. The caller must provide valid storage and a valid maximum length. The current length is trusted only when the descriptor has `LENGTH_VALID`; otherwise helpers scan lazily up to the maximum length.

```dq
function CsProcess(acs : cstring):
  var cs : cstring = acs;  // alias to the same descriptor/buffer
  cs.Append(" four");
endfunc

function Test():
  var cs1 : cstring(31) = "one";
  CsProcess(cs1);          // cs1 becomes "one four"
endfunc
```

A local unsized `cstring` variable is an alias to an existing descriptor/buffer, not a fixed buffer and not a value copy. It must be initialized from an existing `cstring(N)` or unsized `cstring`.

## Public `strview`, Unsized `cstring`, and ABI `SDqTextInfo`

`strview` is a public DQ type for a read-only, non-owning view of character data.

Unsized `cstring` is a public DQ type for a non-owning mutable bounded C-compatible string buffer alias.

Both concepts use the same compact ABI descriptor shape, `SDqTextInfo`, but they have different language-level permissions.

```dq
struct SDqTextInfo:
  dataptr : pointer;
  charlen : uint32;
  info    : uint32;  // maxlen + width/encoding + flags
endstruct
```

The exact padding and alignment are target ABI details. Static `SDqTextInfo` objects in `.rodata` should be pointer-aligned.

```text
dataptr = pointer to the first stored character
charlen = number of logical characters when LENGTH_VALID is set
maxlen  = maximum logical length encoded in info
width   = character storage width encoded in info
flags   = descriptor flags encoded in info
```

For read-only `strview` descriptors, `LENGTH_VALID` is normally set and `maxlen == charlen`. For writable unsized `cstring` descriptors, `maxlen` is the writable maximum logical length of the target C buffer and `charlen` may be valid or unknown depending on `LENGTH_VALID`.

For empty views:

```text
dataptr = null or static empty storage
charlen = 0
maxlen  = 0
width   = 1
flags   = READONLY | LENGTH_VALID
```

A `strview` can represent source data from:

```dq
"abc"       // string literal, view points into .rodata
s           // dynamic string
cs          // cstring(N), as read-only view
pc          // ^cchar, after scanning length
s[1:4]      // string slice expression in strview context
v[1:4]      // strview slice expression
'a'         // one-character temporary source
```

`strview` is strictly read-only.

```dq
var v : strview = "abc";

var ch : char = v[0];  // OK
v[0] = 'X';            // compile error
v.Append("x");         // compile error
v.Delete(0);           // compile error
```

An unsized `cstring` is a mutable alias to an existing bounded C-compatible buffer.

```dq
function Process(cs : cstring):
  cs.Append("x");       // OK, modifies the target buffer
endfunc

function Example():
  var storage : cstring(31) = "abc";
  var alias   : cstring = storage;

  alias[0] = 'X';        // storage == "Xbc"
endfunc
```

`strview` and unsized `cstring` indexing is strict, like `str` indexing:

```dq
var v : strview = "abc";

v[0];      // OK
v[2];      // OK
v[3];      // runtime bounds error
v[$end];   // runtime bounds error
```

`strview` slicing is forgiving, like `str` slicing, and returns another `strview`:

```dq
var v : strview = "abcdef";

v[1:4];    // strview "bcd"
v[:100];   // strview "abcdef"
v[100:];   // empty strview
```

A `strview` can be assigned to `str`, which copies the viewed characters into an owning dynamic string value:

```dq
var v : strview = "abc";
var s : str = v;  // copies "abc" into str value
```

A `str` can be assigned to `strview`, which creates a view of the current `str` storage:

```dq
var s : str = "abcdef";
var v : strview = s;  // non-owning read-only view of s storage
```

This has the same lifetime and invalidation dangers as array slices. The view does not keep the source alive and does not increment a dynamic string manager refcount by itself.

Danger example:

```dq
var s : str = "abcdef";
var v : strview = s[1:4];

s.Append("...");  // may detach/reallocate/move s storage

// v may now be invalid
```

Rules for `strview`:

- `strview` does not own the referenced character storage.
- `strview` does not increment `str` refcounts by itself.
- `strview` cannot be used to mutate the referenced storage.
- `strview` may become invalid when the source storage is destroyed, resized, detached, moved, or otherwise invalidated.
- `strview` may be stored or returned only with the same care as pointers or array slices.
- Assigning `strview` to `str` copies the characters and produces safe owning storage.

Rules for unsized `cstring`:

- `cstring` does not own the referenced character storage.
- `cstring` carries or can recover the current logical length.
- `cstring` carries the maximum logical writable length.
- `cstring` operations must preserve zero termination.
- `cstring` assignment aliases the same descriptor/buffer; it does not copy text.
- `cstring` may become invalid when the target storage goes out of scope, is moved, or is otherwise invalidated.
- A default local `var cs : cstring;` is invalid because there is no target storage.

Recommended compiler diagnostics:

```dq
function BadView() -> strview:
  var cs : cstring(31) = "abc";
  return cs[:];  // should be compile error or warning: returns view into local storage
endfunc

function BadCStr() -> cstring:
  var cs : cstring(31) = "abc";
  return cs;  // should be compile error or warning: returns cstring alias to local storage
endfunc
```

The compiler does not need to prove all `strview` or unsized `cstring` lifetimes safe. Both are explicit advanced borrowed types.

## Dynamic String Runtime Handling

Dynamic strings use a hidden refcounted heap manager object:

```dq
object ODynStrMgr:
  refcount  : uint;
  dataptr   : pointer;
  length    : uint32;  // character count
  capacity  : uint32;  // character capacity
  charwidth : uint8;   // 1, 2, or 4 bytes per character
  flags     : uint8;   // static/read-only/immortal/etc. if needed
endobj
```

The user-visible `str` variable is internally a nullable reference to `ODynStrMgr`:

```dq
var s : str;  // internally: manager reference = null
```

A null manager represents the canonical empty string:

```dq
s.length   == 0
s.capacity == 0
s == ""
```

Operations that need storage allocate a manager automatically.

Mutating string helper functions must generally receive the manager reference by reference because they may need to rebind the string variable after allocation, detach, widening, releasing, or reallocation. Source arguments should normally be passed as read-only `strview` values. At ABI level, read-only string sources and mutable bounded `cstring` aliases use the `SDqTextInfo` descriptor shape.

```dq
function DqStrSetChar(
  smgr  : ref ^ODynStrMgr,
  index : int,
  ch    : char
);

function DqStrAssign(
  smgr : ref ^ODynStrMgr,
  src  : refin strview
);

function DqStrAppend(
  smgr : ref ^ODynStrMgr,
  src  : refin strview
);

function DqStrInsert(
  smgr  : ref ^ODynStrMgr,
  index : int,
  src   : refin strview
);

function DqStrDelete(
  smgr  : ref ^ODynStrMgr,
  index : int,
  count : int = 1
);

function DqStrPop(
  smgr  : ref ^ODynStrMgr,
  count : int
) -> str;

function DqStrPopChar(
  smgr : ref ^ODynStrMgr
) -> char;

function DqStrTrim(
  src        : refin strview,
  trim_chars : refin strview
) -> str;

function DqStrIndexOf(
  src    : refin strview,
  needle : refin strview,
  start  : int = 0
) -> int;

function DqStrReserve(
  smgr        : ref ^ODynStrMgr,
  mincapacity : uint32
);

function DqStrClear(
  smgr            : ref ^ODynStrMgr,
  release_storage : bool = false
);
```

Before a write or structural modification, helpers perform writable-storage preparation:

```text
function EnsureWritable(required_length, required_charwidth):
  if smgr == null:
    allocate new manager if required_length > 0 or capacity is requested
    return

  if smgr.refcount > 1 or smgr.flags has READONLY/STATIC:
    allocate new manager
    copy existing characters
    decrement old manager refcount
    bind variable to new manager

  if required_charwidth > smgr.charwidth:
    widen storage

  if required_length > smgr.capacity:
    grow storage
```

Allocation failure or capacity overflow must not silently corrupt the string. The operation must raise a runtime error or exception. The original string should remain unchanged whenever practical.

All calculations of `capacity * charwidth` must be checked for integer overflow.

---

## C String Runtime Handling

C-string helper functions operate on an unsized `cstring` descriptor. At ABI level this is `SDqTextInfo` with writable, zero-terminated, width-1 storage in this draft.

```dq
function DqCStrRefreshLength(
  cs : ref cstring
);

function DqCStrAssign(
  cs  : ref cstring,
  src : refin strview
);

function DqCStrAppend(
  cs  : ref cstring,
  src : refin strview
);

function DqCStrPrepend(
  cs  : ref cstring,
  src : refin strview
);

function DqCStrInsert(
  cs    : ref cstring,
  index : int,
  src   : refin strview
);

function DqCStrDelete(
  cs    : ref cstring,
  index : int,
  count : int = 1
);

function DqCStrClear(
  cs : ref cstring
);
```

Helpers that need the current logical length first ensure a valid length:

```text
function EnsureCStrLength(cs):
  if cs.flags has LENGTH_VALID:
    return

  cs.charlen = bounded_zero_scan(cs.dataptr, cs.maxlen)
  cs.flags += LENGTH_VALID
```

Mutating helpers follow these rules:

```text
1. EnsureCStrLength(cs) when the operation needs the current length.
2. Clamp insertion/deletion indexes like dynamic string Insert/Delete.
3. Copy only as many cchars as fit into cs.maxlen.
4. Always write a zero terminator at cs.dataptr[cs.charlen].
5. Update cs.charlen.
6. Keep LENGTH_VALID set.
```

Source arguments are normally passed as `strview`. If the source is a `cstring` whose `LENGTH_VALID` flag is not set, the source descriptor is refreshed before copying or searching.

Overlapping source and destination ranges must be handled correctly. If a `cstring` source aliases the destination buffer, the helper must preserve enough source range information before moving bytes or truncating the destination.

A raw `^cchar` does not carry `maxlen`, so it is not a valid mutable `cstring` argument by itself. To create an unsized `cstring` alias from a raw buffer, the programmer or caller must provide the maximum logical length explicitly through a library helper or another typed wrapper.

---

## Character Lifetime and Initialization Rules

String characters are unmanaged scalar values.

For dynamic strings:

| Operation | Required character handling |
|---|---|
| append/insert/prepend from source | copy and widen source character values as needed |
| delete/truncate/shrink | adjust length and move remaining characters when needed |
| grow by `SetLength()` | fill new characters with explicit fill character |
| clear | set length to 0 |
| manager destruction | free character storage and manager storage |
| assignment to `""` | release manager reference; if refcount reaches 0, free manager contents |
| reallocation | move/copy existing character values |
| clone | allocate capacity equal to length and copy characters |

No type-info handler functions are needed for characters.

---

## Recommended Core Rules

1. `char` is `uint32`.
2. `cchar` is `uint8`.
3. `str` is a refcounted, copy-on-write dynamic character string.
4. `strview` is a public read-only non-owning view of character data.
5. Unsized `cstring` is a public non-owning mutable bounded C-string alias / fat pointer.
6. `strview` and unsized `cstring` use the ABI descriptor shape `SDqTextInfo` with `dataptr`, `charlen`, and packed `info`.
7. `SDqTextInfo.info` carries maximum length, character width/encoding, and flags.
8. `LENGTH_VALID` means `charlen` may be trusted. If it is not set, helpers must scan lazily when they need the current length.
9. Empty dynamic strings use a null manager and allocate no storage.
10. `str.length`, `str.capacity`, and `strview.length` are measured in characters.
11. Dynamic string storage uses one fixed width per manager: 1, 2, or 4 bytes per character.
12. String assignment shares the manager.
13. Any string modification first ensures unique writable storage.
14. Modifying one string variable never modifies another string variable assigned from it.
15. `str` and `strview` indexing are strict and invalid indexes cause runtime bounds errors.
16. `s[i] = ch` is valid for `str` and may detach, widen, or reallocate the manager.
17. `v[i] = ch` is invalid for `strview` because views are read-only.
18. Normal `str` slicing returns a new `str`, not a view.
19. A string slice expression may produce a `strview` only when the target/context explicitly requires `strview`.
20. `strview` slicing returns another `strview`.
21. `str` and `strview` slice bounds are clamped like array slice bounds.
22. String literals emit zero-terminated read-only character data in `.rodata`.
23. String literals used as `str` sources also emit static read-only `SDqTextInfo` descriptors in `.rodata`.
24. A string literal can be used directly as `^cchar` only when its `charwidth` is 1.
25. Source arguments to string helpers are normally passed as `strview` / `SDqTextInfo`.
26. `Append()`, `Prepend()`, `Insert()`, `Delete()`, `SetLength()`, `Truncate()`, `Reserve()`, `Compact()`, `Clear()`, `Clone()`, `Pop(count)`, and `PopFirst(count)` are valid on dynamic strings.
27. `Insert()` and `Delete()` clamp their index arguments and do not produce index-bounds runtime errors.
28. `Pop(count)` and `PopFirst(count)` clamp their count argument and return a `str`.
29. No-argument `Pop()` and `PopFirst()` may be supported as single-character shorthands and are runtime errors on empty strings.
30. `Trim()`, `LTrim()`, `RTrim()`, `LPad()`, `RPad()`, `IndexOf()`, `LastIndexOf()`, `Contains()`, `StartsWith()`, and `EndsWith()` are recommended common string helpers.
31. Read-only string helpers should accept `strview` sources where practical.
32. `cstring(N)` stores at most `N` logical `cchar` characters and uses `N + 1` bytes of storage.
33. `cstring(N)` always maintains a hidden zero terminator.
34. `cstring(N)` is the only form that creates fixed inline C-string storage.
35. Plain `var cs : cstring;` is invalid because unsized `cstring` has no storage.
36. A local `var cs : cstring = existing;` is an alias to an existing `cstring(N)` or unsized `cstring` descriptor/buffer.
37. A function parameter `cs : cstring` receives a bounded mutable descriptor; `maxlen` must be valid and `charlen` is valid only with `LENGTH_VALID`.
38. Assignment to `cstring(N)` silently truncates by length and always zero-terminates.
39. `cstring(N)` and unsized `cstring` indexing use logical string rules; the hidden terminator is not indexable.
40. `cstring(N)` slicing returns a new `str` by default and may produce `strview` in explicit `strview` context.
41. `cstring(N)` mutating methods may use a hidden compiler-maintained descriptor for standalone local variables.
42. Hidden cstring descriptors are not part of public storage layout and should not be permanently inserted into struct fields, object fields, array elements, or externally visible records.
43. Passing a `cstring(N)` buffer to unknown external C code through `^cchar` invalidates known length information unless the call is annotated as read-only or length-preserving.
44. `^cchar` is a non-owning pointer to zero-terminated 8-bit C-compatible storage.
45. Converting `^cchar` to `strview` requires scanning until the first zero terminator.
46. `strview` and unsized `cstring` have the same lifetime and invalidation dangers as array slices or raw pointers.
