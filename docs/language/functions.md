# Functions

Functions are declared with `func`.

For exact signature, parameter-mode, overload, and result rules, see the
[Functions reference](../reference/functions.md).

```dq
func Add(a : int, b : int) -> int:
    return a + b
endfunc
```

When there is no return value, omit `->`.

```dq
func Log(value : int):
    // ...
endfunc
```

## Declarations and Definitions

A function signature may be declared without a body.

```dq
func Add(a : int, b : int) -> int
```

The definition provides the body.

```dq
func Add(a : int, b : int) -> int:
    return a + b
endfunc
```

External functions are declarations connected to symbols outside DQ.

```dq
func printf(fmt : ^char, ...) -> int  [[external]]
```

## Return Values

Functions with a return type have a built-in `result` variable.

```dq
func Add(a : int, b : int) -> int:
    result = a + b
endfunc
```

`return value` is also supported.

```dq
func Add(a : int, b : int) -> int:
    return a + b
endfunc
```

## Parameters

The default parameter mode is by value.

```dq
func IncCopy(value : int) -> int:
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
func SetValue(value : ref int):
    value = 5
endfunc

func SumInput(value : refin int) -> int:
    return value + 1
endfunc

func MakeValue(value : refout int):
    value = 33
endfunc

func MaybeSet(value : refnull int):
    if %value == nil:
        return
    endif
    value = 44
endfunc
```

`refnull` accepts `nil` as an argument. Inside the function, `%param == nil`
tests whether a real storage location was supplied.

## Default Arguments

Parameters may have default values.

```dq
func Repeat(text : strview, count : int = 1):
    // ...
endfunc
```

Default arguments are used when the caller omits the trailing argument.

## Variable Arguments

External functions can declare C-style variable arguments with `...`.

```dq
func printf(fmt : ^char, ...) -> int  [[external]]
```

DQ library functions commonly use `[]anyvalue` for type-checked variable value
lists.

```dq
Print("{} {}", ["value", 123])
```

## Function Overloading

Overloaded functions and methods must be marked with `[[overload]]`.

```dq
func Print(value : int) [[overload]]:
    // ...
endfunc

func Print(value : strview) [[overload]]:
    // ...
endfunc
```

The overload set is selected by argument types.

## Function References

Function reference types are declared with `type`.

```dq
type FUnary = func(value : int) -> int

func Inc(value : int) -> int:
    return value + 1
endfunc

var cb : FUnary = Inc
var value : int = cb(10)
```

Function references may be `nil` and can be compared with `nil`.

A typed or opaque pointer can be explicitly cast to a plain function reference
for low-level interoperability, such as constructing an interrupt vector table.
The pointer does not carry signature information, so the declared function
reference signature determines how an indirect call is generated.

```dq
type FHandler = func()
var raw : pointer = GetHandlerAddress()
var handler : FHandler = FHandler(raw)
```

This conversion is never implicit and is not available for object method
reference types declared with `of object`.

```dq
if cb <> nil:
    cb(1)
endif
```

Object method references add `of object`.

```dq
type FObjText = func(msg : cstring) of object
```

## Special Functions

Names beginning with `*` are special-purpose functions.

`*Main` is the program entry point.

```dq
func *Main() -> int:
    return 0
endfunc
```

`*ModuleInit` is a module initialization function, called before the `*Main()`

```dq
var g_handler : OHandler = nil

func *ModuleInit():
    g_handler = new OHandler()
endfunc
```

On bare-metal targets, the application owns its entry point and must call the
compiler-generated `dq_module_init()` after initialized data and zeroed storage
are ready. This runs the initialization functions of all used DQ modules,
including generated constructors for global fixed objects.

`*Create` is an object constructor and `*Destroy` is an object destructor.

```dq
object OThing:
    func *Create(value : int):
        // ...
    endfunc

    func *Destroy():
        // ...
    endfunc
endobject
```
