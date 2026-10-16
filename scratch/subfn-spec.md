# DQ `subfn` Specification

## 1. Purpose

`subfn` defines a local helper function inside a normal function or object member function. A `subfn` may not contain another `subfn`.

The main goals are:

- allow short helper functions to be placed close to the code that uses them;
- allow those helpers to access variables of the enclosing function;
- allow helpers inside object member functions to access `self`;
- keep the implementation efficient and compatible with LLVM;
- avoid requiring heap-allocated closures for the initial implementation;
- keep `subfn` lifetime strictly bounded by the execution of the enclosing function.

Example:

```dq
func SomeFunc() -> int:
    var a : [*]int

    subfn AppendArr(avalue : int):
        a.Append(avalue)
    endsubfn

    subfn SumArr() -> int:
        result = 0
        for i :? in a:
            result += i
        endfor
    endsubfn

    AppendArr(1)
    AppendArr(2)
    result = SumArr()
endfunc
```

---

## 2. Syntax

A subfunction is declared inside a function body:

```dq
subfn <name>(<arguments>) [-> <return-type>]:
    <body>
endsubfn
```

Example:

```dq
subfn Add(a : int, b : int) -> int:
    result = a + b
endsubfn
```

The argument and return type rules are the same as for normal DQ functions unless explicitly stated otherwise in this specification.

`subfn` declarations are only valid inside:

- normal functions;
- object member functions.

A `subfn` must not be declared at module/global scope or anywhere within another `subfn` body, including inside its control-flow blocks. Only one level of subfunctions is allowed.

---

## 3. Lexical Scope

A `subfn` has normal lexical access to names visible at its declaration point.

It may access:

- its own parameters;
- its own local variables;
- constants visible from the enclosing scope;
- local variables of the enclosing function or method;
- parameters of the enclosing function or method;
- `self`, when lexically available;
- object members accessible through `self`;
- module/global symbols normally visible at that location.

Example:

```dq
func Calc(a : int) -> int:
    var scale : int = 3

    subfn Mul(x : int) -> int:
        result = x * scale + a
    endsubfn

    result = Mul(10)
endfunc
```

Here `Mul()` captures:

- `scale`;
- `a`.

---

## 4. `self` in Object Member Functions

When the enclosing function is an object member function, `self` is part of the lexical environment.

Example:

```dq
object OTest:
    value : int

    func Calc(a : int) -> int:

        subfn AddValue(x : int) -> int:
            result = self.value + x
        endsubfn

        result = AddValue(a)
    endfunc
endobject
```

`AddValue()` captures `self`.

### 4.1 Implicit member access

If DQ allows object members to be referenced without explicitly writing `self`, then such a reference still counts as a capture of `self`.

Example:

```dq
object OTest:
    value : int

    func Inc():
        subfn DoInc():
            value += 1
        endsubfn

        DoInc()
    endfunc
endobject
```

If `value` resolves to `self.value`, then `DoInc()` implicitly captures `self`.

The frontend should preferably represent such a resolved member access explicitly in the AST or semantic representation, for example:

```text
MemberRef(
    object = SelfRef(),
    member = value
)
```

This makes capture detection straightforward.

### 4.2 Keep member function scope restrictions

```

object OTest:
    value : int

    func SomeFunc():
        subfn PrintValue():
            @.PrintLn('value={}', [value])  // @. is required as the global value symbols are not visible from object method scope by default
        endsubfn

        PrintValue()
    endfunc
endobject
```

---

## 5. Capture Semantics

A captured symbol is a symbol owned by the enclosing normal function or method and needed by the `subfn`, either for its own body or to supply a sibling subfunction's hidden arguments.

Every captured lexical variable is passed directly as an implicit hidden parameter.

- Mutable captured variables are passed by reference to their original storage, even when a particular subfunction only reads them.
- Immutable captured values are passed by value; compile-time constants need no runtime parameter when they can be resolved directly.
- Captured `self` is passed as its existing object pointer, without an additional level of indirection.

No capture-context structure is needed for non-escaping `subfn`s.

Conceptually:

```dq
func F():
    var a : int = 1

    subfn G():
        a += 1
    endsubfn

    G()
endfunc
```

must modify the original `a` belonging to `F()`.

It must not operate on an independent copy.

### 5.1 Captured parameters

Enclosing function parameters are also capturable.

