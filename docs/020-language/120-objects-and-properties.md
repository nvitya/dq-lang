# Objects and Properties

An **object** keeps data and the functions that work on it together. Its data
is stored in **fields** (variables that belong to the object), and its functions
are called **methods**. Use objects for things that have their own state and
behavior, such as a counter, a document, a sensor, or a network connection.

Object type names conventionally start with a capital `O`, as in `OCounter`.

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

This prints `2`. The arrow `<-` creates an **embedded object** named `counter` (an
object allocated directly in the local function rather than separately on the
heap), and `OCounter()` creates and initializes it. When `Main` finishes, the
object is cleaned up automatically.

Objects differ from [structures](100-structures-and-unions.md) in several
important ways:

| Feature | Structure | Object |
| --- | --- | --- |
| Assignment | Copies the fields | Copies the reference; both variables point to the same object |
| Allowed fields | Plain data only | Any type, including `str`, `[*]T`, and other objects |
| Setup and cleanup | None | Constructor (`*Create`), destructor (`*Destroy`), automatic field cleanup |
| Methods | Plain methods | Supports virtual methods and properties |
| Type testing | No | Can test the actual type at runtime with `is` |

## Fields and Methods

An object declaration defines two things between `object Name:` and
`endobject`:
- **Fields:** variables that belong to the object and hold its data.
- **Methods:** functions that belong to the object and can read or update its
  fields.

A field can have an initial value (such as `lives : int = 3`), which is given
to every newly created object. Fields without an initializer start with their
zero or empty value: numeric fields start at zero, strings and dynamic arrays
start empty, and object references start as `nil` (referring to no object).

```dq
struct SPoint:
    x : int
    y : int
endstruct

object OPlayer:
    name     : str        // starts empty
    scores   : [*]int     // dynamic array, starts empty
    position : SPoint     // all fields zero
    lives    : int = 3
    rival    : OPlayer    // object reference, starts nil

    func AddScore(points : int):
        scores.Append(points)
    endfunc

    func Total() -> int:
        result = 0
        for points : int in scores:
            result += points
        endfor
    endfunc
endobject
```

Use a dot (`.`) to access a field or call a method:

```dq
var player <- OPlayer()
player.name = "Lin"
player.AddScore(10)
player.AddScore(25)
var total : int = player.Total() // 35
```

Inside a method, the object's own fields and methods can be used directly by
name—no special prefix is needed.

If an object needs to refer to itself—for example, to pass itself as an argument
to another function—it uses the special name `self`, as in `engine.Create(self)`.

