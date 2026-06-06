# DQ Strings

Status: draft
Scope: dynamic strings, fixed-size C strings, character types, indexing, slicing, mutation, copy-on-write handling, runtime representation

---

## Definitions

DQ has three string-related character/text forms in this draft:

```dq
char          // uint32 character value
cchar         // uint8 C-compatible character value
string        // dynamic refcounted copy-on-write character string
cstring[N]    // fixed-size C-compatible zero-terminated cchar storage
^cchar        // pointer to zero-terminated C-compatible cchar storage
```

`char` is the normal DQ character type.

```dq
char == uint32
```

A `string` stores `char` values. The public length of a string is measured in characters, not bytes.

A `cstring[N]` stores at most `N` logical `cchar` characters and has one hidden zero terminator byte. Therefore its actual storage size is `N + 1` bytes.

```dq
var cs : cstring[31];  // 31 usable cchars, 32 bytes of storage
```

`cstring[N]` exists mainly for C interoperability. `string` is the normal DQ text type.

---

## Type Overview

| Type form | Owns storage | Length | Storage location | Character width | Can resize |
|---|---:|---:|---|---|---:|
| `string` | yes | runtime | heap manager, null for empty | 1, 2, or 4 bytes per character | yes |
| `cstring[N]` | yes | runtime, max `N` | inline/static/local/object storage | 1 byte | up to max length |
| `^cchar` | no | zero-terminated | external/static/C-owned storage | 1 byte | no |

A `string` is internally a nullable reference to a dynamic string manager object.

A null string manager is not an invalid string. It represents the canonical empty string.

```dq
var s1 : string;
var s2 : string = "";
var s3 : string = '';

s1.length == 0
s2.length == 0
s3.length == 0
s1 == ""
s2 == ""
s3 == ""
```

Direct null comparison is not allowed at the language level:

```dq
s == null  // compile error
s <> null  // compile error
```

Use empty-string comparison or length instead:

```dq
s == ""
s <> ""
s.length == 0
```

---

## Dynamic String Semantics

A `string` is a refcounted, copy-on-write, mutable character sequence.

String assignment shares the manager object:

```dq
var a : string = "abc";
var b : string = a;
```

After assignment, both variables may reference the same internal manager:

```text
a.manager == b.manager
manager.refcount == 2
```

Before any modification, the runtime ensures that the target string variable has unique writable storage.

```dq
var a : string = "abc";
var b : string = a;

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
var s : string = "abc";  // may use charwidth = 1
var ch : char = s[0];    // returns uint32 character value
```

Indexing is O(1), because the string uses fixed-width character storage internally.

Appending or assigning a wider character may widen the internal storage:

```dq
var s : string = "abc";  // charwidth = 1

s.Append('€');           // may widen to charwidth = 2
s.Append(char(0x1F600)); // may widen to charwidth = 4
```

If the manager is uniquely owned, widening may happen in place. If the manager is shared, widening happens while detaching to a new manager.

Narrowing should not happen automatically after every modification. Explicit compaction may narrow:

```dq
var s : string = "abc€";  // charwidth = 2
s.Delete(3);              // "abc", still may be charwidth = 2
s.Compact();              // may shrink capacity and narrow to charwidth = 1
```

---

## Length and Capacity

For `string`, both `length` and `capacity` are measured in characters.

```dq
var s : string;

s.length    // 0
s.capacity  // 0
```

For an allocated dynamic string manager:

```text
allocated bytes = capacity * charwidth
```

`Reserve(n)` ensures that the string has capacity for at least `n` characters.

```dq
var s : string;

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

String literals are source string data that can initialize or be assigned to `string`, `cstring[N]`, or `^cchar` depending on context.

```dq
var s  : string      = "asdf";
var cs : cstring[32] = "asdf";
var pc : ^cchar;

