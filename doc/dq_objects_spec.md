# DQ Object Allocation and Lifetime Specification

Status: draft
Scope: object references, storage allocation forms, constructor/destructor calls, assignment rules, embedded object storage, inheritance, virtual functions, and field initialization.

---

## 1. Core Model

Object variables are references. An object type variable does not contain the object data directly.

Object storage can be allocated in different places:

```text
heap      allocated by new
stack     allocated by a local <- declaration
bss       allocated by a global <- declaration
embedded  allocated as part of a containing object
```

The storage location does not change the object access model. Methods and fields are accessed through an object reference in all cases.

---

## 2. Reference Variables

A normal object declaration creates a rebindable reference variable:

```dq
var obj : OTest = null;
var obj : OTest = new OTest(1, "name");
var obj = new OTest(1, "name");  // type is taken after the new
```

The variable stores a reference and may be assigned another object reference later:

```dq
obj = other;
obj = null;
```

Normal reference variables do not own automatic storage. If they point to heap storage, that storage must be released with `delete`.

---

## 3. Fixed Storage Declarations

The `<-` declaration allocates object storage and binds a fixed reference to it:

```dq
var local <- OTest(1, "stack");
var global <- OTest(2, "bss");
```

For object fields, `<-` embeds the object storage inside the containing object:

```dq
object OParent:
  child <- OChild(1);
endobj
```

A fixed storage reference cannot be rebound:

```dq
local = other;   // error
global = null;   // error
child = other;   // error
```

The compiler may lower fixed storage references without storing a separate reference slot. Uses of the variable can be lowered directly to the address of the owned storage.

---

## 4. Field Initialization

Object fields are default-initialized during object construction.

Scalar fields and reference fields may have inline initializers:

```dq
object OSerialConfig:
public
  bitrate    : int = 115200;
  timeout_us : int = 2000;
  name       : string = "uart0";
endobj
```

Inline field initializers are not ordinary assignment statements written at the declaration location. They belong to the object's field initialization phase.

If an object has inline field initializers and no explicit constructor, the compiler generates an implicit parameterless `*Create` constructor:

```dq
object OSerialConfig:
public
  bitrate    : int = 115200;
  timeout_us : int = 2000;
endobj
```

is equivalent to:

```dq
object OSerialConfig:
public
  bitrate    : int;
  timeout_us : int;

  function *Create():
    bitrate = 115200;
    timeout_us = 2000;
  endfunc
endobj
```

For an explicit constructor, field initializers are inserted after the required base constructor call, if any, and before the user-written constructor body:

```dq
object OSerialConfig:
public
  bitrate    : int = 115200;
  timeout_us : int = 2000;

  function *Create(abitrate : int):
    bitrate = abitrate;
  endfunc
endobj
```

is lowered logically as:

```dq
function *Create(abitrate : int):
  bitrate = 115200;
  timeout_us = 2000;
  bitrate = abitrate;
endfunc
```

Field initialization is performed in declaration order. Scalar field initialization and embedded object construction are part of the same field initialization phase:

```dq
object ODevice:
  id     : int = 1;
  child1 <- OChild(10);
  flags  : int = 0;
  child2 <- OChild();
endobj
```

An embedded object field declared without constructor arguments does not imply a constructor call:

```dq
child3 <- OChild;  // storage only, not constructed here
```

Such a field must still be constructed exactly once by the containing object's constructor.

---

## 5. Constructors

`*Create` is the reserved special function name for object constructors.

```dq
object OTest:
  function *Create(a : int):
  endfunc
endobj
```

Constructor calls are part of object initialization:

```dq
var a <- OTest(1);            // stack storage, calls Create(1)
var b : OTest = new OTest(2); // heap storage, calls Create(2)
```

An embedded object may be constructed at field declaration:

```dq
child1 <- OChild(1, 2);
child2 <- OChild();
```

Or its storage may be declared without an immediate constructor call:

```dq
child3 <- OChild;
```

In that case, the containing object's constructor must construct it exactly once before returning:

```dq
function *Create():
  child3.Create();
endfunc
```

Each embedded object must be constructed exactly once. Double construction and missing construction are compile-time errors when detectable.

Constructors are not virtual and are not inherited as callable overloads of the derived object.

---

## 6. Destructors

`*Destroy` is the reserved special function name for object destructors.

```dq
object OTest:
  function *Destroy():
  endfunc
endobj
```

