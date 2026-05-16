# DQ Object Allocation and Lifetime Specification

Status: draft
Scope: object references, storage allocation forms, constructor/destructor calls, assignment rules, and embedded object storage.

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

## 4. Constructors

`Create` is the reserved constructor name for objects.

```dq
object OTest:
  function Create(a : int):
  endfunc
endobj
```

Constructor calls are part of object initialization:

```dq
var a <- OTest(1);          // stack storage, calls Create(1)
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
function Create():
  child3.Create();
endfunc
```

Each embedded object must be constructed exactly once. Double construction and missing construction are compile-time errors when detectable.

---

## 5. Destructors

`Destroy` is the reserved destructor name for objects.

```dq
object OTest:
  function Destroy():
  endfunc
endobj
```

For fixed storage objects, destruction is automatic:

```text
stack storage    destroyed at scope exit
bss storage      destroyed during program shutdown
embedded storage destroyed with the containing object
```

For heap storage, destruction happens through `delete`:

```dq
delete obj;
```

`delete` calls `Destroy`, releases the heap storage, and sets the reference variable to `null`. `delete null` is a no-op.

---

## 6. Assignment Rules

Assignment rebinding is allowed only for normal reference variables:

```dq
var r : OTest = null;
var s <- OTest(1);

r = s;       // allowed, r now references s
s = r;       // error, s is a fixed storage reference
```

Assigning a reference to stack or embedded storage into a longer-lived variable can create a dangling reference. The compiler should reject obvious lifetime escapes where practical, such as assigning a local fixed-storage object to a global reference or returning it from a function.

---

## 7. Example

```dq
object OCoords:
  function Create(x : int, y : int) [[overload]]:
  endfunc

  function Create() [[overload]]:
    Create(0, 0);
  endfunc
endobj

object OTest:
private
  data : OData = null;       // normal reference field
  coords1 <- OCoords(0, 0);  // embedded storage, constructed here
  coords2 <- OCoords;        // embedded storage, constructed by OTest.Create
  coords3 <- OCoords();      // embedded storage, default constructed here

public
  function Create():
    data = new OData();
    coords2.Create();
  endfunc

  function Destroy():
    delete data;
  endfunc
endobj
```
