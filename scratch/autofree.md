# autofree types

Add a special `autofree` type designator to the DQ programming language

## Syntax

The `autofree` is a type extension, similarly to pointers. The `autofree` can be used only for objects and pointer types.

```
var obj : autofree OMyObj
var obj = new autofree OMyObj()
var obj : autofree ? = FuncWithObjResult()

var st   : autofree ^SMyStruct
var barr : autofree ^byte

// as part of object (not allowed in structs or unions)

object:
    child : autofree OChildObj
    echild <- autofree OSomeObj  // ERROR: autofree is invalid for embedded object
endobject

struct:
    child : autofree OChildObj  // ERROR: autofree is invalid in structs
endstruct

union:
    child : autofree OChildObj  // ERROR: autofree is invalid in unions
endunion

```

## Parents

`autofree` types are valid for:

 * local variables
 * global variables
 * object members
 * arrays
 * dynamic arrays

`autofree` is not allowed in structs or unions. Structs currently do not
support managed members, so this keeps the same restriction for `autofree`.

## Behaviour

Every `autofree` type is pointer-like (pointer or object) and can contain
`nil`. When not explicitly initialized, an `autofree` value is initialized to
`nil`.

Upon leaving scope, the `autofree` types are automatically freed, when they not `nil`.

```
if x > 0:
    var bp : autofree ^byte  // initialized to nil
    if y > 0:
        return // bp was nil, no free required
    endif
    bp = ^byte(MemAlloc(1000))

    // ... doing some work with bp

    if z > 0:
        return  // bp must be automatically freed, as it is not nil
    endif

endif // bp must be automatically freed, as it is not nil

```
When setting `nil` value to an `autofree` type, its previously pointed non-nil
content must be freed. Re-assignments first free the previous non-nil content.

Assigning from another `autofree` value transfers ownership: the target
receives the pointer and the source is set to `nil`. Exact self-assignment is a
no-op.

Assigning a normal object or pointer value to an `autofree` value claims
ownership of that value. The original normal value remains an ordinary alias;
the user must not free it afterwards.

```
var obj = new autofree OHelper()

// ... doing some work with obj

obj = OSubHelper()  // the previous content is released, and a new one replaced !

obj = nil // frees the obj

var obj2 : autofree OHelper = new OHelper()
obj = obj2  // frees the previous obj, transfers obj2, and sets obj2 to nil
obj = obj   // no-op

```

`delete` is allowed for `autofree` values. It always sets the target to `nil`
after freeing it, so it cannot be freed again during automatic cleanup.

When used in fixed-length arrays:

```
if x > 0:
    var farr : [3] autofree OHelper  // all members initialized to nil
    farr[0] = OHelper(1)
    farr[2] = OHelper(5)
    farr[0] = OHelper(11) // the previous farr[0] is freed
endif // farr[0], farr[2] is freed automatically

```

When used in dynamic arrays:

```
if x > 0:
    var darr : [*] autofree OHelper  // initialized to empty
    darr.Append(new OHelper(2))
    darr.Append(new OHelper(6))
    darr.Append(new OHelper(9))
    darr[0] = nil
    darr[1] = new OHelper(55) // previous OHelper must be freed
endif // all non-nil values in darr are freed automatically (and then the internal dynamic array helper object)

```

Assignments to an `autofree` array element follow the same rules as standalone
variables. In particular, assigning from another `autofree` element transfers
ownership and clears the source element. `Append`, `Prepend`, and `Insert`
likewise claim ownership from normal values and transfer it from `autofree`
values or elements.

`Clone()` is not allowed on a dynamic array whose element type is `autofree`:
cloning would create two independent arrays owning the same elements.

`Pop()` and `PopFirst()` remove the element without freeing it, but their
result type is the normal, non-`autofree` `T`. The caller must either delete
the returned value manually or assign it to an `autofree` value.

```
var item = darr.Pop()                  // ordinary OHelper: delete it manually
var owned : autofree ? = darr.Pop()    // claims ownership for automatic cleanup
```

`Delete()` removes and frees the selected `autofree` elements.

Function arguments:
```
func ObjManipulator(aobj : ref autofree OSomeObject):
    aobj = new OSomeObject(66)  // should free the previously pointed object,
                                // when aobj was not nil before
endfunc

ObjManipulator(o1)  // valid

func ObjManipulator2(aobj : ref OSomeObject):
    aobj = new OSomeObject(99)
endfunc

ObjManipulator2(o1)  // ERROR: o1 is autofree type,
                     // ObjManipulator2 expects non-autofree

func ObjUser(aobj : OSomeObject):
    if aobj <> nil:
        aobj.DoWork()
    endif
endfunc

ObjUser(o1) // ok, accepts autofree and non-autofree arguments too


func SomeCreator() -> autofree OSomeObject:  // ERROR: returning an autofree type is not allowed
    return new OSomeObject(33)
endfunc

```

An `autofree` argument passed to a normal value parameter is borrowed. The
callee may use it but must not retain or free it. An `autofree` value may be
passed by `ref` only to a matching `ref autofree T` parameter.

Function result types cannot be `autofree`. A normal object or pointer result
can be assigned to an `autofree` variable, which claims ownership of it.

Object members:

```
object OHandler:
    child : autofree OHelper // initialized to nil, when assigned to non-nil,
                             // will be automatically freed on destruction
endobject
```

Objects are freed by calling `Destroy` followed by `MemFree`. Pointer values
are freed with `MemFree`; therefore, an `autofree` pointer must refer to memory
that is compatible with `MemFree`.

Global `autofree` values are initialized to `nil` and support ownership
transfers and cleanup on reassignment. They do not receive automatic shutdown
cleanup until DQ has a global/module de-initialization routine.
