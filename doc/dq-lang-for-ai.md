# DQ Writing Hints for AI Agents

This is a compact practical guide for writing current DQ code. Prefer examples in
`stdpkg/`, `examples/`, and `autotest/tests/` over older draft specs when they
disagree.

## Core Style

- Use Pascal-style blocks for normal code:
  `function X(): ... endfunc`, `if ...: ... endif`, `object ...: ... endobj`.
- Indent with 2 spaces. Semicolons are optional; prefer omitting them in new code.
- Prefer readable names and explicit types at declarations.
- Prefer `use print` and `PrintLn()` for examples/tools instead of raw `printf`.
- Use `str` for owned text, `strview` for read-only text parameters, `cstring(N)`
  only for fixed C-compatible buffers.
- Use `ref`, `refin`, `refout`, or `refnull` when a parameter should alias or mutate
  caller storage. Plain parameters are by value.
- Use explicit casts: `int(x)`, `float64(x)`, `^byte(ptr)`, `OType(rawptr)`.
- Boolean expressions must be `bool`; integers are not truthy.

## File Skeletons

Executable:

```dq
use print

function *Main() -> int:
  PrintLn('Hello {}', ['DQ'])
  return 0
endfunc
```

Module with public interface and private implementation:

```dq
use print

function Add(a : int, b : int) -> int

implementation

function Add(a : int, b : int) -> int:
  return a + b
endfunc
```

Each `.dq` file is one module. There is no required `module` declaration.

## Imports and Namespaces

```dq
use print, strutils
use file
use ./local_mod as lm only(Foo, Bar)
use ./facade reexport
use ./private_impl --        // import module but do not merge public names
```

- `use pkg/mod` or `use ./relative_mod`.
- `as name` creates an alias.
- `only(...)` and `exclude(...)` control which names merge into local scope.
- `--` keeps imported names out of local scope; access them via alias.
- `reexport` exposes imported interface names from this module.
- Qualified module/global access uses `@`: `@lm.Foo()`, `@.printf(...)`.

## Declarations

```dq
const MaxCount : int = 100
type TIndex = int32

var n : int = 0
var text : str = 'abc'
var inferred = 42
```

Function forms:

```dq
function Sum(a : int, b : int = 0) -> int:
  result = a + b       // common return variable
endfunc

function ClampPositive(v : int) -> int:
  if v < 0:
    return 0           // early return is fine
  endif
  return v
endfunc

function Log(msg : strview):
  PrintLn(msg)
endfunc
```

Overloads require `[[overload]]` on all overloads in the set:

```dq
function ToText(v : int) -> str [[overload]]:
  return Format('{}', [v])
endfunc
```

External C functions:

```dq
function printf(fmt : ^cchar, ...) -> int  [[external]]
function libc_fopen(name : ^cchar, mode : ^cchar) -> pointer  [[external('fopen')]]
```

## Types

Common scalar types:

```dq
bool
byte, uint8, int8, uint16, int16, uint32, int32, uint64, int64
uint, int
float32, float64, float
char, cchar
pointer
```

Pointers:

```dq
var x : int = 7
var p : ^int = &x
p^ = 9
if p <> null:
  PrintLn('{}', [p^])
endif
```

Pointer indexing does not dereference. `p[2]^` means `(p + 2)^`.

References:

```dq
function Inc(v : ref int):
  v += 1
endfunc

function MaybeSet(v : refnull int):
  if &v != null:
    v = 1
  endif
endfunc
```

Local aliases:

```dq
ref item = arr[i]
item = 10
```

## Text

```dq
var s : str = 'hello'
var v : strview = s
var cs : cstring(31) = 'fixed buffer'
```

- One-character literal is `char`; empty or multi-character literal is `str`.
- Single and double quotes both work. Prefer the one that avoids escaping.
- `str` is owned, mutable, refcounted copy-on-write.
- `strview` is a non-owning read-only view; prefer for input parameters.
- `cstring(N)` is inline C-compatible zero-terminated storage; unsized `cstring`
  is a bounded mutable alias parameter.
- Use `s.length`, `s.capacity`, `s.Append(x)`, `s.Prepend(x)`, `s.Insert(i, x)`,
  `s.Delete(i, count)`, `s.Clear()`, `s.SetLength(n, fill)`, `s.Clone()`.
- Do not compare `str` to `null`; use `s == ''` or `s.length == 0`.

Formatting:

```dq
use print, strutils

PrintLn('name={} count={}', [name, count])
var out : str = Format('{:X}', [value])
```