pc = "asdf";
```

Empty string literals used as `string` values use the null-manager representation:

```dq
var s1 : string = "";
var s2 : string = '';
```

A non-empty literal may initially be implemented by allocating a normal string manager. A later optimization may represent string literals as static read-only managers. Any modification of a read-only/static manager detaches first.

```dq
var s : string = "abc";
s[0] = 'X';  // creates writable storage first if the literal is static/read-only
```

A `char` value can be used as a one-character string source where a string source is accepted:

```dq
var s : string = 'a';
s.Append('b');
s.Insert(1, 'x');
```

---

## String Indexing

String indexing is strict. The index must refer to an existing character.

```dq
var s : string = "abc";

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
var s : string = "";
s[$last];  // runtime bounds error
s[$end];   // runtime bounds error
```

---

## String Character Assignment

A string character is assignable.

```dq
var s : string = "abc";

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
var a : string = "abc";
var b : string = a;

b[0] = 'X';

// a == "abc"
// b == "Xbc"
```

Writing a wider character may widen storage:

```dq
var s : string = "abc";
s[1] = char(0x1F600);  // may widen to charwidth = 4
```

---

## String Slicing

DQ has no public string slice/view type.

A string slice expression returns a new `string` value.

```dq
var s : string = "abcdef";

var x : string = s[1:4];   // "bcd"
var y : string = s[1::3];  // "bcd"
```

Slicing syntax follows the array slicing syntax:

```dq
s[start:end]    // half-open interval, end is excluded
s[start::end]   // closed interval, end is included
```

Empty bounds mean the start or end of the string:

```dq
s[:]      // copy of the whole string
s[2:]     // from index 2 to the end
s[:3]     // from the beginning to index 3 excluded
s[2::]    // from index 2 to the last character, inclusive form
```

Slicing is forgiving. Invalid slice bounds are clamped to the actual string bounds.

```dq
var s : string = "abcde";

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

A slice expression has public type `string`, but the compiler may internally lower it to `SDqStrView` when it is immediately consumed by a string runtime helper.

```dq
var s : string = "abcdef";

var x : string = s[1:4];  // creates a string value
s.Append(s[1:4]);         // compiler may pass an internal temporary view to Append
```

---

## Dynamic String Assignment and Aliasing

Whole string assignment shares the string manager:

```dq
var a : string = "abc";
var b : string = a;
```

Modification detaches if needed:

```dq
b.Append("def");

// a == "abc"
// b == "abcdef"
```

Assignment from a string source creates or shares a string value depending on the source:

```dq
var s1 : string = "abc";     // string from literal
var s2 : string = s1;        // shares manager
var s3 : string = s1[1:3];   // new string "bc"
```

Assignment to the empty string releases the current manager reference and returns to the canonical null-manager empty string:

```dq
var s : string = "abc";
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

Dynamic strings support mutating operations similar to dynamic arrays, but with copy-on-write semantics.

```dq
var s : string = "abc";

// add characters
s.Append('d');        // "abcd"
s.Append("ef");      // "abcdef"
s.Prepend('X');      // "Xabcdef"
s.Prepend("--");     // "--Xabcdef"
s.Insert(1, 'Y');    // insert before normalized index 1
s.Insert(2, "zz");   // insert string source

// remove characters
s.Delete(0, 1);       // delete first character
s.Delete(1, 2);       // delete two characters starting at index 1

var last  : char = s.Pop();      // remove and return last character
var first : char = s.PopFirst(); // remove and return first character

// size and storage management
s.SetLength(10, ' '); // resize, growing with explicit fill character
s.Truncate(5);        // equivalent to reducing length to at most 5
s.Reserve(4096);      // ensure capacity >= 4096 characters
s.Compact();          // capacity = length, may narrow charwidth
s.Clear();            // set length to 0, keep capacity when possible
s.Clear(true);        // release storage and become null-manager empty

// explicit copy
var copy : string = s.Clone();

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
var s : string = "ab";

s.Append('c');     // "abc"
s.Append("def");  // "abcdef"
```

`Prepend(source)` adds characters to the beginning of the string.

```dq
var s : string = "cd";

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
var s : string = "abc";

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
var s : string = "abcde";

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
var s : string = "abc";