For fixed storage objects, destruction is automatic except for module-level storage:

```text
stack storage    destroyed at scope exit
bss storage      constructed during module initialization, not automatically destroyed
embedded storage destroyed with the containing object
```

Module-level fixed-storage objects may acquire resources during construction. They are intentionally not destroyed by the runtime automatically; code that needs shutdown behavior must expose and call explicit cleanup routines.

For heap storage, destruction happens through `delete`:

```dq
delete obj;
```

`delete` calls `Destroy`, releases the heap storage, and sets the reference variable to `null`. `delete null` is a no-op.

For polymorphic objects, `delete` must destroy the most-derived runtime object, even if the reference type is a base object.

---

## 7. Assignment Rules

Assignment rebinding is allowed only for normal reference variables:

```dq
var r : OTest = null;
var s <- OTest(1);

r = s;       // allowed, r now references s
s = r;       // error, s is a fixed storage reference
```

Assigning a reference to stack or embedded storage into a longer-lived variable can create a dangling reference. The compiler should reject obvious lifetime escapes where practical, such as assigning a local fixed-storage object to a global reference or returning it from a function.

---

## 8. Object Inheritance

An object may inherit from one base object using Pascal-style syntax:

```dq
object OFileStream(OStream):
endobj
```

Only single inheritance is supported. Multiple base objects are not allowed:

```dq
object OBad(OBase1, OBase2):  // error
endobj
```

The derived object contains the base object as its first subobject. A reference to a derived object may be implicitly converted to a reference to its base object:

```dq
var fs : OFileStream = new OFileStream("test.bin");
var s  : OStream = fs;  // allowed implicit upcast
```

Because object variables are references, derived-to-base assignment does not copy the object and does not slice it.

The same rule applies to fixed-storage objects, subject to the normal lifetime escape checks:

```dq
var fs <- OFileStream("test.bin");
var s : OStream = fs;  // allowed while fs is alive
```

---

## 9. Member Visibility

Object members may use these visibility sections:

```dq
private
protected
public
```

`private` members are accessible only inside the declaring object.

`protected` members are accessible inside the declaring object and inside derived objects.

`public` members are accessible from any code that can access the object type.

---

## 10. Virtual Functions

A member function marked `[[virtual]]` creates a virtual dispatch slot:

```dq
object OStream:
public
  function Reset() [[virtual]]:
  endfunc
endobj
```

A derived object overrides a virtual function by declaring a matching function marked `[[override]]`:

```dq
object OFileStream(OStream):
public
  function Reset() [[override]]:
  endfunc
endobj
```

The `[[override]]` attribute is required for overriding. It is a compile-time error if `[[override]]` is used and no matching inherited virtual function exists.

Override matching is strict in the first language version. The function name, parameter types, and return type must match exactly.

A virtual function may be marked `[[abstract]]`:

```dq
function Read(buf : ^u8, len : int) -> int [[virtual, abstract]];
```

An object with one or more unimplemented abstract functions is abstract and cannot be directly constructed.

A virtual function may be marked `[[final]]` to prevent further overrides:

```dq
function Reset() [[override, final]]:
endfunc
```

Constructors are never virtual. Destructors participate in runtime destruction for polymorphic objects so that deleting a base reference destroys the most-derived object.

Virtual calls inside constructors and destructors should be rejected, or at minimum diagnosed, because the object is not in a stable most-derived state during construction and destruction.

---

## 11. `inherited` Calls

The `inherited` keyword calls the nearest base implementation directly. It is a static base call, not a virtual dispatch.

Explicit form:

```dq
inherited Reset();
inherited Write(buf, len);
```

Inside an overridden non-lifecycle function, the shorthand form is allowed:

```dq
function Reset() [[override]]:
  inherited;
endfunc
```

The shorthand means: call the inherited implementation of the current function with the current function arguments.

Example:

```dq
function Write(buf : ^u8, len : int) -> int [[override]]:
  return inherited;
endfunc
```

is equivalent to:

```dq
function Write(buf : ^u8, len : int) -> int [[override]]:
  return inherited Write(buf, len);
endfunc
```

For a function with a non-void return type, the result of `inherited` must be used. Ignoring the return value is a compile-time error unless the function result type explicitly allows discarding.

The shorthand `inherited;` is not allowed for constructors or destructors. Lifecycle calls must be explicit.

---

## 12. Inherited Constructor and Destructor Calls