## Arrays and Slices

```dq
var fixed : [3]int = [1, 2, 3]     // owning fixed-size array
var inferred : [?]int = [4, 5, 6]  // fixed, length from initializer
var dyn : [*]int = [1, 2, 3]       // owning dynamic array
var view : []int = dyn[1:]         // non-owning mutable slice
```

- Use `.length` and `.capacity`.
- Dynamic arrays support `Append`, `Prepend`, `Insert`, `Delete`, `Pop`,
  `PopFirst`, `SetLength`, `Reserve`, `Compact`, `Clear`, `Clone`.
- Slices do not own storage and cannot resize it.
- Passing array literals to `[]T` parameters is OK: `Sum([1, 2, 3])`.
- Do not compare dynamic arrays to `null`; use `.length == 0` or `arr == []`.

## Control Flow

```dq
if a < b:
  ...
elif a == b:
  ...
else:
  ...
endif

while i < n:
  i += 1
endwhile

for i : int = 0 to 10:
  ...
endfor

for i : int = 10 downto 0 step 2:
  ...
endfor

for i : int = 0 count n:
  ...
endfor

for i : int = 0 while i < n step 2:
  ...
endfor
```

Use `break` and `continue` normally. `iif(cond, a, b)` is the short conditional
expression and evaluates only the selected branch.

## Operators

- Logical bool operators are lowercase: `and`, `or`, `not`.
- Integer/bitwise operators are uppercase: `AND`, `OR`, `XOR`, `NOT`, `SHL`,
  `SHR`, `IDIV`, `IMOD`.
- `/` returns floating-point division. Use `IDIV` for integer division.
- Comparisons: `==`, `<>`, `<`, `<=`, `>`, `>=`.
- Address/deref: `&x`, `p^`.

## Structs and Objects

Structs are plain data:

```dq
struct SPoint:
  x : int
  y : int
endstruct
```

Objects have methods, constructors, destructors, inheritance, access sections,
and either reference or embedded storage:

```dq
object OCounter:
private
  value : int = 0

public
  function *Create(start : int):
    value = start
  endfunc

  function Add(delta : int):
    value += delta
  endfunc

  function Get() -> int:
    return value
  endfunc
endobj
```

Storage and lifetime:

```dq
var local <- OCounter(10)          // embedded/in-place object; local destructor runs
var refobj : OCounter = null       // object reference
refobj = new OCounter(5)
delete refobj = null
```

- `function *Create(...)` is a constructor; `function *Destroy()` is a destructor.
- Inside methods, fields and methods are directly visible; no `self` is needed.
- Inherited object: `object OChild(OBase):`.
- Mark virtual methods with `[[virtual]]`, overrides with `[[override]]`, abstract
  methods with `[[virtual, abstract]]`.
- Call overridden base behavior with `inherited`.
- Method implementations may be outside the object: `function OFile.Open(...):`.

Properties:

```dq
property name : str read m_name
property position : int64 read CurPos write Seek
```

## Enums

```dq
enum NColor = (red, green, blue)
enum NState : uint16 = (
  idle = 0,
  running = 10,
  stopped = 20,
)

var c : NColor = NColor.green
PrintLn('{}', [int(c.ord)])
```

Enum values can be context-qualified (`green`) or type-qualified
(`NColor.green`). Use `.ord` or `Ord(x)` for the ordinal.

## Exceptions

```dq
object EMyError(Exception):
endobj

try:
  raise EMyError('bad value: {}', [v])
except EMyError as e:
  e.PrintMessage()
finally:
  Cleanup()
endtry
```

Use `raise Exception('message')` or a derived exception object. `finally` is
optional.

## Preprocessor and Options

```dq
#ifdef DEBUG
  ...
#elifdef OTHER
  ...
#else
  ...
#endif

#if false
  ...
#endif

#opt module_root_depth = 1
```

## Preferred AI Checklist

- Start small: choose imports, define data types, write `*Main` or public module
  declarations, then implementation.
- Prefer `strview` for text input parameters and `[]T` for array input parameters
  unless the function needs ownership.
- Use `ref` only when mutation/aliasing is intentional.
- Use `return expr` for simple return paths; use `result = ...` when accumulating.
- Initialize variables before reading them.
- Match object allocation to ownership: `<-` for embedded lifetime, `new/delete`
  for heap lifetime.
- Keep C interop isolated behind small DQ wrappers when possible.
- After writing DQ, compile and run a single file with `dq-run file.dq`