```dq
func F(a : int):
    subfn G():
        print(a)
    endsubfn
endfunc
```

`a` is captured from the enclosing function.

### 5.2 Captured dynamic values

For types whose variable storage contains mutable descriptor state, the subfunction must access the original variable storage.

Example:

```dq
var a : [*]int

subfn AppendValue(v : int):
    a.Append(v)
endsubfn
```

If `Append()` can update the dynamic-array descriptor, the subfunction must operate on the same descriptor variable as the enclosing function.

Passing only a copy of the descriptor is not sufficient.

---

## 6. Capture Analysis

The compiler must determine which enclosing local symbols are used by each `subfn`.

For each resolved symbol reference inside a `subfn`:

1. determine the symbol's owning scope;
2. if it belongs to the current `subfn`, it is local and not captured;
3. if it belongs to the enclosing normal function or method, it is captured;
4. if it is module/global/static storage, no closure capture is required;
5. if the reference resolves to an object member through `self`, mark `self` as captured.

Each `subfn` should maintain a capture list.

Conceptually:

```text
SubFunctionSymbol:
    name
    parameters
    return_type
    local_symbols
    captures[]
```

Each capture entry should at minimum identify:

```text
Capture:
    symbol
    source_scope
    access_mode
```

The access mode determines the corresponding hidden parameter:

```text
mutable variable -> by_reference
immutable value  -> by_value
self             -> existing object pointer
```

Capture analysis must also include enclosing symbols needed to supply hidden arguments to called sibling subfunctions, even when the caller does not otherwise reference those symbols. A fixed-point pass over the sibling call graph computes these requirements. See sections 19 and 20.

---

## 7. Name Resolution

A `subfn` participates in lexical name lookup.

Recommended lookup order inside a subfunction:

1. local variables of the subfunction;
2. parameters of the subfunction;
3. symbols from the enclosing normal function or method scope;
4. object members through the enclosing `self`, when applicable;
5. module/global/imported symbols according to normal DQ rules.

Normal shadowing rules apply.

Example:

```dq
func F():
    var x : int = 1

    subfn G():
        var x : int = 2
        print(x)
    endsubfn
endfunc
```

`G()` refers to its own local `x` and does not capture the outer `x`.
Shadowing warning should be issued in this case too.

---

## 8. Visibility of a `subfn`

A `subfn` is visible only inside its enclosing lexical function scope.

Example:

```dq
func F():
    subfn G():
    endsubfn

    G() // valid
endfunc

func H():
    G() // invalid
endfunc
```

A `subfn` symbol must not be exported as a module-level function.

Its generated LLVM function may use `internal` linkage.

---

## 9. Declaration Order

Recommended initial rule:

A `subfn` follows the same declaration-before-use rules as other local declarations unless DQ already provides whole-function predeclaration behavior.

If the compiler already scans function-local declarations before compiling statements, forward calls may be allowed.

Example, depending on existing DQ declaration rules:

```dq
Foo()

subfn Foo():
endSubfn
```

The implementation should remain consistent with DQ's existing local symbol declaration semantics.

---

## 10. Lifetime Restrictions

For the initial implementation, a `subfn` must not outlive the invocation of its enclosing function.

Therefore:

- it may be called directly;
- it may call a lexically visible sibling `subfn`;
- its address must not be stored as an ordinary function pointer;
- it must not be returned;
- it must not be stored in an object, global variable, or heap object;
- it must not be passed to an API expecting a plain function pointer unless it captures nothing and the compiler explicitly supports that conversion.

This restriction avoids heap-allocated closure environments.

Example that should initially be rejected:

```dq

type FSomeFunc = func()

func MainFunc() -> FSomeFunc:
    var x : int

    subfn SubFunc():
        PrintLn(x)
    endsubfn

    result = SubFunc   // error
endfunc
```

Diagnostic example:

```text
error: address of capturing subfunction 'SubFunc' cannot be taken
```

---

## 11. Non-Capturing Subfunctions

A `subfn` that captures no enclosing local state can be lowered exactly like a private normal function.

Example:

```dq
func F():
    subfn Add(a : int, b : int) -> int:
        result = a + b
    endsubfn

    result = Add(1, 2)
endfunc
```

Possible lowering:

```text
F$Add(a, b)
```

No hidden capture parameters are required.

Such a function may later be eligible for conversion to a normal function pointer if DQ chooses to support that behavior.

---

