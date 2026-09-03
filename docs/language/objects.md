# Objects

DQ objects are reference types with fields, methods, constructors,
destructors, properties, inheritance, and virtual dispatch.

For allocation, lifetime, dispatch, casting, and property rules, see
[Objects and Properties](../reference/objects-and-properties.md).

## Declaration

```dq
object OCounter:
private
    value : int = 0

public
    func Inc():
        value += 1
    endfunc

    func Value() -> int:
        return value
    endfunc
endobject
```

Members can be grouped with visibility labels such as `public` and `private`.
The current codebase commonly uses these labels inside object declarations.

## Allocation

Object variables are references. A reference may be `nil`.

```dq
var obj : OCounter = nil
```

Use `new` for heap allocation.

```dq
obj = new OCounter()
```

Use `delete` to destroy a heap object and release its storage.

```dq
delete obj
```

Embedded allocation uses `<-`.

```dq
var local_counter <- OCounter()
var global_counter <- OCounter()
```

Embedded objects live in the containing storage location, such as a local stack
frame, global data, or another object.

## Constructors and Destructors

`*Create` is the constructor. `*Destroy` is the destructor.

```dq
object OData:
    value : int

    func *Create(initial : int):
        value = initial
    endfunc

    func *Destroy():
        // cleanup
    endfunc
endobject
```

Constructors can be overloaded with `[[overload]]`.

```dq
object OText:
    func *Create() [[overload]]:
    endfunc

    func *Create(text : strview) [[overload]]:
    endfunc
endobject
```

## Methods

Methods are declared inside an object. They can also be implemented outside the
object by qualifying the function name.

```dq
object OThing:
    func Run()
endobject

func OThing.Run():
    // ...
endfunc
```

Inside a method, object fields and methods are available without a `self.`
prefix.

```dq
func OCounter.Inc():
    value += 1
endfunc
```

From the object methods variables and functions outside of the object cannot be accessed without full namespace qualification.
Method body `use` statements can blend-in namespaces so they can be used without qualification.
```dq
func OCounter.Print()
  @print.PrintLn('counter={}', value)  // @print. can be used to access PrintLn()
  @.PrintLn('counter={}', value)       // "@." for access everything that available in the current module
  use print                            // now the symbols in @print are available without qualification
  PrintLn('counter={}', value)
endfunc
```

## Inheritance

Objects support single inheritance.

```dq
object OBase:
    func Count() [[virtual]]:
    endfunc
endobject

object OChild(OBase):
    func Count() [[override]]:
        inherited
    endfunc
endobject
```

`inherited` calls the inherited implementation of the current method.

## Virtual Methods

Virtual methods are marked with `[[virtual]]`. Overrides are marked with
`[[override]]`.

```dq
object OBase:
    func Write(text : strview) [[virtual]]:
    endfunc
endobject

object OChild(OBase):
    func Write(text : strview) [[override]]:
    endfunc
endobject
```

The compiler also recognizes `[[abstract]]` and `[[final]]` in object method
contexts.

## Dynamic Casts

Object references can be dynamically cast with `TryCast` or with `tryfrom`.

```dq
var base : OBase = child
var child2 : OChild = TryCast(OChild, base)

var child3 : OChild tryfrom base
```

If the value is not compatible with the requested object type, the result is
`nil`.

## Properties

Properties expose field-like syntax backed by fields or methods.

```dq
object OBox:
private
    stored : int = 0

    func GetStored() -> int:
        return stored
    endfunc

    func SetStored(value : int):
        stored = value
    endfunc

public
    property value : int read GetStored write SetStored
endobject
```

A property must have at least a `read` or a `write` accessor.

Field-backed properties are also supported.

```dq
property direct : int read stored write stored
```

Properties are used like fields.

```dq
box.value = 10
box.value += 1
PrintInt(box.value)
```

Read-only and write-only properties are possible by omitting the opposite
accessor.

```dq
property readonly : int read GetStored
property writeonly : int write SetStored
```

Indexed properties place an index signature before the property result type.

```dq
property item : [index : int]int read GetItem write SetItem
property cell : [row : int, col : int]int read GetCell write SetCell
```

Indexed accessors must be methods. A getter receives the index arguments and
returns the property type. A setter receives the index arguments followed by the
new value.

An indexed property can be marked `default`. The default indexed property is
used when the object itself is indexed.

```dq
object OJson:
    ...
    property child : [int]OJson default read Child
endobject

func Test():
    var firstchild : OJson
    var rootobj : OJson = new OJson()
    rootobj.LoadFromFile('test.json')
    firstchild = rootobj[0]
endfunc
```

Properties themselves are not addressable, but values returned by a property
keep their normal semantics. For example, a property returning an object
reference can be used to access that object's members.

## Object Function References

Method reference types use `of object`.

```dq
type FObjText = func(msg : cstring) of object
```

An object method reference contains both the method and the object instance.
Virtual functions resolved at assignment.