s.SetLength(5, ' ');  // "abc  "
s.SetLength(2, ' ');  // "ab"
s.SetLength(0, ' ');  // ""
```

There is intentionally no hidden default fill character.

`Truncate(new_length)` only shrinks the string. If `new_length >= length`, it does nothing.

```dq
var s : string = "abcdef";

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
var a : string = "abc";
var b : string = a.Clone();

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

`Pop()` removes and returns the last character.

```dq
var s : string = "abc";
var ch : char = s.Pop();  // ch = 'c', s = "ab"
```

`PopFirst()` removes and returns the first character.

```dq
var s : string = "abc";
var ch : char = s.PopFirst();  // ch = 'a', s = "bc"
```

Calling `Pop()` or `PopFirst()` on an empty string is a runtime error, because there is no character to return.

`PopFirst()` is an O(n) operation because the remaining characters are moved one position toward the beginning of the string.

---

## Overlapping Source Rules

String operations must correctly handle overlapping source and destination ranges.

```dq
var s : string = "abc";

s.Append(s);       // "abcabc"
s.Append(s[1:]);   // valid
s.Prepend(s);      // "abcabc"
s.Insert(1, s);    // valid
```

A string source expression may reference the same manager as the destination.

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
var s : string = "abcd";

s.Insert(2, s[1:3]);  // "abbccd"
```

---

## `cstring[N]`

`cstring[N]` is fixed-size inline C-compatible zero-terminated storage.

The `N` in `cstring[N]` is the maximum logical text length, not the raw storage size.

```dq
var cs : cstring[31];
```

This means:

```text
maximum logical length = 31 cchars
actual storage size    = 32 bytes
storage[N]             = hidden zero terminator position when full
```

A `cstring[N]` stores `cchar` values. `cchar` is `uint8`.

```dq
cchar == uint8
```

A `cstring[N]` always maintains zero termination.

```dq
var cs : cstring[5] = "abc";
```

Possible storage:

```text
'a' 'b' 'c' 0 ? ?
```

The bytes after the first terminator are not part of the logical string. The implementation may zero them or leave them unspecified.

### cstring Properties

```dq
var cs : cstring[31] = "abc";

cs.length       // 3
cs.maxlength    // 31
cs.storage_size // 32
```

`length` is the number of cchars before the first zero terminator, limited to `maxlength`.

`maxlength` is the declared maximum logical length `N`.

`storage_size` is `N + 1`.

### cstring Assignment

Assignment copies source text into the fixed storage, silently truncates to `maxlength`, and always writes the zero terminator.

```dq
var cs : cstring[5];

cs = "abcdefghi";

// logical content: "abcde"
// storage bytes:   'a' 'b' 'c' 'd' 'e' 0
```

Assignment from `^cchar` copies until the source zero terminator or until `maxlength` characters have been copied.

```dq
var pc : ^cchar;
var cs : cstring[32];

pc = "asdf";
cs = pc;  // copies from zero-terminated cchar storage
```

Assignment from `string` copies character values into `cchar` storage. Characters that do not fit into `cchar` are a runtime conversion error in this draft. Length overflow is silently truncated.

```dq
var s  : string = "abcdef";
var cs : cstring[3];

cs = s;  // "abc"
```

A future library may provide explicit lossy conversion helpers if desired.

### cstring Indexing

`cstring[N]` indexing uses the same logical indexing rules as normal strings.

The hidden zero terminator is not part of the logical string and is not reachable through normal indexing.

```dq
var cs : cstring[31] = "abc";

cs[0]       // OK, 'a'
cs[2]       // OK, 'c'
cs[$last]   // OK, 'c'

cs[-1]      // runtime bounds error
cs[3]       // runtime bounds error
cs[$end]    // runtime bounds error
```

`cs[31]` is not valid merely because the storage has 32 bytes. Indexing is based on logical length, not raw storage size.

```dq
var cs : cstring[31] = "abc";

cs[31];  // runtime bounds error
```

### cstring Character Assignment

A `cstring[N]` character is assignable inside the current logical length.

```dq
var cs : cstring[31] = "abc";

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
var cs : cstring[31] = "abcdef";

