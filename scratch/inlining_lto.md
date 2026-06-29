# Discuss function inlining

> The current DQ compiler is in the compiler subdirectory. It precompiles the modules into .dqm files which have a special .dqm_if section, that represents the public AST for that module.
>
> The DQ currently does not support function inlineing. How could be that implemented ? (do not start implement now just discuss)

<details><summary>12 previous messages</summary>

> I’ll inspect the compiler layout and the existing module/object format enough to ground the discussion in this codebase, then I’ll stay at design level as requested.
>
> <details><summary>Explored a list, ran 2 commands</summary>
>
> - Ran `rg -n "dqm|dqm_if|inline|module|public AST|section" compiler src Makefile CMakeLists.txt 2>/dev/null`
>
> <details><summary>Explored a list</summary>
>
> - Listed files in `compiler`
>
> </details>
>
> - Ran `git status --short`
>
> </details>
>
> I found the expected pieces: module interfaces are serialized under `compiler/ast/dqm_if.*`, imported through `module_intf.*`, and functions live in `types/otype_func.*`. I’m going a layer deeper now: specifically whether bodies are currently serializable or only signatures/references are.
>
> <details><summary>Explored 4 files</summary>
>
> - Read `./compiler/ast/dqm_if.cpp`
> - Read `./compiler/ast/dqm_if.h`
> - Read `./compiler/types/otype_func.h`
> - Read `./compiler/ast/statements.h`
>
> </details>
>
> The interface format currently records declarations, types, constants, variables, functions, parameters, uses, and link deps, but I don’t see body records. That strongly shapes the answer: same-module inlining is mostly an AST/codegen problem, cross-module inlining needs either serialized inline bodies or a lower-level IR/LTO route.
>
> <details><summary>Explored 4 files</summary>
>
> - Read `./compiler/types/otype_func.cpp`
> - Read `./compiler/ast/module_intf.h`
> - Read `./compiler/ast/module_intf.cpp`
> - Read `./compiler/codegen/dqc_codegen.cpp`
>
> </details>
>
> One useful detail: `.dqm` is just an object file with the interface bytes embedded as a private global in `.dqm_if`. That means imported modules are presently linked as native object code, while the compiler only “sees” their public signatures from the embedded interface.
>
> <details><summary>Explored 6 files, 2 searches</summary>
>
> - Searched for `WriteDqmIfFunction|WriteDqmIfDecl|ReadFunctionDecl|AddLoadedFunction|GenerateConversion\(|CreateCall|OExprCall|FuncCall` in `otype_func.cpp`
> - Read `./compiler/ast/module_intf.cpp`
> - Read `./compiler/types/otype_func.cpp`
> - Read `./compiler/ast/expressions.h`
> - Read `./compiler/ast/module_intf.cpp`
> - Read `./compiler/ast/expressions.cpp`
> - Read `./compiler/types/otype_func.cpp`
> - Searched for `class O.*Call|target_func|callee|func` in `expressions.h`
>
> </details>
>
> Call generation is centralized enough to imagine a neat hook: `OCallExpr::Generate()` builds converted LLVM arguments, then `GenerateFunctionCall()` emits either a direct call or virtual dispatch. For AST-level inline expansion, that hook is too late for source-level semantics but a good place to attach LLVM attributes or decide direct-call eligibility.
>
> <details><summary>Explored 11 files, a search</summary>
>
> - Read `./compiler/ast/expressions.h`
> - Searched for `FoldTree|FoldChildren|TryFoldSelf|GenerateFuncBody|GenGlobalDecl|MPM|PassBuilder|Inliner|AlwaysInline|Optimize` in `ast`
> - Read `./compiler/codegen/dqc_codegen.cpp`
> - Read `./compiler/ast/module_intf.cpp`
> - Read `./compiler/parser/dqc_parser_expr.cpp`
> - Read `./compiler/ast/attributes.h`
> - Read `./compiler/ast/symbols.cpp`
> - Read `./compiler/ast/symbols.h`
> - Read `./compiler/ast/attributes.cpp`
> - Read `./compiler/parser/dqc_parser_expr.cpp`
> - Read `./compiler/ast/symbols.h`
> - Read `./compiler/ast/module_intf.cpp`
>
> </details>
>
> The attribute system already has room for this: function flags are parsed in one place, stored on `OValSym`, and serialized through the existing DQM attribute record. Adding `[[inline]]` / `[[noinline]]` would fit naturally there, but the behavior behind those flags is the real design choice.
>
> <details><summary>Explored 2 files, a search</summary>
>
> - Read `./compiler/ast/attributes.cpp`
> - Read `./compiler/ast/symbols.cpp`
> - Searched for `ApplyAttributes\(|CheckInvalidAttributes|ATTF_` in `ast`
>
> </details>
</details>