## 12. LLVM Representation

LLVM IR does not contain lexically nested function definitions.

Each DQ `subfn` must therefore be emitted as a normal LLVM function, normally with `internal` linkage.

Example source:

```dq
func SomeFunc():
    var a : [*]int

    subfn AppendArr(v : int):
        a.Append(v)
    endsubfn

    AppendArr(1)
endfunc
```

Conceptual lowering:

```text
SomeFunc()
SomeFunc$AppendArr(^a, v)
```

Each captured lexical variable has its own hidden parameter. Here `^a` denotes a reference to the original dynamic-array descriptor variable.

---

## 13. Hidden Capture Parameters

A capturing `subfn` receives one implicit hidden parameter for each captured lexical variable.

Conceptually:

```text
SomeFunc$AppendArr(^[*]int a, int v)
```

Hidden capture parameters precede normal source parameters. They are not visible in DQ source code and do not participate in overload resolution as source-level arguments.

Conceptual LLVM IR:

```llvm
define internal void @SomeFunc$AppendArr(
    ptr %a,
    i32 %v
) {
    ...
}
```

The hidden `%a` parameter points directly to the enclosing variable's storage. There is no intermediate context pointer or context structure.

LLVM's `nest` attribute is not required for normal direct `subfn` calls.

---

## 14. Capture Parameter List

Only values actually required by a subfunction, including those needed to call sibling subfunctions, belong in its hidden parameter list.

Example:

```dq
func F():
    var a : int
    var b : int
    var c : int

    subfn G():
        a += c
    endsubfn
endfunc
```

Conceptual signature and call:

```text
F$G(^int a, ^int c)
F$G(^a, ^c)
```

`b` is not captured. Both `a` and `c` are mutable variables, so both are passed by reference even though `G()` only reads `c`.

### 14.1 Capture representation

For normal local mutable variables, pass a pointer/reference to the original variable storage.

For `self`, pass the existing object pointer directly.

Each captured symbol appears only once in a subfunction's hidden parameter list, even if it is needed both by the body and by a called subfunction.

Use a deterministic order for hidden parameters and the same order at every call site. The exact order is implementation-defined.

---

## 15. Direct Capture Passing

The same rule applies regardless of the number of captures.

### 15.1 No captures

```text
Parent$Subfn(args...)
```

### 15.2 Only `self` captured

```text
Parent$Subfn(self, args...)
```

### 15.3 Single ordinary mutable capture

```text
Parent$Subfn(^a, args...)
```

### 15.4 Multiple mutable captures

```text
Parent$Subfn(^a, ^b, ^c, args...)
```

Direct hidden parameters expose captured values and references to LLVM without requiring it to unpack a context structure.

The normal target calling convention determines which arguments use registers and which use the stack. A large capture count does not require a separate context structure: arguments that do not fit in registers use the stack according to the normal ABI.

---

## 16. Capture Storage and Lifetime

No capture-context object is created or initialized.

At each call, the caller supplies the original storage references, immutable values, and object pointers required by the callee's capture list.

Mutable captured variables must have addressable storage when their references are passed. LLVM may optimize that storage when it can preserve the required semantics.

Captured storage remains owned by its declaring function invocation and is valid while directly called subfunctions execute. Capturing a variable does not copy its mutable storage or transfer its ownership.

No heap allocation is required under the initial lifetime restrictions.

---

## 17. No Subfunction Nesting

A `subfn` may not contain another `subfn`, even if neither captures any variables.

All subfunctions belonging to a normal function or method are siblings. Capture analysis only needs that one enclosing function or method and its sibling call graph; there are no intermediate subfunction scopes through which captures must propagate.

---

## 18. Recursive `subfn`

A `subfn` may call itself if normal DQ recursion rules permit it.

Example:

```dq
func F():
    subfn Fact(n : int) -> int:
        if n <= 1:
            result = 1
        else:
            result = n * Fact(n - 1)
        endif
    endsubfn

    result = Fact(5)
endfunc
```

The recursive call must forward the same hidden capture arguments as the current invocation, preserving the original storage references and `self` pointer.

For a non-capturing subfunction, recursion requires no hidden capture parameters.

---

## 19. Calls Between Sibling Subfunctions

Sibling subfunctions share the same enclosing normal function or method. They may call each other if they are visible according to DQ's declaration rules.

Example:

```dq
func F():
    var x : int

    subfn A():
        x += 1
    endsubfn

    subfn B():
        A()
    endsubfn

    B()
endfunc
```

If `B()` calls `A()`, `B()` must have access to every hidden capture argument required by `A()`.

Here `B()` captures `x` solely to forward it to `A()`:

```text
F():
    F$B(^x)

F$B(^int x):
    F$A(x)

F$A(^int x):
    increment the variable referenced by x
```

The hidden `x` parameter inside the lowered `B()` already holds the reference to the enclosing variable. Forward it unchanged; do not take the address of the hidden parameter itself.

Sibling calls use direct hidden arguments, with no shared environment structure.

---

## 20. Required-Capture Propagation Between Siblings

Each subfunction's capture list includes:

- enclosing symbols referenced by its own body;
- enclosing symbols needed to supply hidden arguments to sibling subfunctions it calls.

Example:

```dq
func F(a : int):
    var x : int
    var y : int
    var z : int

    subfn A():
        x += a
    endsubfn

    subfn B():
        y += x
        A()
    endsubfn
endfunc
```

Conceptual signatures and call:

```text
F$A(^int a, ^int x)
F$B(^int a, ^int x, ^int y)

inside B: F$A(a, x)
```

`B()` needs `a` only to call `A()`. `z` is not captured by either subfunction.

For each enclosing normal function or method, run a simple fixed-point pass over its sibling call graph:

1. initialize each subfunction's required-capture set with the enclosing symbols referenced by its own body;
2. for every call from subfunction `Caller` to sibling `Callee`, add `Callee`'s required captures to `Caller`'s set;
3. repeat step 2 until no set changes.

Conceptually:

```text
required[S] = direct_captures[S]
repeat until unchanged:
    for each sibling call Caller -> Callee:
        required[Caller] |= required[Callee]
```

This handles multi-hop sibling calls and recursive call cycles where permitted by declaration rules. Every propagated symbol belongs to the same enclosing function or method; a sibling's own locals and parameters are never propagated as captures.

Track captures by resolved symbol identity, so a caller's local with the same name does not replace an enclosing variable required by its callee.

The enclosing function or method supplies its own storage, values, and `self` pointer when calling a subfunction. Sibling and recursive calls forward the corresponding hidden arguments.

Finalize capture lists and their ordering before generating LLVM signatures and calls. No environment type or multi-level capture propagation is needed.

---

## 21. Interaction with Object Methods

For an object method:

```dq
object OParser:
    tokenidx : int

    func Parse():
        var count : int

        subfn NextToken():
            ++tokenidx
            ++count
        endsubfn

        NextToken()
    endfunc
endobject
```

The generated subfunction behaves approximately like:

```text
OParser$Parse$NextToken(^OParser self, ^int count)
```

The enclosing method calls it with:

```text
OParser$Parse$NextToken(self, ^count)
```

Member access uses the hidden `self` object pointer directly:

```text
self.tokenidx
```

`count` is accessed through its hidden storage reference. No environment structure or extra indirection for `self` is needed.

---

## 22. `result` Handling

A `subfn` has its own `result` symbol exactly like a normal function.

Example:

```dq
subfn Sum() -> int:
    result = 123
endsubfn
```

The `result` inside `Sum()` refers to the return value of `Sum()`, not to the enclosing function's `result`.

The enclosing function's `result` is shadowed by the subfunction's own result symbol.

If access to an enclosing result value is ever desired, it should require an explicit language mechanism; it should not happen accidentally through capture.

---

## 23. Exceptions and Cleanup

A `subfn` behaves like a normal DQ function regarding:

- exceptions;
- stack unwinding;
- destructors;
- local variable cleanup;
- backtrace/debug information.

Hidden capture parameters do not change exception semantics.

Captured storage remains valid while directly called subfunctions execute. Cleanup and destruction of captured variables remain the responsibility of their declaring scope.

---

## 24. Debug Information

Generated debug information should identify the source `subfn` as a lexical child of its enclosing function where practical.

The generated LLVM symbol name may be mangled, but source-level debugging should preferably display the original `subfn` name.

Example generated symbol:

```text
_ZDQ...SomeFunc$AppendArr...
```

or another DQ-specific mangling format.

The exact mangling scheme is implementation-defined.

---

## 25. Suggested Compiler Frontend Structure

Recommended semantic objects:

```text
FunctionSymbol
    parent_function_scope
    is_subfn
    parameters
    locals
    captures
    subfunctions
```

Only normal functions and object member functions may have subfunctions; a subfunction's `subfunctions` list must be empty. Its `parent_function_scope` is the enclosing normal function or method.

Each local symbol should identify its owning function scope:

```text
VariableSymbol
    owner_function
    type
    storage
```

During symbol resolution:

```text
resolve identifier
    -> obtain VariableSymbol
    -> compare current_function with symbol.owner_function
    -> if current_function is a subfn and owner is its parent_function_scope:
           register capture
```

For `self`:

```text
SelfSymbol
    owner_method
    type = ^ObjectType
```

The same capture mechanism can then handle `self`.

---

## 26. Suggested Implementation Phases

### Phase 1: Parsing

Add:

- `subfn` keyword;
- `endsubfn` keyword;
- AST node for subfunction declaration;
- parser support inside normal function and object member function bodies.

Reject any `subfn` declaration encountered while parsing a subfunction body, including inside control-flow blocks.

### Phase 2: Symbol tables

Add one level of subfunction scopes under normal function and object member function scopes.

A `subfn` symbol must be inserted into the enclosing function-local symbol table.

### Phase 3: Lexical symbol lookup

Allow identifier resolution to continue into the enclosing normal function or method scope.

Track the owning function for resolved locals and parameters.

### Phase 4: Capture analysis

When an outer local/parameter/`self` is referenced, add it to the current subfunction's capture set.

Run the fixed-point pass from section 20 over the sibling call graph of each enclosing function or method until no required-capture set changes.

### Phase 5: Capture parameter planning

Finalize one deterministic capture list per subfunction.

Assign one hidden parameter to each capture: a reference for mutable variables, a value for immutable runtime captures, and the existing object pointer for `self`. Do not generate environment/context types.

### Phase 6: LLVM function generation

Generate every `subfn` as a module-level LLVM function with `internal` linkage.

Prepend the planned hidden capture parameters to each generated subfunction signature.

### Phase 7: Call generation

When generating a call to a `subfn`:

- emit normal source arguments;
- prepend one hidden argument per capture in the callee's capture-list order;
- supply the enclosing function's storage, values, and `self` pointer for calls from that function;
- forward existing hidden arguments for sibling and recursive calls.

### Phase 8: Diagnostics

Reject subfunction nesting and unsupported escaping/address-taking cases.

### Phase 9: Debug information

Associate generated functions with source-level nested scopes if practical.

---

## 27. Diagnostics

Recommended diagnostics include:

### Invalid global declaration

```dq
subfn F():
endsubfn
```

```text
error: 'subfn' is only allowed inside a function
```

### Forbidden subfunction nesting

```dq
func F():
    subfn Outer():
        subfn Inner():
        endsubfn
    endsubfn
endfunc
```

```text
error: a 'subfn' cannot be declared inside another 'subfn'
```

### Escaping capturing subfunction

```dq
var p = %Nested
```

```text
error: address of capturing subfunction 'Nested' cannot be taken
```

### Returning a capturing subfunction

```dq
result = Nested
```

```text
error: capturing subfunction 'Nested' cannot escape its enclosing function
```

### Invalid use outside scope

```dq
func F():
    subfn G():
    endsubfn
endfunc

func H():
    G()
endfunc
```

```text
error: unknown identifier 'G'
```

---

## 28. Initial Restrictions

The first implementation should intentionally avoid full closures.

Recommended initial restrictions:

- `subfn` may access enclosing locals and `self`;
- each captured lexical variable is passed as its own hidden parameter;
- mutable captures use references to original storage;
- `self` uses its existing object pointer;
- captured storage lifetime remains bounded by its declaring invocation;
- no capture-context structure is required;
- direct calls are supported;
- recursive calls are supported;
- a `subfn` must not contain another `subfn`;
- required captures propagate only through the one-level sibling call graph;
- sibling subfunction calls are supported;
- capturing subfunction addresses cannot be taken;
- capturing subfunctions cannot be returned or stored;
- no heap closure allocation;
- no LLVM trampolines are required.

These restrictions preserve simple and deterministic runtime behavior.

---

## 29. Future Extension: First-Class Closures

A future DQ version may allow a nested function to escape its enclosing scope.

That would require a closure representation conceptually similar to:

```text
Closure:
    function_ptr
    context_ptr
```