cs[3] = cchar(0);

// logical content: "abc"
// length == 3
```

A character assigned to `cstring[N]` must fit into `cchar`. Otherwise it is a runtime conversion error.

### cstring Slicing

`cstring[N]` slicing uses the same slicing rules as normal strings.

A `cstring[N]` slice expression returns a new `string`, not a `cstring` and not a view.

```dq
var cs : cstring[31] = "abcdef";

var s1 : string = cs[1:4];    // "bcd"
var s2 : string = cs[:];      // "abcdef"
var s3 : string = cs[4:100];  // "ef"
```

Slice bounds are clamped exactly like string slice bounds:

```dq
cs[-5:]      // whole logical cstring as string
cs[:100]     // whole logical cstring as string
cs[100:]     // ""
cs[$last:]   // last logical cchar as string
```

### cstring Mutating Methods

`cstring[N]` may support a restricted string-like API.

Growth operations silently truncate to `maxlength` and always maintain zero termination.

```dq
var cs : cstring[5] = "abc";

cs.Append('d');      // "abcd"
cs.Append("efghi"); // "abcde"
cs.Prepend('X');     // "Xabcd", tail truncated if needed
cs.Insert(1, 'Y');   // insert before index 1, tail truncated if needed
cs.Delete(1, 2);     // delete two logical characters
cs.Clear();          // empty cstring, storage[0] = 0
```

`Reserve()`, `Compact()`, and dynamic capacity operations are not valid on `cstring[N]`, because its storage size is fixed.

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

Assigning a string literal to `^cchar` points to static read-only zero-terminated literal storage.

```dq
var pc : ^cchar = "asdf";
```

Copying from `^cchar` scans until the first zero terminator.

```dq
var s  : string;
var cs : cstring[32];
var pc : ^cchar = "asdf";

s  = pc;  // creates dynamic string "asdf"
cs = pc;  // copies into fixed cstring storage
```

Passing a null `^cchar` where a valid C string is required is a runtime error in this draft.

```dq
var pc : ^cchar = null;
var s : string = pc;  // runtime error
```

A future standard library may provide helpers that treat null C strings as empty strings if desired.

---

## Equality and Empty Checks

String equality compares logical character content.

```dq
var a : string = "abc";
var b : string = "abc";

if a == b:
  // true
endif
```

Empty comparison:

```dq
s == ""   // s.length == 0
s <> ""   // s.length <> 0
```

`cstring[N]` equality with `string` compares logical content:

```dq
var s  : string = "abc";
var cs : cstring[31] = "abc";

s == cs  // true
cs == s  // true
```

Comparison with `^cchar` may be supported by scanning the zero-terminated C string:

```dq
var pc : ^cchar = "abc";

s == pc  // true, if ^cchar comparison is enabled
```

If enabled, comparison with null `^cchar` is a runtime error.

---

## Strings as Function Arguments

A function parameter of type `string` receives a string value. Passing a string shares the manager and increments the refcount if needed.

```dq
function PrintName(name : string):
  printf(name);
endfunc
```

Inside the function, modifying the parameter detaches locally and does not modify the caller's variable:

```dq
function F(s : string):
  s.Append("x");  // modifies only local parameter value
endfunc

var a : string = "abc";
F(a);

// a == "abc"
```

To allow a function to modify the caller's string variable, pass it by `ref` or `refout`:

```dq
function AppendSuffix(s : ref string):
  s.Append("_suffix");
endfunc

var a : string = "abc";
AppendSuffix(a);

// a == "abc_suffix"
```

A function should use `cstring[N]`, `^cchar`, or explicit conversion helpers for C interop.

```dq
function CallC(cs : ^cchar):
  SomeCFunction(cs);
endfunc
```

---

## Internal `SDqStrView`

The compiler/runtime may use an internal string view structure for passing source string data to helper functions.

```dq
struct SDqStrView:
  dataptr   : pointer;
  charlen   : uint32;
  charwidth : uint8;