Yes, and the module model is exactly the important constraint here.

The short version: DQ can get **same-module inlining cheaply**, but **cross-module inlining needs extra body information**, because `.dqm_if` currently records function declarations/signatures, not function bodies.

I’d think about it in three layers.

**1. Same-Module Inlining**

The compiler already emits all functions from the current source into one LLVM module, then runs LLVM optimization in [dqc_codegen.cpp](./compiler/codegen/dqc_codegen.cpp:384). At `-O1+`, LLVM’s normal pipeline can inline direct calls when it sees both caller and callee.

So the first useful feature would be attributes:

```dq
[[inline]]
function Foo(...) ...
```

and maybe:

```dq
[[always_inline]]
[[noinline]]
```

Implementation-wise this is small:

- add attribute flags in [attributes.h](./compiler/ast/attributes.h:19)
- parse them in [dqc_parser_expr.cpp](./compiler/parser/dqc_parser_expr.cpp:221)
- store them on `OValSymFunc` / `OValSym`
- when creating `LlFunction` in [otype_func.cpp](./compiler/types/otype_func.cpp:668), attach LLVM attributes like `AlwaysInline`, `InlineHint`, or `NoInline`

This would make inline work inside one compiled DQ source/module compilation unit, with very little custom compiler logic.

**2. Cross-Module Inlining**

This is the harder part. Imported modules are loaded from `.dqm_if` in [module_intf.cpp](./compiler/ast/module_intf.cpp:1918). A loaded function gets its signature, module ownership, attributes, and linkage name, but not its body:

```cpp
fn->has_body = false;
```

The final `.dqm` object is linked later, so the caller’s LLVM optimizer cannot see the callee body. Native object code cannot be inlined after the fact unless you use LTO/bitcode.

There are two main designs.

**Option A: LLVM LTO / Bitcode**

Instead of, or in addition to, native object code in `.dqm`, store LLVM bitcode for the module. Then final linking can use LTO so LLVM sees bodies across module boundaries.

Pros:

- least DQ AST serialization work
- LLVM handles heuristics, recursion, cost model, debug info, etc.
- private helpers can still be optimized internally by LTO

Cons:

- `.dqm` format/linking becomes more LLVM-specific
- final link step must become LTO-aware
- harder if you want `.dqm` to remain “ordinary object + interface section”

This is probably the most powerful route.

**Option B: Serialize Inline Function Bodies Into `.dqm_if`**

For functions marked `[[inline]]`, extend the interface format to include a body record inside `DQMIF_FUNC_BEGIN` / `DQMIF_FUNC_END`.

For example conceptually:

```text
DQMIF_FUNC_BEGIN "AddOne"
  attrs
  params
  retval
  DQMIF_FUNC_INLINE_BODY_BEGIN
    serialized statements / expressions
  DQMIF_FUNC_INLINE_BODY_END
DQMIF_FUNC_END
```

Then on import, `OModuleIntf::ReadFunctionDecl()` reconstructs an inline body, not just the signature. At call generation time, [OCallExpr::Generate()](./compiler/ast/expressions.cpp:2160) could ask the target function whether it is inlineable and emit the cloned body instead of `CreateCall`.

This keeps `.dqm` as native object code, but it is a bigger compiler feature.

The tricky parts are:

- cloning expressions/statements with a symbol remap
- mapping parameters to evaluated call arguments
- handling `return`
- preserving side-effect order
- avoiding name collisions
- preventing recursive infinite expansion
- deciding what private symbols an exported inline body may reference
- serializing enough AST but not accidentally exporting private implementation details

I would start with a restricted version:

- only explicit `[[inline]]`
- direct calls only, not virtual dispatch
- no varargs
- no `external`
- no `abstract`
- probably no `refout` at first
- imported inline bodies may only reference public symbols, builtins, constants, and types available through the interface
- first support single-expression / single-return functions

That would already cover many useful cases like tiny math helpers, accessors, wrappers, and type conversions.

**3. Where The Hook Belongs**

I would not make a broad global helper pass first. The ownership boundaries are already pretty clear.

Good homes:

- `OValSymFunc`: `IsInlineCandidate()`, `WriteDqmIfInlineBody()`, `ReadDqmIfInlineBody()`, maybe `GenerateInlineCall(...)`
- `OExpr` / `OStmt`: clone/serialize methods
- `OCallExpr`: decide whether to emit inline expansion or normal call
- `ODqmIfWriter` / `ODqmIfReader`: only low-level record transport