If the context outlives the enclosing function, captured storage must be moved or copied to lifetime-managed memory.

This future feature is explicitly outside the scope of the initial `subfn` implementation.

LLVM trampoline intrinsics are not recommended as the primary DQ closure representation because:

- they are target-dependent;
- they may require executable writable memory;
- they are undesirable on embedded targets;
- they complicate portability and security.

An explicit `{code pointer, context pointer}` closure representation is preferred if first-class closures are added later.

---

## 30. LLVM `nest` Attribute

LLVM provides a `nest` parameter attribute intended for language implementations involving nested functions/static chains.

DQ does not need to use it for the initial `subfn` implementation.

Normal hidden capture arguments are sufficient:

```llvm
define internal i32 @Parent$Nested(
    ptr %captured_x,
    ptr %self,
    i32 %arg
)
```

This representation is:

- portable;
- explicit;
- straightforward to generate;
- straightforward to debug;
- suitable for embedded targets;
- optimizer-friendly.

The compiler may later evaluate LLVM's `nest` machinery if another ABI requirement makes it useful.

---

## 31. Optimization Expectations

Generated nested helper functions should normally use `internal` linkage.

LLVM can then perform optimizations such as:

- inlining;
- propagation of captured values and references;
- elimination of unused parameters;
- promotion of captured variable storage to registers when legal;
- simplification of loads and stores after inlining.

Passing captures directly exposes them to LLVM without context allocation, field initialization, or field loads. The frontend should emit simple, correct direct parameters and let LLVM optimize them under the normal target calling convention.

Example source:

```dq
func F():
    var x : int = 3

    subfn G(a : int) -> int:
        result = x + a
    endsubfn

    result = G(4)
endfunc
```

After inlining/optimization, LLVM may eliminate the captured variable storage and fold the result to `7`.

---

## 32. Required Test Cases

### 32.1 Non-capturing subfunction

```dq
func Test() -> int:
    subfn Add(a : int, b : int) -> int:
        result = a + b
    endsubfn

    result = Add(2, 3)
endfunc
```

Expected result: `5`.

### 32.2 Capture local by reference

```dq
func Test() -> int:
    var a : int = 1

    subfn Inc():
        ++a
    endsubfn

    Inc()
    result = a
endfunc
```

Expected result: `2`.

### 32.3 Capture enclosing parameter

```dq
func Test(a : int) -> int:
    subfn Get() -> int:
        result = a
    endsubfn

    result = Get()
endfunc
```

`Test(7)` returns `7`.

### 32.4 Capture multiple values

```dq
func Test(a : int) -> int:
    var b : int = 3

    subfn Calc() -> int:
        result = a + b
    endsubfn

    result = Calc()
endfunc
```

`Test(4)` returns `7`.

### 32.5 Capture dynamic array descriptor

```dq
func Test() -> int:
    var a : [*]int

    subfn Add(v : int):
        a.Append(v)
    endsubfn

    Add(10)
    Add(20)
    result = a[1]
endfunc
```

Expected result: `20`.

### 32.6 Capture `self`

```dq
object OTest:
    value : int

    func GetPlus(a : int) -> int:
        subfn Calc() -> int:
            result = self.value + a
        endsubfn

        result = Calc()
    endfunc
endobject
```

Expected: object member and enclosing argument are both accessible.

### 32.7 Implicit `self`

```dq
object OTest:
    value : int

    func Inc():
        subfn DoIt():
            ++value
        endsubfn

        DoIt()
    endfunc
endobject
```

Expected: `value` resolves through `self`.

### 32.8 Local shadowing

```dq
func Test() -> int:
    var a : int = 1

    subfn Get() -> int:
        var a : int = 2
        result = a
    endsubfn

    result = Get()
endfunc
```

Expected result: `2`.

Outer `a` is not captured.

### 32.9 Recursive subfunction

```dq
func Test() -> int:
    subfn Fact(n : int) -> int:
        if n <= 1:
            result = 1
        else:
            result = n * Fact(n - 1)
        endif
    endsubfn

    result = Fact(5)
endfunc
```

Expected result: `120`.

### 32.10 Recursive capturing subfunction

```dq
func Test() -> int:
    var bias : int = 1

    subfn Fact(n : int) -> int:
        if n <= 1:
            result = bias
        else:
            result = n * Fact(n - 1)
        endif
    endsubfn

    result = Fact(3)
endfunc
```

