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

// as part of object (not allowed in struct)

object:
    child : autofree OChildObj
    echild <- autofree OSomeObj  // ERROR: autofree is invalid for embedded object
endobject

```

## Parents

`autofree` types are valid for:

 * local variables
 * object members
 * arrays
 * dynamic arrays

## Behaviour

Every `autofree` type is a pointer like (pointer or object). These might contain `nil`. Every autofree types, when not initialized exactly, initialized to `nil`.

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
When setting `nil` value to `autofree` types, their previously pointed non-nil content must be freed.
Re-assignments first free the previous non-nil content.

```
var obj = new autofree OHelper()

// ... doing some work with obj

obj = OSubHelper()  // the previous content is released, and a new one replaced !

obj = nil // frees the obj

```

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