The imported normal callable symbol should still exist, because taking a function reference or failing inline heuristics must fall back to the real linked function.

My preferred path would be:

1. Add `[[inline]]`, `[[always_inline]]`, `[[noinline]]` as LLVM attributes first.
2. Confirm same-module behavior with small tests.
3. Add `.dqm_if` inline-body records only for explicitly marked public functions.
4. Initially allow only simple inline bodies.
5. Later expand to full statement-block cloning if needed.

That gives useful behavior quickly without committing the whole compiler to a large public-AST serialization scheme on day one.

> I think if I want to match gcc speed I have to take the LTO path.

Yes, if you mean **runtime performance comparable to C/GCC across modules**, LTO is probably the right path.

The current `.dqm` model is:

- each module compiles to a native object
- `.dqm_if` carries only public semantic/interface data
- final compilation links already-compiled module objects

That means LLVM can optimize well **inside one module**, but cannot inline or specialize across `.dqm` boundaries. GCC gets a lot of its best whole-program wins through LTO: cross-file inlining, dead code elimination, constant propagation, devirtualization, internalization, etc. DQ would need the same kind of visibility.

I’d consider making `.dqm` a richer artifact:

```text
module.dqm
  native object code, maybe optional
  .dqm_if section
  LLVM bitcode section or companion bitcode payload
```

Then final linking can do one of these:

1. **Full LTO**
   All used module bitcode is loaded, merged, optimized, then emitted.
   Best optimization, slower compile/link.

2. **ThinLTO**
   Each module keeps a summary; linker does cross-module import/inlining selectively.
   Usually the better long-term default: much closer to scalable GCC/Clang behavior.

3. **Hybrid**
   Normal builds link native `.dqm` objects.
   `-flto` / `--lto` builds use embedded bitcode.
   This keeps fast edit/build cycles while enabling release-quality optimization.

I would not serialize DQ AST bodies if your goal is GCC-level performance. AST body import helps inline small public functions, but it will always become a second optimizer. LLVM already has the machinery you want.

The design direction I’d pick:

- Add compiler option: `--lto=off|full|thin`, default `off` initially.
- When compiling a module with LTO enabled, emit LLVM bitcode into the `.dqm` artifact or next to it as `.dqm.bc`.
- Keep `.dqm_if` as the semantic import format for type checking and name resolution.
- At final executable link, collect bitcode for all DQ modules and run LLVM LTO before emitting the final object/executable.
- Still support native `.dqm` fallback for non-LTO builds.

One important thought: if DQ wants C-like performance, LTO should be combined with better linkage/internalization. Public DQ symbols need external linkage for module boundaries, but during final LTO many can become internal if they are not exported to C or externally visible. That unlocks much more aggressive optimization.

So yes: for “match GCC speed”, I’d treat inline body serialization as a convenience feature at most. The serious road is LLVM LTO, probably ThinLTO eventually.

> What is LLVM bitcode ?

LLVM bitcode is LLVM’s **portable, serialized form of LLVM IR**.

Think of the compiler pipeline like this:

```text
DQ source
  -> DQ AST / semantic model
  -> LLVM IR
  -> machine code object file
  -> executable
```

LLVM IR is a low-level, typed, compiler-friendly intermediate language. It is not source code, but it is still much richer than machine code: it has functions, types, control flow, calls, attributes, debug metadata, optimization hints, etc.

LLVM bitcode is that IR saved to a compact binary file, usually `.bc`.

So:

```text
native object .o
  = already lowered to machine code for x86_64/aarch64/etc.

LLVM bitcode .bc
  = still optimizer-visible LLVM IR
```

Why it matters for LTO:

If DQ module `A` calls function `B` from module `B`, and module `B` is already compiled to native object code, LLVM cannot inline `B` into `A`.

But if both modules provide LLVM bitcode, LLVM can load them together during link-time optimization:

```text
A.bc + B.bc
  -> LLVM whole-program optimization
  -> final native object/executable
```

That allows cross-module:

- function inlining
- dead code removal
- constant propagation
- devirtualization
- better alias/escape analysis
- internalization of symbols not externally needed

In your `.dqm` world, a future LTO-capable artifact could be:

```text
foo.dqm
  native object code       optional fast non-LTO path
  .dqm_if section          DQ semantic interface
  LLVM bitcode payload     optimization-visible implementation
```

