# Objects and Properties

An **object** keeps data and the functions that work on it together. Its data
is stored in **fields**, and its functions are called **methods**. Use objects
for things that have their own state, such as a counter, a document, or a
network connection.

Here is a counter with a method that increases its value:

```dq
use print

object OCounter:
    value : int = 0

    func Increment():
        value += 1
    endfunc
endobject

func *Main():
    var counter <- OCounter()
    counter.Increment()
    counter.Increment()
    PrintLn("{}", [counter.value])
endfunc
```

This prints `2`. `counter` is a local fixed object, created by `<-` and cleaned
up automatically when `Main` ends.

## Object Types and References

An object variable refers to an object rather than containing a copy of its
fields. Assigning it to another variable makes both refer to the same object:

```dq
var counter <- OCounter()
var alias : OCounter = counter
alias.Increment() // counter.value is now 1 too
```

This differs from [structures](100-structures-and-unions.md), whose assignment
copies the fields. An ordinary alias does not keep an object alive or take
responsibility for destroying it.

An object reference may be `nil`, meaning it refers to no object. Check it
before accessing fields or calling methods if it might be empty:

```dq
var selected : OCounter = nil
if selected <> nil:
    selected.Increment()
endif
```

## Storage Forms

Choose how long the object needs to exist:

| Form | Use it when |
| --- | --- |
| `var counter <- OCounter()` | The object should live in its containing scope or object. |
| `var counter = new autofree OCounter()` | You need a heap object with automatic cleanup by its owner. |
| `var counter : OCounter = new OCounter()` | You will explicitly destroy it or give it to an owner. |

For a heap object managed manually, pair `new` with `delete`:

```dq
var counter : OCounter = new OCounter()
counter.Increment()
delete counter = nil // destroy it, release its storage, and clear this variable
```

Do not pass fixed objects created with `<-` to `delete`. Other references to a
destroyed object become invalid even if they are still non-`nil`.

Local fixed objects and local `autofree` owners are cleaned up when their scope
ends, including when an exception leaves it. An embedded fixed object lives
with the object that contains it. Global objects need an explicit shutdown
plan. See [Memory Management and Ownership](130-memory-management-and-ownership.md).

## Constructors and Destructors

A **constructor**, `*Create`, prepares a new object. Its parameters let the
caller supply initial values. A **destructor**, `*Destroy`, performs any custom
cleanup when the object is destroyed:

```dq
use print

object OGreeting:
    name : str

    func *Create(aname : strslice):
        name = aname
    endfunc

    func SayHello():
        @.PrintLn("Hello, {}!", [name])
    endfunc

    func *Destroy():
        @.PrintLn("Goodbye, {}!", [name])
    endfunc
endobject

func *Main():
    var greeting <- OGreeting("Lin")
    greeting.SayHello()
endfunc
```

This prints `Hello, Lin!` followed by `Goodbye, Lin!`. Managed fields such as
`name` already clean themselves up; a destructor is useful for additional
work, such as closing an external resource. You can omit both special methods
when no custom work is needed. Constructors can be overloaded with
`[[overload]]`.