If an object inherits from a base object, every derived constructor must explicitly call one inherited base constructor:

```dq
function *Create(...):
  inherited Create(...);
  ...
endfunc
```

The inherited constructor call must be the first effective runtime statement of the constructor body.

Valid:

```dq
function *Create(aname : string):
  inherited Create();
  localvar = 0;
endfunc
```

Invalid:

```dq
function *Create(aname : string):
  localvar = 0;
  inherited Create();  // error, must be first
endfunc
```

Invalid:

```dq
function *Create(aname : string):
  inherited Create();
  inherited Create();  // error, base constructed twice
endfunc
```

Invalid:

```dq
function *Create(aname : string):
  localvar = 0;        // error, missing inherited Create(...)
endfunc
```

If the compiler generates an implicit constructor for a derived object because of inline field initializers, it also generates the required `inherited Create()` call. This is valid only when the base object has a matching parameterless constructor.

If an object inherits from a base object, every derived destructor must explicitly call the inherited base destructor:

```dq
function *Destroy():
  ...
  inherited Destroy();
endfunc
```

The inherited destructor call must be the last effective runtime statement of the destructor body.

Valid:

```dq
function *Destroy():
  CloseFile();
  inherited Destroy();
endfunc
```

Invalid:

```dq
function *Destroy():
  inherited Destroy();
  CloseFile();        // error, inherited Destroy() must be last
endfunc
```

Invalid:

```dq
function *Destroy():
  CloseFile();        // error, missing inherited Destroy()
endfunc
```

Invalid:

```dq
function *Destroy():
  CloseFile();
  inherited Destroy();
  inherited Destroy(); // error, base destroyed twice
endfunc
```

The inherited destructor call is a lifecycle continuation point. It does not behave exactly like an ordinary method call. Before base destruction continues, the compiler destroys the embedded fields of the current derived object in reverse construction order.

Therefore this destructor:

```dq
function *Destroy():
  CloseFile();
  inherited Destroy();
endfunc
```

lowers logically as:

```text
call CloseFile()
destroy embedded fields of this object part
call inherited Destroy()
```

Base destruction then follows the same rule recursively.

---

## 13. Construction and Destruction Order

Construction of a derived object follows this logical order:

```text
1. allocate complete object storage
2. initialize hidden virtual table pointer, if required
3. execute inherited Create(...)
4. initialize this object's fields in declaration order
5. execute the user-written constructor body
```

For a base object with no inherited object, construction begins with its own field initialization phase.

Destruction of a derived object follows this logical order:

```text
1. execute the user-written destructor body up to inherited Destroy()
2. destroy this object's embedded fields in reverse construction order
3. execute inherited Destroy()
4. repeat recursively for base objects
```

For heap storage, memory is released only after the complete destructor chain has finished.

---

## 14. Object Layout

A derived object uses base-prefix layout:

```text
ODerived storage:
  OBase subobject
  ODerived fields
```

This allows an upcast from `ODerived` to `OBase` without changing the address in the single-inheritance case.

A polymorphic object contains a hidden virtual table pointer. The virtual table contains the information needed for virtual function dispatch and most-derived destruction.

The storage location does not affect layout. Heap, stack, bss, and embedded objects of the same type use the same object layout.

---

## 15. Example

```dq
object OData:
  data : [3]int;
endobj

object OCoords:
  function *Create(x : int, y : int) [[overload]]:
  endfunc

  function *Create() [[overload]]:
    Create(0, 0);
  endfunc
endobj

object OStream:
protected
  is_open : bool = false;

public
  function *Create():
  endfunc

  function Reset() [[virtual]]:
  endfunc

  function *Destroy():
  endfunc
endobj

object OFileStream(OStream):
private
  data    : OData = null;       // normal reference field
  bitrate : int = 115200;       // scalar field initializer
  coords1 <- OCoords(0, 0);     // embedded storage, constructed here
  coords2 <- OCoords;           // embedded storage, constructed by OFileStream.Create
  coords3 <- OCoords();         // embedded storage, default constructed here

public
  function *Create(aname : string):
    inherited Create();
    data = new OData();
    coords2.Create();
    is_open = true;
  endfunc

  function Reset() [[override]]:
    bitrate = 115200;
    inherited;
  endfunc

  function *Destroy():
    delete data;
    is_open = false;
    inherited Destroy();
  endfunc
endobj
```