The `.dqm_if` is for the DQ compiler to understand public types/functions. The bitcode is for LLVM to optimize and generate final machine code.

> Can be the LLVM bitcode embedded into the .dqm files, and can the LLVM linker use the bitcode directly from the .dqm file ?

<details><summary>2 previous messages</summary>

> I’ll ground this in how LLVM/Clang normally package LTO data, because the distinction between “embedded in an object section” and “linker plugin recognizes it” matters a lot here.
>
> <details><summary>Searched the web</summary>
>
> <details><summary>Searched the web</summary>
>
> - Searched the web for `LLVM documentation bitcode object file section LTO linker plugin embedded bitcode`
> - Searched the web for `site:llvm.org/docs ThinLTO LLVM bitcode linker plugin`
> - Searched the web for `https://llvm.org/docs/LinkTimeOptimization.html`
> - Searched the web for `'embed-bitcode' in https://clang.llvm.org/docs/CommandGuide/clang.html`
>
> </details>
>
> </details>
</details>

Yes to the first, “not automatically” to the second.

You can absolutely embed LLVM bitcode into `.dqm` files, just like you already embed `.dqm_if`:

```text
module.dqm
  native object sections
  .dqm_if       DQ public interface
  .dq_llvm_bc   LLVM bitcode blob
```

But a normal LLVM-aware linker will not magically look inside an arbitrary `.dq_llvm_bc` section and use it for LTO. LLVM’s documented LTO model is that the linker treats **LLVM bitcode files themselves** like linkable inputs, using `libLTO` / linker integration. The LLVM docs describe the linker reading native object files and LLVM bitcode files, then passing bitcode modules to the optimizer before final native linking. Clang’s `-flto` similarly emits LLVM bitcode-format object files suitable for LTO, not “normal object plus arbitrary hidden bitcode section” by default. Sources: LLVM LTO design docs and Clang command guide.
https://llvm.org/docs/LinkTimeOptimization.html
https://clang.llvm.org/docs/CommandGuide/clang.html#cmdoption-flto

So for DQ you have three realistic designs:

**1. `.dqm` is bitcode in LTO mode**

In LTO mode, make `foo.dqm` be an LLVM bitcode file, not a native object. The extension can still be `.dqm`; file magic matters more than extension.

Problem: you still need `.dqm_if`. You could embed `.dqm_if` into LLVM IR as a global/section, but then your module interface loader must be able to extract it from bitcode too, not only native object sections.

**2. `.dqm` stays native object, DQ extracts bitcode**

Keep:

```text
foo.dqm
  native code
  .dqm_if
  .dq_llvm_bc
```

Then during final DQ link with `--lto`, the DQ compiler/driver reads each `.dqm`, extracts `.dq_llvm_bc` into memory or temporary `.bc` files, and feeds those to LLVM LTO itself or to `lld/clang -flto`.

This is probably the best fit for your current artifact design.

**3. Separate companion file**

```text
foo.dqm      native object + .dqm_if
foo.dqm.bc   LLVM bitcode
```

Simplest to implement and debug. Less elegant, but very practical.

My preference for DQ: start with option 3, then move to option 2 once the LTO path works. The linker will not use embedded bitcode from a custom `.dqm` section by itself; DQ should either extract it or drive LLVM LTO directly.

> Currently the DQ compiler uses the gcc for linking. I assume that I have to switch to the LLVM linker then.

<details><summary>2 previous messages</summary>

> I’ll quickly check the current link path so I can answer against what DQ actually does, not just the generic LLVM story.
>
> <details><summary>Explored 2 files, a search</summary>
>
> - Read `./compiler/src/dqc.cpp`
> - Searched for `gcc|clang|ld|link|Run|Process|link_module_artifacts|linklib` in `compiler`
> - Read `./compiler/utils/processrunner.h`
>
> </details>
</details>

Mostly yes, but with one nuance: you do not necessarily need to use `ld.lld` directly. You need an **LLVM-aware LTO link path**.

Right now DQ literally builds a command like this in [dqc.cpp](./compiler/src/dqc.cpp:302):

```cpp
gcc current_module.dqm used_module1.dqm used_module2.dqm -o app ...
```

That is fine for native object files, but GCC will not understand LLVM bitcode for LTO. For LLVM bitcode, the easiest path is usually:

```text
clang -flto ... -fuse-ld=lld
```

rather than calling `ld.lld` yourself. `clang` acts as the driver: it adds startup objects, libc, libgcc/compiler-rt, system paths, linker flags, and invokes the actual linker correctly.