Inside a method, `self` refers to the current object, and fields and methods
can be used without a `self.` prefix. To call a function from the surrounding
module, use `@.`, as in `@.PrintLn(...)` above. See
[Method-Local Use](170-modules-and-packages.md#method-local-use) for a shorter
form when a method needs many such calls.

## Members and Visibility

Use visibility labels to control access to fields, methods, and properties:

| Label | Who can access the following members |
| --- | --- |
| `public` | Code that uses the object. |
| `protected` | The object's implementation and its descendants. |
| `private` | The declaring object's implementation. |

For example, keep a counter's stored value private and let callers change it
only through `Increment`:

```dq
object OCounter:
private
    stored : int = 0
public
    property value : int read stored

    func Increment():
        stored += 1
    endfunc
endobject
```

Callers can read `counter.value` but cannot assign to it or access `stored`.
The property is explained below.

Methods may be declared inside the object and defined later:

```dq
object OButton:
    func Click() ..
endobject

func OButton.Click():
    // handle the click
endfunc
```

The definition must match the declaration's parameters, result, and attributes.

## Properties

A **property** looks like a field to its caller, but can use a field or methods
to read and write the value. Use one to expose a read-only value or check new
values before storing them.

```dq
object OVolume:
private
    stored : int = 0

    func SetValue(value : int):
        if value < 0:
            stored = 0
        elif value > 100:
            stored = 100
        else:
            stored = value
        endif
    endfunc
public
    property value : int read stored write SetValue
endobject

func *Main():
    var volume <- OVolume()
    volume.value = 150 // SetValue limits the stored value to 100
    volume.value -= 10 // reads 100, then calls SetValue with 90
endfunc
```

`read` names the field or method used to get the value; `write` names the field
or method used to set it. A getter takes no parameters and returns the property
type. A setter takes the new value as its parameter. Accessor methods belong to
the object, and their types must match the property.

Omit `write` for a read-only property, or omit `read` for a write-only property.
At least one accessor is required. Operations such as `+=` need both accessors.

## Indexed and Default Properties

An indexed property lets callers use brackets to select a value. Here, a small
scoreboard keeps its array private:

```dq
object OScores:
private
    values : [3]int

    func GetItem(index : int) -> int:
        return values[index]
    endfunc

    func SetItem(index : int, value : int):
        values[index] = value
    endfunc
public
    property item : [index : int]int default read GetItem write SetItem
endobject

func *Main():
    var scores <- OScores()
    scores.item[0] = 10
    scores[1] = 20 // the default property lets you omit .item
endfunc
```

The getter receives the indices. The setter receives those indices followed by
the new value. More than one index is allowed, for example
`property cell : [row : int, col : int]int read GetCell write SetCell`.
Put `default` after the property type and before its accessors.

## Inheritance and Virtual Dispatch

An object can extend one base object, reusing its fields and methods. A
**virtual method** lets the derived object supply its own behavior even when
called through a base reference:

```dq
use print

object OMessage:
    func Text() -> str [[virtual]]:
        return "Hello"
    endfunc
endobject

object OWelcome(OMessage):
    func Text() -> str [[override]]:
        return "Welcome back"
    endfunc
endobject

func *Main():
    var welcome <- OWelcome()
    var message : OMessage = welcome
    PrintLn("{}", [message.Text()])
endfunc
```

This prints `Welcome back`. Mark the base method `[[virtual]]` and its
replacement `[[override]]`. Without virtual dispatch, the declared reference
type determines the method selection.

Inside an override, `inherited` calls the base implementation of the current
method. Use an explicit call such as `inherited Text()` when you need its
result. `[[abstract]]` declares a method without an implementation;
`[[final]]` prevents further overriding where supported.

## Runtime Type Tests and Casts

Use `is` to check whether a reference points to a particular object type or
one of its descendants. Use `tryfrom` or `TryCast` to obtain a reference of
that type:

```dq
var welcome <- OWelcome()
var message : OMessage = welcome
if message is OWelcome:
    var specific : OWelcome tryfrom message
    // specific refers to the same welcome object
endif

var other : OWelcome = TryCast(OWelcome, message)
```

The checked conversions return `nil` if the object is incompatible. They do
not create another object or transfer ownership. Keep the original object
alive while using the converted reference.

## Field Initialization

Fields initialize in declaration order before the constructor body. Explicit
initializers run for each new object; other fields receive their default
values. Base initialization happens before derived fields. Construction
proceeds from base to derived; destruction proceeds from derived to base.

A derived constructor can pass arguments to its base with
`inherited Create(...)`. A destructor can explicitly call
`inherited Destroy()`. The compiler prevents calling the same base lifetime
operation twice. If construction raises an exception, already initialized
owned fields and base state are cleaned up in reverse order; no partially
constructed object is returned.

### Delayed Construction of Embedded Objects

Normally, `<- OType(...)` constructs the embedded object immediately. When its
constructor needs information from the containing object's constructor, use
`..` to reserve storage and call `Create` later:

```dq
object OChild:
    value : int
    func *Create(initial : int):
        value = initial
    endfunc
endobject

object OOwner:
    child <- OChild ..

    func *Create(initial : int):
        child.Create(initial)
    endfunc
endobject
```

Construct the child before using it. Its storage belongs to the containing
object, so do not release it with `delete`.

## Dispatch and Addressability

A property is not itself addressable, so it cannot be passed directly to a
`ref`, `refin`, or `refout` parameter. Copy its value to a variable, let the
function update that variable, then assign it back if needed. Values returned
by a property keep their normal behavior; for example, a returned object
reference can be used for member access.

Accessor methods can be virtual. Default-property lookup starts with the
receiver's declared type, choosing the last default property declared there;
if none is declared, it looks in the base type. A base reference therefore
selects the base type's default property, although that property's accessor
may still call a derived override.

## Forward Declarations

When two object types refer to each other, declare one name before giving its
full definition:

```dq
object OParent ..

object OChild:
    parent : OParent
endobject

object OParent:
    child : OChild
endobject
```

Supply the complete definition before code needs the type's fields, methods,
or storage layout.

## Object Type Variables and Names

A `type of OBase` variable stores a concrete object type derived from `OBase`.
This lets a function choose which type to create at runtime:

```dq
object OBase:
endobject

object OChild(OBase):
endobject

func MakeObject() -> OBase:
    var selected : type of OBase = OChild
    return new selected()
endfunc
```

The selected type must support the base constructor contract. Unrelated types,
abstract types, and incompatible constructor signatures are rejected. The
returned reference still needs an owner or an explicit `delete`.

`TypeName(Type)` returns a type's name. `TypeName(object_value)` reports the
actual object type for a non-`nil` reference; for `nil`, it reports the declared
type. Use this for diagnostics, rather than as a permanent file-format or
external-interface identifier.

## Object Function References

A `func(...) of object` reference stores a method and the object to call it on.
A virtual implementation is selected for that object when the reference is
assigned. The reference does not keep the object alive. See
[Object Method References](060-functions.md#object-method-references) for a
complete counter example.