Expected result: `6`.

The recursive call must forward the hidden reference to `bias` unchanged.

### 32.11 Reject subfunction nesting

```dq
func Test():
    subfn Outer():
        subfn Inner():
        endsubfn
    endsubfn
endfunc
```

Expected: compilation error, even though neither subfunction captures anything. Also reject a `subfn` declared inside a control-flow block within `Outer()`.

### 32.12 Sibling call chain

```dq
func Test() -> int:
    var a : int = 0

    subfn Inc():
        ++a
    endsubfn

    subfn Run():
        Inc()
    endsubfn

    subfn Start():
        Run()
    endsubfn

    Start()
    result = a
endfunc
```

Expected result: `1`. Both `Run()` and `Start()` require the hidden reference to `a` solely to forward it along the sibling call chain.

### 32.13 Separate `result`

```dq
func Test() -> int:
    subfn Get() -> int:
        result = 5
    endsubfn

    result = Get()
endfunc
```

Expected result: `5`.

The nested `result` belongs only to `Get()`.

### 32.14 Reject escaping capture

```dq
func Test():
    var a : int

    subfn Get():
        print(a)
    endsubfn

    var p = %Get
endfunc
```

Expected: compilation error.

### 32.15 Read-only use of a mutable capture

```dq
func Test() -> int:
    var a : int = 1

    subfn Get() -> int:
        result = a
    endsubfn

    var first : int = Get()
    a = 7
    result = first + Get()
endfunc
```

Expected result: `8`. `Get()` receives a reference to `a` and observes its current value at each call.

### 32.16 Hidden parameter lowering

Verify generated IR before optimization for the capture examples above:

- each runtime capture has its own hidden parameter, with no context structure or context allocation;
- multiple mutable captures are separate references, even when only read by the subfunction;
- `self` is the existing object pointer, without an extra pointer to its storage;
- sibling and recursive calls forward existing references unchanged;
- sibling capture propagation reaches a fixed point for multi-hop calls and call cycles where declaration rules permit them;
- a caller's local that shadows an enclosing variable does not replace the capture required by a sibling callee;
- the enclosing normal function or method supplies its own local storage directly when calling a capturing subfunction;
- captures are deduplicated and supplied in the same order as the callee's hidden parameters.

Also test enough captures and source arguments to exceed the target's argument-register capacity. The call must preserve the same semantics using the normal target ABI for stack arguments, without introducing a capture-context structure.

---

## 33. Recommended First Implementation Model

For the first agentic implementation, use the following model:

1. parse `subfn` inside normal functions and object member functions, rejecting declarations inside subfunctions;
2. create a normal function scope for it;
3. allow lexical lookup into the enclosing normal function or method scope;
4. record every referenced enclosing local/parameter/`self`;
5. compute required captures with a fixed-point pass over each enclosing function's sibling call graph;
6. finalize a deterministic hidden parameter list for each subfunction;
7. pass mutable captures by reference, immutable runtime captures by value, and `self` as its existing object pointer;
8. emit each `subfn` as an LLVM module-level `internal` function;
9. prepend one hidden parameter per captured lexical variable;
10. supply hidden arguments from the enclosing function or method and forward them at recursive and sibling calls;
11. reject escaping/address-taking of capturing subfunctions;
12. rely on the normal target calling convention and LLVM optimization for argument placement and optimization.

No capture-context structure, shared environment, or static chain is needed for non-escaping subfunctions.

---

## 34. Summary

`subfn` is a local helper function with access to its enclosing normal function or method. Subfunctions cannot contain further subfunctions.

Core semantics:

```text
subfn
    + normal parameters/local variables
    + lexical access to enclosing locals
    + lexical access to enclosing parameters
    + lexical access to self
    + one direct hidden parameter per captured lexical variable
    + reference semantics for captured mutable variables
    + existing object pointer for captured self
    + fixed-point capture propagation between siblings
    + stack-bounded lifetime
    - no subfunctions inside subfunctions
    - no capture-context structure
    - no escaping closure in the initial implementation
```

Core LLVM lowering:

```text
DQ nested function
        |
        v
LLVM internal module-level function
        +
individual hidden capture parameters
```

This design gives DQ convenient Pascal-style/local helper functions while keeping the generated LLVM model explicit, efficient, portable, and suitable for both desktop and embedded targets.