endstruct
```

`SDqStrView` is not a public DQ string slice type.

It is an internal, non-owning view of character data.

```text
dataptr   = pointer to the first stored character
charlen   = number of characters
charwidth = bytes per character: 1, 2, or 4
```

For empty views:

```text
dataptr   = null or arbitrary
charlen   = 0
charwidth = 1
```

`SDqStrView` can represent source data from:

```dq
"abc"       // string literal
s           // dynamic string
cs          // cstring[N]
pc          // ^cchar, after scanning length
s[1:4]      // string slice expression
'a'         // one-character source
```

Runtime helpers can receive one source argument instead of separate pointer/length/width arguments:

```dq
function DqStrAppend(
  smgr : ref ^ODynStrMgr,
  src  : refin SDqStrView
);

function DqStrInsert(
  smgr  : ref ^ODynStrMgr,
  index : int,
  src   : refin SDqStrView
);

function DqStrAssign(
  smgr : ref ^ODynStrMgr,
  src  : refin SDqStrView
);
```

Lifetime rules:

- `SDqStrView` does not own the referenced character storage.
- It does not increment string refcounts by itself.
- It must not be stored in heap objects.
- It must not be returned as a persistent value.
- It is valid only for the duration guaranteed by the compiler-generated call site.

This keeps public string slicing safe while still allowing efficient helper calls.

---

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

The user-visible `string` variable is internally a nullable reference to `ODynStrMgr`:

```dq
var s : string;  // internally: manager reference = null
```

A null manager represents the canonical empty string:

```dq
s.length   == 0
s.capacity == 0
s == ""
```

Operations that need storage allocate a manager automatically.

String helper functions must generally receive the manager reference by reference because they may need to rebind the string variable after allocation, detach, widening, releasing, or reallocation.

```dq
function DqStrSetChar(
  smgr  : ref ^ODynStrMgr,
  index : int,
  ch    : char
);

function DqStrAppend(
  smgr : ref ^ODynStrMgr,
  src  : refin SDqStrView
);

function DqStrInsert(
  smgr  : ref ^ODynStrMgr,
  index : int,
  src   : refin SDqStrView
);

function DqStrDelete(
  smgr  : ref ^ODynStrMgr,
  index : int,
  count : int = 1
);

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
3. `string` is a refcounted, copy-on-write dynamic character string.
4. Empty dynamic strings use a null manager and allocate no storage.
5. `string.length` and `string.capacity` are measured in characters.
6. Dynamic string storage uses one fixed width per manager: 1, 2, or 4 bytes per character.
7. String assignment shares the manager.
8. Any string modification first ensures unique writable storage.
9. Modifying one string variable never modifies another string variable assigned from it.
10. String indexing is strict and invalid indexes cause runtime bounds errors.
11. `s[i] = ch` is valid and may detach, widen, or reallocate the manager.
12. String slicing returns a new `string`, not a view.
13. String slice bounds are clamped like array slice bounds.
14. `Append()`, `Prepend()`, `Insert()`, `Delete()`, `SetLength()`, `Truncate()`, `Reserve()`, `Compact()`, `Clear()`, `Clone()`, `Pop()`, and `PopFirst()` are valid on dynamic strings.
15. `Insert()` and `Delete()` clamp their index arguments and do not produce index-bounds runtime errors.
16. `Pop()` and `PopFirst()` are runtime errors on empty strings.
17. `cstring[N]` stores at most `N` logical `cchar` characters and uses `N + 1` bytes of storage.
18. `cstring[N]` always maintains a hidden zero terminator.
19. Assignment to `cstring[N]` silently truncates by length and always zero-terminates.
20. `cstring[N]` indexing uses logical string rules; the hidden terminator is not indexable.
21. `cstring[N]` slicing returns a new `string`.
22. `^cchar` is a non-owning pointer to zero-terminated 8-bit C-compatible storage.
23. `SDqStrView` is an internal non-owning source view used by compiler/runtime helpers, not a public string-slice type.