So I would think of the options like this:

1. **Non-LTO mode**
   Keep using `gcc` if it works today.

2. **LLVM LTO mode**
   Use `clang -flto` as the link driver, preferably with `-fuse-ld=lld`.

3. **DQ-managed LTO**
   DQ extracts/loads all module bitcode, runs LLVM LTO itself via LLVM APIs, emits one optimized native object, then can still use `gcc` only for the final system link step.

The simplest first implementation is probably option 2:

```text
dq-comp --lto app.dq
  -> compile main module to bitcode
  -> collect dependency module bitcode
  -> run clang -flto -fuse-ld=lld main.bc dep1.bc dep2.bc -o app -l...
```

If `.dqm` contains embedded bitcode, DQ would first extract that bitcode to temp `.bc` files, because `clang/lld` will not automatically search your custom `.dqm` section.

So: yes, for LLVM bitcode LTO, switch away from the current hardcoded `gcc object...` link command. But I would switch to a configurable **link driver**, not hardwire only `ld.lld`. Use `clang` as the first LLVM LTO backend. It will save you a surprising amount of linker/platform pain.

> I would like to create a DQ compiler package for windows. On windows usually no development tools are available by default, the compiler package should contain everything, including the linker.

Yes. For a Windows DQ compiler package, you should assume **nothing useful exists on the user’s machine** except Windows itself.

But “include the linker” really means include the whole **link environment**, not just `lld.exe`.

You need:

- `dq-comp.exe`, `dq-run.exe`
- bundled linker: usually LLVM `lld`
- probably bundled `clang.exe` as the link driver, even if users never call it directly
- target runtime libraries
- Windows import libraries, for example `kernel32`, `user32`, `msvcrt` / `ucrt`, etc.
- startup objects / CRT glue
- DQ runtime modules: `.dq`, `.dqm`, `.dqm_if`
- a config file so DQ knows where its bundled tools/libs live

The big choice is the Windows ABI/toolchain family.

**Option A: MinGW-w64 + LLVM/lld**

This is probably the best self-contained package.

Bundle:

```text
bin/
  dq-comp.exe
  dq-run.exe
  clang.exe
  lld.exe / ld.lld.exe
  llvm-ar.exe maybe

lib/
  mingw-w64 CRT/startup objects
  Windows import libs
  compiler-rt or libgcc-equivalent pieces
  DQ runtime libs/modules

share/ or rtl/
  DQ source modules
```

Then DQ links via something like:

```text
clang --target=x86_64-w64-windows-gnu -fuse-ld=lld app.obj modules... -o app.exe ...
```

Pros:

- can be made truly standalone
- no Visual Studio required
- good fit for open-source distribution
- LLVM LTO can work through bundled `clang/lld`

Cons:

- ABI is MinGW/GNU-style Windows ABI, not MSVC ABI
- C library behavior differs from MSVC builds in some corners

**Option B: MSVC ABI + lld-link**

This targets the normal Visual Studio ABI:

```text
lld-link main.obj modules... /out:app.exe ...
```

Pros:

- best compatibility with MSVC-built libraries
- native Windows/COFF ecosystem

Cons:

- you also need MSVC CRT libs and Windows SDK import libs
- redistribution/licensing gets more delicate
- harder to make “download DQ and go” without requiring Build Tools or SDK

For a self-contained DQ package, I’d start with **MinGW-w64 + LLVM**. It is the friendlier path.

For the DQ compiler architecture, I’d avoid hardcoding `gcc` / `clang` / `lld` directly in the middle of compilation. Add a small linker-driver abstraction:

```text
ELinkMode:
  native_gcc
  native_clang
  windows_mingw_clang_lld
  windows_msvc_lld_link
  llvm_lto_clang_lld
```

Then a Windows package can ship a default config:

```text
link.driver = bundled-clang
link.target = x86_64-w64-windows-gnu
link.lto = off/full/thin
tool.clang = <dq-install>/bin/clang.exe
tool.lld = <dq-install>/bin/ld.lld.exe
lib.path = <dq-install>/lib/windows-x86_64
```

For LTO, bundled LLVM is actually a win: you control the LLVM version used to produce and consume bitcode. That matters because LLVM bitcode compatibility across versions is not something I’d want to depend on casually.

So my recommendation:

1. Keep Linux as-is for now.
2. Add configurable link driver support.
3. Make the first Windows package use bundled `clang + lld + MinGW-w64 runtime/import libs`.
4. Later add an MSVC-ABI package if you need compatibility with MSVC libraries.