DQ checks code from top to bottom. This means a method must be declared before
other methods in the same object can call it. If method `A` calls method `B`,
write `B` above `A`, or [declare the method first and define its body
later](#declaring-methods-separately).

To call a function outside the object from inside a method, use a module
prefix: `@.PrintLn(...)` reaches `PrintLn` imported into the current module,
while `@print.PrintLn(...)` names the `print` module explicitly. Compiler
built-ins such as `SizeOf()` need no prefix. The full lookup rules and
method-local `use` are covered under
[Organizing Object Code](#organizing-object-code).

## Object References

An object variable holds a **reference** (a handle pointing to the object in
memory), rather than containing a separate copy of the fields. Assigning an
object variable to another variable makes both refer to the same object:

```dq
var counter <- OCounter()
var alias : OCounter = counter
alias.Increment()
// counter.value is now 1: counter and alias refer to the exact same object
```

This is different from structures: copying a structure copies all of its data,
so changing one copy leaves the other unchanged.

The same applies when passing objects to functions. An ordinary object
parameter receives a reference to the caller's object, so the function can read
and modify the object directly without needing a `ref` parameter:

```dq
func GiveBonus(player : OPlayer):
    player.AddScore(5) // modifies the caller's player object
endfunc
```

An ordinary reference such as `alias` or `player` only **borrows** the object:
it can use the object, but it does not own it and is not responsible for
destroying it.

### The `nil` Reference

An object reference can be `nil`, meaning it currently points to no object at
all. Like any other local variable in DQ, an object variable must be initialized
before it is read (for example, initialized to `nil`).

Always check whether a reference might be `nil` before using it:

```dq
var selected : OPlayer = nil
// ...
if selected <> nil:
    selected.AddScore(1)
endif
```

Accessing a field or calling a method through a `nil` reference is invalid and
stops the program with an error (a segmentation fault).

## Creating and Destroying Objects

Every object needs memory to hold its fields. DQ provides three ways to store
and manage an object, depending on how long it needs to exist:

| Form | How it works | When to use it |
| --- | --- | --- |
| `counter <- OCounter()` | **Embedded object:** stored directly in a function's local storage, the global data segment, or a containing object. | Use for local objects, global objects, or fields without a separate heap allocation. Local objects and fields are destroyed automatically; globals are not. |
| `var counter = new autofree OCounter()` | **Auto-free object:** lives in dynamic memory (the heap). The variable owns it and releases it automatically. | The object needs to outlive a single function, but you still want automatic cleanup without calling `delete`. |
| `var counter = new OCounter()` | **Manually managed heap object:** lives on the heap. You must destroy it yourself with `delete`. | You want full manual control over when the object is destroyed. |

### Embedded Objects (`<-`)

An **embedded object** declared with `<-` has its storage directly in the
context where it is declared:

- Inside a function, it lives in local storage (the stack) and is destroyed
  automatically when its scope ends, including on an early `return` or when an
  exception leaves the scope.
- At module scope, it lives in the global data segment. Global embedded objects
  are not destroyed automatically when the program finishes.
- As a field of another object, it lives inside the containing object and is
  destroyed with it.

An embedded object cannot be redirected to point to another object, and it
cannot be passed to `delete`.

For an embedded field, the constructor call can be delayed: write
`field <- OType ..`, without `()`, and call `field.Create(...)` explicitly from
the containing object's constructor. See
[Embedding Objects in Fields](#embedding-objects-in-fields) for an example.

### Constructors and Destructors

A **constructor** is a special method named `*Create`. It runs automatically
when a new object is being created, allowing you to initialize fields and accept
arguments from the caller.

A **destructor** is a special method named `*Destroy`. It runs automatically
when the object is being destroyed, allowing you to perform any final cleanup
(such as closing a file or releasing an external system resource).

The asterisk `*` marks them as [special functions](060-functions.md#special-functions). When calling
them explicitly or referring to them, write their names without `*`.

```dq
use print

object OConnection:
    host : str

    func *Create(ahost : strslice):
        host = ahost
        @.PrintLn("Connected to {}", [host])
    endfunc

    func Send(message : strslice):
        @.PrintLn("{} <- {}", [host, message])
    endfunc

    func *Destroy():
        @.PrintLn("Disconnected from {}", [host])
    endfunc
endobject

func *Main():
    var connection <- OConnection("example.com")
    connection.Send("hello")
endfunc
```

Output:

```text
Connected to example.com
example.com <- hello
Disconnected from example.com
```

In `OConnection("example.com")`, the argument `"example.com"` is passed to
`*Create`. If an object does not declare a constructor, you create it with empty
parentheses: `OType()`.

### Heap Objects and Autofree Variables (`new` and `autofree`)

The **heap** is general memory where values can live independently of any
single function call. Using `new` allocates memory for the object on the heap.

Adding `autofree` gives the variable an **autofree type**. An **autofree
variable** destroys its heap object automatically when the variable goes out of
scope, when it is set to `nil`, or when a new object is assigned to it. The
variable is responsible for releasing the object, so it is also called its
**owner**:

```dq
var counter = new autofree OCounter()
counter.Increment()
counter = new OCounter() // destroys the first counter before taking the new one
counter = nil            // destroys the second counter
```

Without `autofree`, you must manually pair `new` with `delete`:

```dq
var counter = new OCounter()
counter.Increment()
delete counter = nil // destroys the object, releases its storage, and clears the variable
```

Forgetting to call `delete` leaves the object stranded in memory (a **memory
leak**). On the other hand, calling `delete` while other variables still point
to the object leaves those variables pointing to invalid memory, which is
dangerous (see [dangling pointers](110-pointers.md#dangling-pointers)).

Global objects are not destroyed automatically when the program finishes. See
[Memory Management and Autofree](130-memory-management-and-ownership.md) for
details on transferring ownership, autofree array elements, and `ref autofree`
parameters.

### Automatic Field Initialization and Cleanup

Fields are initialized before the constructor body runs, so the constructor can
safely use them immediately.

When an object is destroyed, the destructor body runs first. After that, DQ
automatically cleans up the object's fields in reverse declaration order:
- Managed strings (`str`) and dynamic arrays release their allocated storage.
- Embedded objects are destroyed.
- Autofree fields destroy the heap objects they hold.

Because of this automatic cleanup, you only need to write a `*Destroy` method
when you have custom cleanup work to do, such as closing external handles or
logging a message. Ordinary object references in fields are just borrows, so the
objects they point to are not destroyed.

If an error or exception occurs inside a constructor, the object creation fails.
Any fields and base parts that were already set up are cleaned up in reverse
order, so half-constructed objects are never left behind. See
[Exceptions and Error Handling](140-exceptions-and-error-handling.md).

### Overloaded Constructors

You can provide more than one constructor by adding the `[[overload]]`
attribute to each. This gives callers different ways to create the object. One
constructor can also call another constructor to reuse setup code:

```dq
object OColor:
    red   : byte
    green : byte
    blue  : byte

    func *Create(ared : byte, agreen : byte, ablue : byte) [[overload]]:
        red = ared
        green = agreen
        blue = ablue
    endfunc

    func *Create(gray : byte) [[overload]]:
        Create(gray, gray, gray) // calls the three-argument constructor
    endfunc
endobject

var orange <- OColor(255, 165, 0)
var gray <- OColor(128) // red, green, and blue are all 128
```

### Embedding Objects in Fields

Just as a structure can contain another structure, an object can contain
another object as one of its fields. Declare the embedded object with `<-`
to construct it as part of the containing object and destroy it with that object:

```dq
object OEngine:
    power : int

    func *Create(apower : int):
        power = apower
    endfunc
endobject

object OCar:
    engine <- OEngine(120) // created with every OCar, destroyed with it
endobject
```

Sometimes the embedded object needs arguments that are only available when the
outer object's constructor runs. In that case, omit the parentheses and write
`..` after the type name (`engine <- OEngine ..`). This reserves the storage
without calling the constructor. Call `Create` explicitly inside the outer
constructor:

```dq
object OCar:
    engine <- OEngine .. // reserves storage; constructed in OCar.Create

    func *Create(apower : int):
        engine.Create(apower)
    endfunc
endobject
```

An embedded object must be constructed before code attempts to use it. Its
storage belongs entirely to the containing object, so never pass an embedded
object to `delete`. The [composed objects example](#lifetime-of-composed-objects)
shows how to use this pattern to give an embedded object a reference back to its
container.

## Member Visibility

By default, all members of an object (its fields, methods, and properties) are
**public**, meaning any code that uses the object can access them.

You can restrict access using visibility labels:

| Label | Who can access the members that follow |
| --- | --- |
| `public` | Any code. This is the default. |
| `protected` | Only methods of this object and of objects derived from it. |
| `private` | Only methods of this specific object. |

Visibility labels act like section headers inside the object: all members
declared below a label share that visibility until another label appears.

Use visibility labels to protect an object's internal data from unauthorized or
accidental changes. In the following example, callers can inspect the balance,
but can only change it through `Deposit`, which ensures invalid amounts are
rejected:

```dq
object OAccount:
private
    balance : int = 0
public
    func GetBalance() -> int:
        return balance
    endfunc

    func Deposit(amount : int) -> bool:
        if amount <= 0:
            return false
        endif
        balance += amount
        return true
    endfunc
endobject
```

Code using an `OAccount` can call `account.GetBalance()`, but cannot touch
`balance` directly. The next section introduces properties, which let callers
read a value with field syntax while keeping the underlying field private.

## Properties

A **property** looks and acts like a regular field to anyone using the object,
but behind the scenes it can use methods to read or write the value.

Properties give you the best of both worlds: clean, natural syntax (such as
`volume.value = 80`) combined with the ability to validate input, compute
values on demand, or expose read-only data while keeping internal fields private.

### Field-Backed Properties

The simplest property reads from or writes to a private field directly.
Omitting `write` makes it a **read-only property**:

```dq
object OTimer:
private
    elapsed : int = 0
public
    property seconds : int read elapsed // read-only for outside callers

    func Tick():
        elapsed += 1
    endfunc
endobject
```

Outside callers can read `timer.seconds`, but attempting to assign to it
(`timer.seconds = 0`) is caught at compile time with an error.

### Computed Properties

A **getter** method computes the value each time the property is read, without
needing to store it in a separate field:

```dq
object OTemperature:
public
    celsius : float = 20
private
    func GetFahrenheit() -> float:
        return celsius * 9 / 5 + 32
    endfunc
public
    property fahrenheit : float read GetFahrenheit
endobject

var temp <- OTemperature()
temp.celsius = 25
var f : float = temp.fahrenheit // computes 77 dynamically
```

No `fahrenheit` field is stored in memory; it always calculates the temperature
from the current `celsius` value.

### Checking New Values with Setters

A **setter** method receives the new value and checks or adjusts it before
storing it:

```dq
use print

object OVolume:
private
    level : int = 50

    func SetLevel(value : int):
        if value < 0:
            level = 0
        elif value > 100:
            level = 100
        else:
            level = value
        endif
    endfunc
public
    property value : int read level write SetLevel
endobject

func *Main():
    var volume <- OVolume()
    volume.value = 150 // SetLevel limits it to 100
    volume.value -= 10 // reads 100, then calls SetLevel(90)
    PrintLn("{}", [volume.value])
endfunc
```

This prints `90`.

### Property Accessor Rules

The methods used to read and write a property are called **accessors**:
- `read` specifies the field or getter method that supplies the value. A getter
  takes no parameters and returns the property type.
- `write` specifies the field or setter method that updates the value. A setter
  takes the new value as its only parameter and returns nothing.

Accessor methods must belong to the same object and must be declared before the
property that uses them.

A property needs at least one accessor:
- Omit `write` for a read-only property.
- Omit `read` for a write-only property.
- Compound assignment operators such as `+=` or `-=` read the current value,
  modify it, and write it back, so they require both `read` and `write`.

### Indexed and Default Properties

An **indexed property** lets callers use square bracket syntax (`[]`), similar
to an array. Its getter receives the index; its setter receives the index
followed by the new value:

```dq
object OScores:
private
    values : [3]int

    func GetScore(index : int) -> int:
        return values[index]
    endfunc

    func SetScore(index : int, value : int):
        values[index] = value
    endfunc
public
    property score : [index : int]int default read GetScore write SetScore
endobject

var scores <- OScores()
scores.score[0] = 10
scores[1] = 20 // the default keyword lets you omit .score
```

Indices are accessor parameters and are not restricted to integers. For example,
`OStrStrMap` in `collections/strstrmap` declares its `item` property with a
string key:

```dq
property item : [akey : str]str default read GetItem write SetItem
```

Callers can use a string with either the property name or the default syntax:

```dq
use collections/strstrmap, print

func *Main():
    var settings <- OStrStrMap()
    settings.item["theme"] = "dark"
    PrintLn("{}", [settings["theme"]]) // prints dark
endfunc
```

An indexed property can accept more than one index, which is useful for grids
and tables:

```dq
property cell : [row : int, col : int]int read GetCell write SetCell
```

Callers can use this with multiple indices: `grid.cell[1, 2] = 5`.

Putting `default` after the indexed property type allows callers to use brackets
directly on the object itself (such as `scores[1]` instead of
`scores.score[1]`). When an object and its base types declare several default
properties, the one declared latest is selected.

### Property Restrictions

A property is not an actual variable stored at a single memory address: reading
it calls a getter or reads a field, and writing it calls a setter. Because of
this:

- A property cannot be passed to a `ref`, `refin`, or `refout` parameter. To
  pass a property's value to a function that modifies it through `ref`, copy the
  value into a temporary local variable, pass that variable, and assign the
  updated value back to the property:

```dq
var level : int = volume.value
Adjust(level) // func Adjust(value : ref int)
volume.value = level
```

- You cannot take the memory address of a property using `%`.

Values returned by a property behave normally; for example, a property that
returns an object reference can be followed immediately by member access, as in
`window.current_button.Click()`.

## Inheritance

**Inheritance** allows a new object type to build on an existing object type:
- The existing type is called the **base object** (or parent).
- The new type is called the **derived object** (or child).

The derived object automatically inherits all fields and methods from the base
object and can add its own new fields and methods. In DQ, an object can inherit
from one base object, written in parentheses after its name:

```dq
object OAnimal:
    name : str

    func *Create(aname : strslice):
        name = aname
    endfunc
endobject

object ODog(OAnimal):
    tricks : int = 0

    func Learn():
        tricks += 1
    endfunc
endobject

var dog <- ODog("Rex") // uses the inherited OAnimal constructor
dog.Learn()
var animal : OAnimal = dog // a derived object can be assigned to a base reference
```

A derived object can be used wherever its base type is expected, as shown in the
assignment `var animal : OAnimal = dog`. When viewed through an `animal`
reference, only the members declared in `OAnimal` are accessible.

### Constructors and Inheritance

When an object inherits from a base type, the base part must be initialized:

- If the derived object does not declare a constructor of its own, it
  automatically inherits the base constructor (as `ODog("Rex")` does above).
- If the derived object declares its own constructor, it must call the base
  constructor using `inherited Create(...)`:

```dq
object OGuideDog(ODog):
    owner : str

    func *Create(aname : strslice, aowner : strslice):
        inherited Create(aname) // initialize the base OAnimal/ODog part first
        owner = aowner
    endfunc
endobject
```

If the base constructor takes no parameters, calling `inherited Create()` is
optional; DQ calls it automatically if you omit it. If the base constructor
requires parameters, the derived constructor must call it explicitly. The
compiler prevents calling the base constructor more than once.

Object setup runs from base to derived, and cleanup runs in reverse order: when
an object is destroyed, the derived destructor runs first, and then the base
destructor runs automatically. A destructor may call `inherited Destroy()`
explicitly if the base cleanup must happen at a specific point during destruction.

### Virtual Methods

Normally, when you call a method through an object reference, the compiler
decides which method to run based on the **declared type** of the variable. For
example, if a variable is declared as `var animal : OAnimal`, calling
`animal.Sound()` would normally call `OAnimal.Sound()`, even if the variable
currently holds an `ODog`.

A **virtual method** changes this: it tells DQ to inspect the **actual object**
in memory at runtime and run that specific object's version of the method
instead.

- In the base object, mark the method with `[[virtual]]` to allow derived
  objects to customize it.
- In the derived object, mark the replacement method with `[[override]]`.

```dq
use print

object OAnimal:
    name : str

    func *Create(aname : strslice):
        name = aname
    endfunc

    func Sound() -> str [[virtual]]:
        return "..."
    endfunc

    func Speak():
        @.PrintLn("{} says {}", [name, Sound()])
    endfunc
endobject

object ODog(OAnimal):
    func Sound() -> str [[override]]:
        return "Woof"
    endfunc
endobject

object OCat(OAnimal):
    func Sound() -> str [[override]]:
        return "Meow"
    endfunc
endobject

func *Main():
    var dog <- ODog("Rex")
    var cat <- OCat("Tom")
    var animals : [2]OAnimal = [dog, cat]
    for animal :? in animals:
        animal.Speak()
    endfor
endfunc
```

Output:

```text
Rex says Woof
Tom says Meow
```

`Speak` is declared only in `OAnimal`, yet its call to `Sound()` runs the
derived implementation for each animal. Without `[[virtual]]`, both lines would
print `...`.

The override must have the exact same parameters and return type as the virtual
base method. The compiler reports an error if `[[override]]` is used on a method
that has no matching virtual method in the base type, or if a derived method has
the same name as a virtual method but forgets `[[override]]`.

Accessor methods can be virtual. A derived object can override an inherited
accessor method to customize how the property behaves.

A destructor cannot call virtual methods, because derived parts of the object
may already have been torn down.

### Calling the Base Implementation

Inside an overriding method, `inherited` calls the base type's version of that
method. This lets you extend the base behavior rather than replacing it
entirely:

```dq
object OLoudDog(ODog):
    func Sound() -> str [[override]]:
        var base : str = inherited Sound()
        return base + "!!!" // "Woof!!!"
    endfunc
endobject
```

Use the explicit call syntax, `inherited Sound()`, when you need the return
value or want to pass different arguments. A bare `inherited` statement calls
the base method with the current arguments and ignores any return value.

### Abstract and Final Methods

An **abstract method** has a header but no body. It acts as a requirement: the
base object declares that every derived object *must* provide its own version
of this method. Declare it with `[[virtual, abstract]]` and two dots `..`
instead of a body:

```dq
object OShape:
    func Area() -> float [[virtual, abstract]] ..
endobject
```

Because an abstract object type has incomplete methods, **you cannot create an
object directly from an abstract type** (writing `new OShape()` or
`<- OShape()` is an error). You can only create objects from derived types that
have implemented all abstract methods.

A **final method**, marked with `[[virtual, final]]`, prevents any further
derived types from overriding it.

## Runtime Types

### Runtime Type Tests and Casts

When you hold a reference of a base type (such as `var animal : OAnimal`), it
might point to an `ODog`, an `OCat`, or another derived object. DQ provides two
tools to check and convert references safely at runtime:

#### Testing Types with `is`

The `is` operator tests whether an object reference points to an object of a
particular type or one of its derived types. It returns `true` or `false`, and
safely returns `false` if the reference is `nil`:

```dq
var dog <- ODog("Rex")
var animal : OAnimal = dog
if animal is ODog:
    // true: animal refers to an ODog
endif
```

#### Converting References with `TryCast()`

To access fields or methods that exist only on the derived type, you must
convert the base reference to the derived type. This conversion is called a
**type cast**.

Because the object might not actually be of that type, use
`TryCast(TargetType, source)` for a safe, checked conversion. Local variables
can be declared in any of these three forms:

- `var specific : ODog = TryCast(ODog, animal)`
- `var specific :? = TryCast(ODog, animal)`
- `var specific = TryCast(ODog, animal)`

The last two forms infer the variable's type from the target type passed to
`TryCast`. All three forms check the actual object at runtime:

- If the object is an `ODog` (or a type derived from `ODog`), the new variable
  receives a reference to that object.
- If the object is something else (such as an `OCat`), or if `animal` is `nil`,
  the new variable receives `nil` instead of crashing.

```dq
var asdog = TryCast(ODog, animal) // the ODog, or nil
var ascat = TryCast(OCat, animal) // nil: animal is not an OCat
if asdog <> nil:
    asdog.Learn()
endif
```

Converting a reference only changes how the compiler views the object. It does
not copy the object, and it does not transfer ownership.

### Object Type Variables and Type Names

In DQ, variables can hold data values, but they can also hold *types*. A
`type of OBase` variable can store the type `OBase` or any type derived from it.
This allows a program to choose at runtime which kind of object to construct:

```dq
func MakeAnimal(cat : bool, name : strslice) -> OAnimal:
    var animaltype : type of OAnimal = ODog
    if cat:
        animaltype = OCat
    endif
    return new animaltype(name) // creates an ODog or OCat using the OAnimal constructor
endfunc
```

Calling `new` on the type variable constructs an object of whichever type was
stored in it. The selected type must accept the constructor arguments of the
base type. Unrelated types, abstract types, and types with incompatible
constructors are rejected by the compiler. The returned heap object still needs
an autofree variable or an explicit `delete`.

#### Querying Type Names

`TypeName(Type)` returns the name of a type as text. When passed an object
reference, `TypeName(reference)` returns the actual runtime type of the object
if it is non-`nil`, or the variable's declared type if it is `nil`:

```dq
var animal : OAnimal = MakeAnimal(true, "Tom")
PrintLn("{}", [TypeName(animal)]) // prints "OCat"
delete animal
```

Use `TypeName` for debugging and logging rather than as a permanent identifier
in file formats or network protocols.

## Object Method References

DQ allows you to store a function in a variable so it can be called later (a
function reference). Because a method belongs to a specific object instance,
storing a method requires an **object method reference**, written as
`func(...) of object`.

An object method reference bundles two things together:

1. The method to call.
2. The specific object instance to call it on.

When you assign a virtual method to an object method reference, DQ resolves the
method implementation for that specific object at the time of assignment. The
reference only borrows the object—it does not keep the object alive. See
[Object Method References](060-functions.md#object-method-references) for a
complete example.

## Organizing Object Code

As objects grow, method bodies can be defined separately from their declarations.
Forward declarations let object types refer to each other, and explicit module
prefixes or method-local `use` control access to outside names.

### Declaring Methods Separately

Every method has two parts:

1. **The declaration (the header):** tells the compiler the method's name, its
   parameters, and what type it returns.
2. **The definition (the body):** the actual lines of code that run when the
   method is called.

Both parts can be written together directly inside the `object ... endobject`
block. However, when an object has many methods or long code, keeping everything
inside the object can make it difficult to read. DQ lets you separate them:

- Put just the **declaration** inside the object, followed by `..` (two dots)
  to indicate that the body will be provided later.
- Write the full **definition** (the body) outside the object, prefixing the
  method name with the object type name, such as `func OButton.Click():`.

```dq
object OButton:
    caption : str

    func Click() ..
    func *Create(acaption : strslice) ..
endobject

func OButton.*Create(acaption : strslice):
    caption = acaption
endfunc

func OButton.Click():
    @.PrintLn("{} clicked", [caption])
endfunc
```

The definition outside the object must match the declaration's parameters,
return type, and attributes. Inside the separated method body, all fields and
methods of the object are available directly by name, exactly as if the method
were written directly inside the object block.

### Forward Declarations

The DQ compiler reads source files from top to bottom. If two object types
reference each other (for example, `OParent` has a field pointing to `OChild`,
and `OChild` has a field pointing to `OParent`), one of them must be introduced
before its full description is written.

A **forward declaration** introduces the name of an object type using `..` so
other declarations can refer to it:

```dq
object OParent .. // forward declaration: introduces OParent

object OChild:
    parent : OParent // allowed because OParent was introduced above
endobject

object OParent:
    child : OChild
endobject
```

The full definition of the object must be supplied before any code creates the
object, accesses its fields, or calls its methods.

### Accessing Variables and Functions Outside of the Object

Inside an object method, bare names always refer to the object's own members
(its fields, methods, and properties). Variables, constants, and functions
outside of the object scope are **not** directly available by their bare names.
This restriction applies to:

- Global variables and constants declared in the same module.
- Functions declared in the same module.
- Symbols from imported modules, including the runtime's built-in `sys` module
  (such as `MemSet`, `MemCopy`, or `MemAlloc`).

This scope separation prevents accidental naming conflicts—an object's field
(such as `count`, `status`, or `length`) can never be confused with an outside
variable or library function with the same name.

The exception to this rule is **compiler internal built-ins**, such as
`SizeOf()`, `OffsetOf()`, and `TypeName()`, as well as language types (such as
`int`, `float`, `str`, `rostr`, or `SPoint`). These are built into the language
itself and are always available everywhere without any prefix.

To access outside variables, constants, or functions from inside a method, you
normally prefix them with `@`:

- **`@module.Name` (fully qualified):** names the imported module and the
  symbol, such as `@print.PrintLn`, `@sys.MemSet`, or `@socket.AF_INET`.
- **`@..Name` (current module only):** refers to a symbol that is defined in
  the current module (until this source code position)
- **`@.Name` (full outside scope):** the short form. It reaches everything
  available in the current module at that source code position, including its
  own global variables and constants, as well as imported functions.

```dq
use print

var greeting_count : int = 0

object OGreeter:
    name : str = "Lin"

    func Greet():
        @print.PrintLn("Hello, {}!", [name]) // fully qualified module reference
        @.greeting_count += 1                // short form for a module variable
        @.PrintLn("Greetings so far: {}", [@.greeting_count])
    endfunc
endobject
```

Writing `PrintLn(...)` or `greeting_count` without a prefix inside `Greet` is a
compile error (`ObjVsInModuleNs`), and the compiler error message suggests the
prefixed form.

### Method-Local Use

While prefixing outside symbols with `@.` or `@module.` works well for
occasional calls, practical code often uses many outside definitions at once.
For example, low-level networking, file I/O, or system routines frequently
rely on dozens of constants, types, and functions from modules such as `socket`
or `sys`. Writing `@socket.AF_INET`, `@socket.libc_htons(...)`, and
`@sys.MemSet(...)` on every single line quickly becomes verbose and clutters the
code.

To solve this, DQ provides the **method-local `use`** statement. Placing a
`use` statement at the very beginning of a method body opens outside namespaces
for direct, unqualified access inside that specific method:

```dq
func ONanoDatagram.SetRemoteAddr(aaddr : rostr, aport : uint16):
    use socket, sys
    MemSet(%remote_addr, 0, SizeOf(remote_addr))
    remote_addr_len = SizeOf(remote_addr)
    remote_addr.sin_family = AF_INET
    remote_addr.sin_port = libc_htons(aport)

    if libc_inet_pton(AF_INET, aaddr, %remote_addr.sin_addr) <> 1:
        raise ENanoNet('Invalid Host IP address: {}', [aaddr])
    endif
endfunc
```

In this method:

- `SizeOf(remote_addr)` needs no prefix because `SizeOf()` is a compiler
  internal function available everywhere.
- `MemSet` comes from the `sys` module, while `AF_INET`, `libc_htons`, and
  `libc_inet_pton` come from `socket`.
- The `use socket, sys` statement makes all of them accessible directly by
  their bare names throughout the method body.

#### Forms of Method-Local `use`

Method-local `use` supports the following forms:

- **`use module1, module2`:** Opens one or more specific imported module
  namespaces for direct access (as shown with `use socket, sys` above).
- **Using an imported alias:** If a module was imported at the file level with
  an alias (such as `use libc/stdio as cstdio`), you must use that **alias
  name** in the method-local statement:
  ```dq
  func OLogger.Log(msg : strslice):
      use cstdio // refers to libc/stdio via its module alias
      printf("%s\n", msg.pchar)
  endfunc
  ```
- **`use .` (local module definitions):** Makes all declarations belonging to
  the current module itself (its global variables, constants, and functions)
  directly available without `@.` prefixes.
- **`use *` (make everything available):** Makes *all* symbols visible at the
  current module level directly available inside the method, including the
  current module's own declarations and all imported modules.

#### Scope and Rules

- Method-local `use` only affects name lookup **inside that specific method
  body**. It does not affect any other methods in the same object or file.
- It does not load new modules from disk; it only opts already imported module
  namespaces into unqualified lookup within the method.
- Object members always take precedence: if an object field has the same name
  as an outside symbol brought in by `use`, using that name inside the method
  accesses the object's field. To reach the outside symbol in that case, use an
  explicit `@module.Name` or `@.Name` prefix.

## Object Examples

### A Short Object Demo

This example combines inheritance, a read-only property, an embedded helper,
and autofree variables. `OChild` changes a protected field and extends a
virtual method, while `OBase` manages the helper's construction and cleanup.

```dq
--8<-- "examples/dq_intro_3.dq"
```

Source: [dq_intro_3.dq](https://github.com/nvitya/dq-lang/blob/main/examples/dq_intro_3.dq).

This test outputs:

```text
DQ Objects Demo
Helper for "Base-heap" created
Base-heap: value=1
Helper for "Child1-stack" created
Child1-stack: value=2 flval=1.5
"Child1-stack" destroyed
Helper for "Child1-stack" destroyed
Error test
Helper for "ErrChild-heap" created
"ErrChild-heap" destroyed
Helper for "ErrChild-heap" destroyed
ERROR: Some error happened!
Backtrace =
  #0 ErrorTest @ /workvc/dq-lang/examples/dq_intro_3.dq:75
  #1 Main @ /workvc/dq-lang/examples/dq_intro_3.dq:93
reassigning autofree type
"Base-heap" destroyed
Helper for "Base-heap" destroyed
Helper for "Child2-heap" created
Child2-heap: value=2 flval=3.14
Autofree type as argument
"Child2-heap" destroyed
Helper for "Child2-heap" destroyed
Helper for "Child3-heap" created
Child3-heap: value=2 flval=2.333333333333334
nil-ing autofree type
"Child3-heap" destroyed
Helper for "Child3-heap" destroyed
Test finished.
```

`OChild` can change the protected `intval` field, while outside callers can
only read it through the inherited `value` property. Calling `Print()` runs
`OBase.Print`, whose virtual call to `PrintExtra()` selects the child's
override. The bare `inherited` in that override prints the base value before
the child adds `flval`.

`helper.Create(self)` gives the embedded helper a borrowed reference to its
container. The helper's method bodies appear after the full `OBase` definition
so they can access `base.name`. The name uses `embstr(64)`, keeping its text
storage inside the object. Because `helper` is declared after `name`, it is
destroyed first and can still read the name during cleanup.

`ErrorTest` shows that the local autofree variable destroys its object before the exception
handler runs. The handler then calls `e.PrintBacktrace()`; its frames depend on
the build and source location. The rest of `Main` demonstrates autofree variable replacement,
replacement through a `ref autofree OBase` parameter, and early destruction by
assigning `nil`.

### A Temperature Log with Properties

`OTemperatureLog` keeps a limited number of recent temperature readings. Its
internal storage is private; callers interact with it exclusively through
properties and methods:

| Property | Kind | Purpose |
| --- | --- | --- |
| `count` | read-only, computed | Current number of stored readings |
| `average` | read-only, computed | Average of all stored readings |
| `capacity` | field-backed read, validated setter | Maximum number of readings; lowering it discards the oldest extra readings |
| `reading` | indexed, default | Reads or updates a stored reading by index |

```dq
use print

object OTemperatureLog:
private
    readings : [*]float
    limit    : int = 5

    func GetCount() -> int:
        return readings.length
    endfunc

    func GetAverage() -> float:
        result = 0
        for value : float in readings:
            result += value
        endfor
        if readings.length > 0:
            result /= readings.length
        endif
    endfunc

    func SetCapacity(value : int):
        limit = iif(value < 1, 1, value)
        while readings.length > limit:
            readings.Delete(0) // drop the oldest readings
        endwhile
    endfunc

    func GetReading(index : int) -> float:
        return readings[index]
    endfunc

    func SetReading(index : int, value : float):
        readings[index] = value
    endfunc

public
    property count    : int   read GetCount
    property average  : float read GetAverage
    property capacity : int   read limit write SetCapacity
    property reading  : [index : int]float default read GetReading write SetReading

    func Add(value : float):
        if readings.length == limit:
            readings.Delete(0)
        endif
        readings.Append(value)
    endfunc

    func Print():
        @.Print("count {}:", [count])
        for i : int = 0 count count:
            @.Print(" {}", [reading[i]])
        endfor
        @.PrintLn(", average {:.2f}", [average])
    endfunc
endobject

func *Main():
    var log <- OTemperatureLog()
    log.Add(21.5)
    log.Add(22)
    log.Add(35)
    log.Add(22.5)
    log.Print()

    log[2] = 23 // corrects a wrong reading through the default property
    log.reading[0] += 0.5
    log.Print()

    log.capacity = 2 // keeps only the two newest readings
    log.Print()

    log.capacity = -5 // SetCapacity limits it to a minimum of 1
    log.Add(24)
    log.Print()
    PrintLn("Capacity: {}", [log.capacity])
endfunc
```

Output:

```text
count 4: 21.5 22 35 22.5, average 25.25
count 4: 22 22 23 22.5, average 22.38
count 2: 23 22.5, average 22.75
count 1: 24, average 24.00
Capacity: 1
```

`count` and `average` do not have dedicated storage fields; their getters
compute fresh values directly from the `readings` array each time they are read.
`capacity` reads the private `limit` field directly, but writes go through
`SetCapacity`, which enforces a minimum value of 1 and trims excess elements.

`log[2] = 23` invokes `SetReading(2, 23)` through the default property.
`log.reading[0] += 0.5` exercises both accessors: it calls `GetReading(0)`, adds
`0.5`, and then passes the result to `SetReading(0, 22)`. Inside `Print`,
properties are accessed naturally by name, such as `count` and `reading[i]`.

Because `readings` is private, external code cannot modify the array directly or
bypass the validation logic in `Add` and `SetCapacity`.

### Shapes in an Array with Autofree Elements

This example builds a shape hierarchy. `OShape` declares an abstract `Area`
method, and each derived shape provides its own calculation. A dynamic array of
`autofree` references owns the shapes, so they are destroyed automatically when
they are removed from the array or when the array leaves scope.

```dq
use print

object OShape:
    name : str

    func *Create(aname : strslice):
        name = aname
    endfunc

    func Area() -> float [[virtual, abstract]] ..

    func Describe() [[virtual]]:
        @.PrintLn("{}: area {:.2f}", [name, Area()])
    endfunc

    func *Destroy():
        @.PrintLn("Released {}", [name])
    endfunc
endobject

object ORectangle(OShape):
    width  : float
    height : float

    func *Create(awidth : float, aheight : float):
        inherited Create("rectangle")
        width = awidth
        height = aheight
    endfunc

    func Area() -> float [[override]]:
        return width * height
    endfunc
endobject

object OSquare(ORectangle):
    func *Create(aside : float):
        inherited Create(aside, aside)
        name = "square"
    endfunc
endobject

object OCircle(OShape):
    radius : float

    func *Create(aradius : float):
        inherited Create("circle")
        radius = aradius
    endfunc

    func Area() -> float [[override]]:
        return 3.14159 * radius * radius
    endfunc

    func Describe() [[override]]:
        inherited // prints the name and the area
        @.PrintLn("  radius: {}", [radius])
    endfunc
endobject

func TotalArea(shapes : []autofree OShape) -> float:
    result = 0
    for shape : OShape in shapes:
        result += shape.Area() // calls the Area method of the actual shape type
    endfor
endfunc

func *Main():
    var shapes : [*]autofree OShape // the array owns the shapes
    shapes.Append(new ORectangle(4, 2.5))
    shapes.Append(new OSquare(3))
    shapes.Append(new OCircle(1.5))

    for shape : OShape in shapes:
        shape.Describe()
    endfor
    PrintLn("Total area: {:.2f}", [TotalArea(shapes)])

    var rectangles : int = 0
    for shape : OShape in shapes:
        if shape is ORectangle: // true for OSquare too
            ++rectangles
        endif

        var circle = TryCast(OCircle, shape) // nil for other shapes
        if circle <> nil:
            circle.radius *= 2
            PrintLn("Enlarged {}: area {:.2f}", [TypeName(circle), circle.Area()])
        endif
    endfor
    PrintLn("Rectangles: {}", [rectangles])

    shapes.Delete(0) // destroys the rectangle
    PrintLn("Remaining: {}", [shapes.length])
endfunc
```

Output:

```text
rectangle: area 10.00
square: area 9.00
circle: area 7.07
  radius: 1.5
Total area: 26.07
Enlarged OCircle: area 28.27
Rectangles: 2
Released rectangle
Remaining: 2
Released square
Released circle
```

`OSquare` inherits from `ORectangle` and reuses its constructor and `Area`
calculation, simply passing the same value for both width and height. `OCircle`
overrides `Describe` and uses a bare `inherited` call to run the base shape
description before printing its radius.

Inside the loop, `shape` has the base type `OShape`, yet each `shape.Area()`
call selects the correct derived formula at runtime. `shape is ORectangle` is
also true for the square, because `OSquare` derives from `ORectangle`.
`TryCast` safely converts the reference to `OCircle`, returning `nil` for any
other shape.

The loop variables and the `TotalArea` parameter only borrow the shapes. The
`shapes` dynamic array owns them: `shapes.Delete(0)` immediately destroys the
rectangle, and the remaining shapes are destroyed automatically when `shapes`
goes out of scope at the end of `Main`.

### Lifetime of Composed Objects

This example traces how objects are created and destroyed when objects are
composed together. An `OMachine` contains an embedded `OEngine`, and the engine
keeps an ordinary reference back to its machine. `OPump` inherits from
`OMachine` and extends its `Run` method.

```dq
use print

object OMachine ..

object OEngine:
    machine : OMachine // back-reference; the engine does not own the machine

    func *Create(amachine : OMachine) ..
    func Start() ..
    func *Destroy() ..
endobject

object OMachine:
    name   : str
    engine <- OEngine .. // constructed later, in OMachine.Create

    func *Create(aname : strslice):
        name = aname
        @.PrintLn("{}: created", [name])
        engine.Create(self)
    endfunc

    func Run() [[virtual]]:
        engine.Start()
    endfunc

    func *Destroy():
        @.PrintLn("{}: destroyed", [name])
    endfunc
endobject

func OEngine.*Create(amachine : OMachine):
    machine = amachine
    @.PrintLn("{}: engine created", [machine.name])
endfunc

func OEngine.Start():
    @.PrintLn("{}: engine running", [machine.name])
endfunc

func OEngine.*Destroy():
    @.PrintLn("{}: engine destroyed", [machine.name])
endfunc

object OPump(OMachine):
    rate : int

    func *Create(aname : strslice, arate : int):
        inherited Create(aname)
        rate = arate
    endfunc

    func Run() [[override]]:
        inherited
        @.PrintLn("{}: pumping {} l/min", [name, rate])
    endfunc
endobject

func ReplaceMachine(machine : ref autofree OMachine):
    machine = new OPump("pump-2", 40) // releases the caller's previous machine
endfunc

func *Main():
    PrintLn("-- embedded object in a block")
    if true:
        var local <- OPump("pump-1", 25)
        local.Run()
    endif

    PrintLn("-- autofree variable replacement")
    var current = new autofree OMachine("mixer")
    current.Run()
    ReplaceMachine(current)
    current.Run()

    PrintLn("-- exception")
    try:
        var temporary = new autofree OMachine("tester")
        raise Exception("power failure")
    except Exception as e:
        PrintLn("Error: {}", [e.message])
    endtry

    PrintLn("-- end of Main")
endfunc
```

Output:

```text
-- embedded object in a block
pump-1: created
pump-1: engine created
pump-1: engine running
pump-1: pumping 25 l/min
pump-1: destroyed
pump-1: engine destroyed
-- autofree variable replacement
mixer: created
mixer: engine created
mixer: engine running
mixer: destroyed
mixer: engine destroyed
pump-2: created
pump-2: engine created
pump-2: engine running
pump-2: pumping 40 l/min
-- exception
tester: created
tester: engine created
tester: destroyed
tester: engine destroyed
Error: power failure
-- end of Main
pump-2: destroyed
pump-2: engine destroyed
```

`OEngine` needs to store a reference to its machine, but `OMachine` is declared
further down in the file. The forward declaration `object OMachine ..` introduces
the type name so `OEngine` can declare the `machine : OMachine` field. The
engine's method bodies are defined after `OMachine` is fully declared because
they access `machine.name`.

The engine cannot be constructed in its field declaration line because the
machine's `self` reference is only available inside `OMachine.Create`. The `..`
in `engine <- OEngine ..` reserves the field's storage, delaying construction
until `engine.Create(self)` is called.

Each machine's destructor runs before its fields are cleaned up, which is why
`destroyed` appears before `engine destroyed`. Furthermore, fields are cleaned
up in reverse declaration order, so `engine` is destroyed before `name`, ensuring
`machine.name` remains valid while the engine's destructor runs.

The three sections illustrate the different ways objects are cleaned up:
- `local` is destroyed when the `if` block ends.
- `ReplaceMachine` replaces the caller's autofree variable via a `ref autofree` parameter,
  which automatically destroys `"mixer"` before assigning `"pump-2"`.
- `temporary` is destroyed when the exception escapes the `try` block, before
  the `except` handler begins executing.
- Finally, `current` releases `"pump-2"` when `Main` returns.
