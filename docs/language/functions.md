# Functions

Functions are declared with `function`.

```dq
function Add(a : int, b : int) -> int:
    return a + b
endfunc
```

When there is no return value, omit `->`.

```dq
function Log(value : int):
    // ...
endfunc
```

## Declarations and Definitions

A function signature may be declared without a body.

```dq
function Add(a : int, b : int) -> int
```

The definition provides the body.

```dq
function Add(a : int, b : int) -> int:
    return a + b
endfunc
```

External functions are declarations connected to symbols outside DQ.

```dq
function printf(fmt : ^char, ...) -> int  [[external]]
```

## Return Values

Functions with a return type have a built-in `result` variable.

```dq
function Add(a : int, b : int) -> int:
    result = a + b
endfunc
```

`return value` is also supported.

```dq
function Add(a : int, b : int) -> int:
    return a + b
endfunc
```

## Parameters

The default parameter mode is by value.

```dq
function IncCopy(value : int) -> int:
    return value + 1
endfunc
```

Reference parameter modes are:

| Mode | Meaning |
| --- | --- |
| `ref` | Read/write reference |
| `refin` | Read-only input reference |
| `refout` | Output reference |
| `refnull` | Nullable reference |

```dq
function SetValue(value : ref int):
    value = 5
endfunc

function SumInput(value : refin int) -> int:
    return value + 1
endfunc

function MakeValue(value : refout int):
    value = 33
endfunc

function MaybeSet(value : refnull int):
    if &value == null:
        return
    endif
    value = 44
endfunc
```

`refnull` accepts `null` as an argument. Inside the function, `&param == null`
tests whether a real storage location was supplied.

## Default Arguments

Parameters may have default values.

```dq
function Repeat(text : strview, count : int = 1):
    // ...
endfunc
```

Default arguments are used when the caller omits the trailing argument.

## Variable Arguments

External functions can declare C-style variable arguments with `...`.

```dq
function printf(fmt : ^char, ...) -> int  [[external]]
```

DQ library functions commonly use `[]anyvalue` for type-checked variable value
lists.

```dq
Print("{} {}", ["value", 123])
```

## Function Overloading

Overloaded functions and methods must be marked with `[[overload]]`.

```dq
function Print(value : int) [[overload]]:
    // ...
endfunc

function Print(value : strview) [[overload]]:
    // ...
endfunc
```

The overload set is selected by argument types.

## Function References

Function reference types are declared with `type`.

```dq
type FUnary = function(value : int) -> int

function Inc(value : int) -> int:
    return value + 1
endfunc

var cb : FUnary = Inc
var value : int = cb(10)
```

Function references may be `null` and can be compared with `null`.

```dq
if cb != null:
    cb(1)
endif
```

Object method references add `of object`.

```dq
type FObjText = function(msg : cstring) of object
```

## Special Functions

Names beginning with `*` are special-purpose functions.

`*Main` is the program entry point.

```dq
function *Main() -> int:
    return 0
endfunc
```

`*ModuleInit` is a module initialization function, called before the `*Main()`

```dq
var g_handler : OHandler = null

function *ModuleInit():
    g_handler = new OHandler()
endfunc
```

`*Create` is an object constructor and `*Destroy` is an object destructor.

```dq
object OThing:
    function *Create(value : int):
        // ...
    endfunc

    function *Destroy():
        // ...
    endfunc
endobj
```
