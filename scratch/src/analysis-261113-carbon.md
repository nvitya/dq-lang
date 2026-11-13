# DQ Compiler (`compiler/`) C++ Codebase Analysis

**Scope analyzed:** 110 files (`56,113` lines of C++23 across `src/`, `parser/`, `ast/`, `types/`, `codegen/`, `builtins/`, `langserver/`, `utils/`, `tools/`, `test/`).

Overall, the compiler has a clean, pragmatic design and recent refactorings (such as unifying `AnalyzeConversion` and centralizing `OStmt`/`OExpr` tree ownership) have noticeably improved consistency. However, rapid feature growth has left several **high-risk logic duplications** (some already diverging into real bugs) and **structural bottlenecks** that will hurt reliability and both human and AI maintainability if not consolidated.

---

## 1. Problematic Code & Logic Duplications

### A. Duplications That Have Already Diverged Into Bugs

1. **In-body vs. Out-of-line Method Declarations (`ReadCompoundMethod` vs. `ParseQualifiedObjectFunction`)**
   - **Locations:** `compiler/parser/dqc_parser.cpp:2138` vs. `compiler/parser/dqc_parser.cpp:2664`
   - **Issue:** Both parse `func [*]<Method>(...)`, validate intrinsic names, inject `__this`, and call `FinishFunctionDecl`. Because they are duplicated, `ParseQualifiedObjectFunction` (`func MyType.Method`) **omits**:
     - Signature validation for `*Create` (no return type) and `*Destroy` (no params/return type),
     - Rejecting `*Create` / `*Destroy` on `struct` types,
     - Registering `object_type->constructors` / `object_type->destructor` when not previously forward-declared.

2. **Six Copies of RTL Function Lookup/Call Helpers — `CallEmbStrFunc` Bypasses Exception Unwinding**
   - **Locations:** `DynArrayFunc`/`CallDynArrayFunc` (`types/otype_array.cpp:55`), `OTypeArraySlice::CallRuntimeFunc` (`types/otype_array.cpp:784`), `DynStrFunc`/`CallDynStrFunc` & `TextFormatFunc`/`CallTextFormatFunc` (`types/otype_string.cpp:119, 169`), `AnyValueFunc`/`CallAnyValueFunc` (`types/otype_anyvalue.cpp:59`), `EnumRtlFunc` (`types/otype_enum.cpp:27`), and `EmbStrFunc`/`CallEmbStrFunc` (`types/otype_embstr.cpp:79, 107`).
   - **Issue:** Nearly identical `g_namespaces` lookup + `OValSymFunc`/overload extraction + error throwing is repeated 6 times. Every other caller takes `OScope *` and emits `scope->GenerateCallOrInvoke(...)`, whereas `CallEmbStrFunc` emits raw `ll_builder.CreateCall(...)`—meaning runtime errors/exceptions thrown inside `embstr` RTL helpers fail to unwind active `try`/`finally` or local destructors.

3. **Object Lifecycle Lowering Duplication (`CreateCall` vs. `GenerateCallOrInvoke`)**
   - **Constructor calls:** `EmitCtorCallWithCleanup` (`ast/expressions.cpp:1766`) and `OVsObject::GenerateConstructorCall` (`types/otype_compound.cpp:845`) are line-for-line duplicates, except `OVsObject::GenerateConstructorCall` calls raw `ll_builder.CreateCall` *even after setting up `OCallLifetimeCleanup::Prepare()`*, leaving the landing pad unreachable and bypassing `try`/scope unwinding if a stack/field constructor throws.
   - **Polymorphic destructor + `MemFree`:** `OTypeAutoFree::GenerateCleanup` (`ast/symbols.cpp:286`) and `OStmtDelete::Generate` (`ast/statements.cpp:504`) both manually walk to the root object, load vtable slot `1`, call the destructor, and call `MemFree` (and both duplicate `MemFree` lookup in `curscope` + `g_namespaces["sys"]` with `ParseStmtDelete` at `parser/dqc_parser_stmt.cpp:1371`). Both use raw `CreateCall` instead of `GenerateCallOrInvoke`.

4. **CLI Option Synchronization & Broken `dq-run` Argument Parsing**
   - **Locations:** `src/comp_options.cpp:526` (`ProcessCommandLineOpts`), `src/comp_options.cpp:832` (`CommandLineOptionHasValue`), `src/projectfile.cpp:440` (`ParseProperty`), `ast/module_intf.cpp:488` (`DqmIfBuildOptions`), `ast/module_intf.cpp:1124` (`ModuleChildArgs`), `langserver/lang_server.cpp:593` (`RunWorker`), and `tools/main_dq_run.cpp:47` (`NeedsCompilerValue`).
   - **Issue:** Every compiler flag is manually maintained in up to **6 places**. In `tools/main_dq_run.cpp:47`, `NeedsCompilerValue` only checks `"-o" == arg` instead of sharing `OCompOptions::CommandLineOptionHasValue`. Running `dq-run --target x86_64-linux app.dq` or `dq-run --pkg_path ./pkgs app.dq` treats `x86_64-linux` / `./pkgs` as the `.dq` source file and `app.dq` as a program runtime argument.

---

### B. High-Maintenance Logic Duplications

5. **Overload & Constructor Candidate Scoring**
   - `OTypeObject::FindConstructorForArgs` (`types/otype_compound.cpp:100–236`) reimplements the entire conversion-cost, ref-mode binding, default-arg, and varargs scoring algorithm of `OTypeFunc::AnalyzeCallCandidate` (`types/otype_func.cpp:365–451`) plus the best-candidate/ambiguity loop of `ParseExprOverloadCallWithRawArgs` (`parser/dqc_parser_expr.cpp:4580–4645`), solely to offset `hidden_this_count = 1`.

6. **Four Separate Method/Function Signature Comparators**
   - `OTypeFunc::MatchesSignature` (`types/otype_func.cpp:250`), `OValSymFunc::UserSignaturesMatch` (`types/otype_func.cpp:1480`), `OTypeFuncRef::CanAcceptMethod` (`types/otype_func.cpp:1688`), and `matches_method_signature` in `OTypeObject::FindVirtualBaseMethod` (`types/otype_compound.cpp:369`) all compare parameter lists/modes/types (skipping `__this`), with subtle differences (`UserSignaturesMatch` ignores return types and rejects varargs outright).
   - Similarly, `OValSymFunc::SignatureText` (`types/otype_func.cpp:1395`) duplicates `OTypeFunc::FormattedTypeName` (`types/otype_func.cpp:1490`).

7. **Module Interface (`.dqm_if`) Top-Level vs. Embedded Readers**
   - `OModuleIntf::ReadEmbeddedModule` (`ast/module_intf.cpp:3257–3386`) and `OModuleIntf::ReadDqmIfRecords` (`ast/module_intf.cpp:3502–3669`) duplicate ~140 lines of record-tag dispatch (`DQMIF_TYPE_BEGIN`, `DQMIF_ENUM_BEGIN`, `DQMIF_CONST_BEGIN`, `DQMIF_VAR_BEGIN`, `DQMIF_FUNC_BEGIN`, `DQMIF_STRUCT/OBJ/UNION_BEGIN`, forward tags, and `SkipGroup` checks). Adding a new declaration kind requires lockstep edits in both.

8. **Assignment & Modify-Assign Validation (`FinalizeStmtAssign`) and Codegen**
   - In `ODqCompAst::FinalizeStmtAssign` (`ast/dqc_ast.cpp:1744–1936`), the `OPropertyExpr` branch and the general `OLValueExpr` branch duplicate pointer-arithmetic checks (`+=`/`-=`), `str +=` byte-wchar literal coercion + `EnsureDynStringRtlUse()`, and `CheckAssignType`.
   - In codegen, `OBinExpr::Generate` (`ast/expressions.cpp:1132–1190`) and `GenerateModifyAssignValue` (`ast/statements.cpp:423–480`) duplicate the entire pointer/float/signed-and-unsigned-int LLVM IR operator switch.

9. **RTTI, Virtual Slot Lookup, LandingPads, and C-Varargs Lowering**
   - **RTTI loop:** `ODqCompCodegen::GenerateExceptBranchMatch` (`codegen/dqc_codegen.cpp:129`) duplicates the TypeInfo-parent PHI loop of `OTypeObject::GenerateInstanceOf` (`types/otype_compound.cpp:697`).
   - **Virtual slot GEP (`slot + 2`):** Duplicated between `GenerateFunctionCall` (`ast/expressions.cpp:2798`) and `BuildObjectFuncRefValue` (`ast/expressions.cpp:2938`).
   - **LandingPad & `exn.storage` creation:** Triplicated across `OCallLifetimeCleanup::GenerateExceptionCleanup` (`ast/expressions.cpp:1985`), `OStmtBlock::Generate` (`ast/statements.cpp:102`), and `OStmtTry::Generate` (`ast/statements.cpp:808`).
   - **C varargs default promotions (`float -> double`, `<i32 -> sext/zext i32`):** Copy-pasted in `OCallExpr::Generate` (`ast/expressions.cpp:2308`) and `OIndirectCallExpr::Generate` (`ast/expressions.cpp:2598`).
   - **`OBreakStmt` vs. `OContinueStmt`:** Identical in `ast/statements.cpp:663–695` (`OContinueStmt` still has the copy-pasted `"BreakStmt::Generate(): empty loop_stack!"` message).

10. **Built-in Type Method Parsers & `raise` Shorthand**
    - `ParseDynArrayMethod`, `ParseEmbStrMethod`, `ParseStringMethod`, and `ParseAnyValueMethod` (`parser/dqc_parser_expr.cpp:1820–2720`) span ~900 lines of repetitive `if ("Method" == membername)` chains, manual `check_count` lambdas, and `array_index_context_*` save/restore boilerplate instead of table-driven method signatures.
    - `parse_raise_shorthand` (`parser/dqc_parser_stmt.cpp:2356–2416`) duplicates `MemAlloc` lookup, abstract-object rejection, raw call arg parsing, and `CheckObjectCtorArgs` from `ParseNewExpr` (`parser/dqc_parser_expr.cpp:4680`).

11. **Dual Lexers (`OScFeederBase` vs. `TStrParseObj`) & Diagnostic Overload Boilerplate**
    - `parser/scf_base.cpp` and `utils/strparse.cpp` maintain two separate C-style `char*` scanners with duplicated identifier, number, hex, `ReadTo`, `SearchPattern`, and escape-decoding `ReadQuotedString` implementations.
    - `ODqCompBase` (`src/dqc_base.h:57–102`) and `OScFeederDq::PreprocError2` (`parser/scf_dq.h:83–86`) declare 28 manual 0..3-parameter overloads instead of default parameters or variadic/pack forwarding, and substitute `$1..$3` sequentially in `FormatDiagMsg` (`src/dqc_base.cpp:95` — if `par1` contains `"$2"`, the next pass corrupts it!).

---

## 2. Long-Term Maintenance & Reliability Risks (Humans & AI)

### 1. Process-Global State & Hidden `static` Caches
- **Evidence:** `g_opt`, `g_compiler`, `g_module`, `g_builtins`, `g_defines`, `g_namespaces`, `g_source_overlay`, `ll_ctx`, `ll_builder`, `ll_module`, `ll_loop_stack`, plus `#define TARGET_PTRSIZE (g_opt.target.pointer_size)` (`src/comp_config.h:62`).
- **Hidden `static` hazards:**
  - `OTypePointer::GetNullPtrType()` (`ast/symbols.h:551`) uses a function-local `static OTypePointer instance(...)`, permanently latching `TARGET_PTRSIZE` from its first invocation.
  - `OModulePath::SourceIdentity`, `IsDirectory`, and `FindPackage` (`ast/module_path.cpp:74, 100, 205`) store unbounded `static unordered_map` filesystem caches that can never be invalidated in-process.
- **Impact:** Prevents running multiple compilations or multi-target tests in-process, forces both module builds and `ODqLanguageServer` (`langserver/lang_server.cpp`) to spawn fresh OS subprocesses on every analysis, and creates implicit order-of-initialization dependencies (`main_dq_comp.cpp:265–276`) that are error-prone for AI-assisted edits.

### 2. Deep Linear "God Class" Inheritance & Oversized Files
- **Evidence:** `ODqCompBase -> ODqCompAst -> ODqCompParserExpr -> ODqCompParserStmt -> ODqCompParser -> ODqCompCodegen -> ODqCompClargs -> ODqCompiler`, paired with downcasts/globals (`ast/` and `types/` calling `g_compiler->...`).
- **Hotspot file sizes:** `dqc_parser_expr.cpp` (**5,805** LOC), `expressions.cpp` (**3,816** LOC), `module_intf.cpp` (**3,721** LOC), `dqc_parser.cpp` (**2,942** LOC), `dqc_parser_stmt.cpp` (**2,497** LOC), `otype_func.cpp` (**2,004** LOC).
- **Impact:** Parser state (`suppressed_left_expr_diags`, `array_index_context_*`, `loop_depth`, `curblock`, `curscope`) is mutated across 4 inheritance layers with manual save/restore variables instead of RAII guards. Functions like `ParseStmtFor` (330 LOC) and `ParsePostfixDotMember` (300 LOC) interleave lexing, scope lookup, type checking, diagnostic suppression, and AST lowering, making local reasoning difficult for both humans and LLMs.

### 3. Cross-Module Linker Collisions & 32-Bit Target Bugs in Codegen
- **Unqualified RTTI/VTable symbol names:** `OTypeObject::GenVTableGlobal` (`types/otype_compound.cpp:538, 545, 589, 627`) emits `_DQTI_<Name>` and `_DQVT_<Name>` with `ExternalLinkage` **without** module qualification (`OModuleIntf::LinkerSymbolNameForModule`). Any two modules defining a public `object` with the same name (e.g., `Stream` or `Error`) will collide at link time.
- **Hardcoded 64-bit ABI in native throw:** `ODqCompCodegen::GenerateDqExcNativeThrowBody` (`codegen/dqc_codegen.cpp:228–232`) declares `__cxa_allocate_exception(i64)` and passes `8` instead of `TARGET_PTRSIZE` / native `size_t`, which mismatches 32-bit targets (`x86`, `armhf`, `arm_m*`, `rv32`, `wasm32`).

### 4. Compile-Time vs. Runtime Semantic Divergence in Constant Folding
- **Location:** `OValueBool::CalculateConstant` (`types/otype_bool.cpp:82–131`).
- **Issues:**
  1. Integer comparisons construct `OValueInt(g_builtins->type_int, 0)` instead of `ex->left->ResolvedType()`. On 32-bit targets (`type_int == int32`), compile-time comparisons of `int64` constants silently truncate to 32 bits!
  2. Unsigned integer comparisons (`uint64`, `uint32`, etc.) compare `vleft.value < vright.value` as signed `int64_t`, so `uint64(0xFFFFFFFFFFFFFFFF) > 0` folds to `false` at compile time vs. `true` at runtime.
  3. `OLogicalExpr` (`and` / `or`) evaluates both operands eagerly in `CalculateConstant`, unlike runtime short-circuiting.

### 5. Shadowed Base Members, Raw Pointer Leaks, and 6-File Attribute Plumbing
- **Member shadowing hazard:** `OModule` (`ast/dq_module.h:76`) declares `vector<ODecl *> declarations;`, **shadowing** `OModuleBase::declarations` (`vector<OIntfDecl *>`, `ast/symbols.h:228`) in its own base class (`dq_module.cpp` has to disambiguate via `OModuleBase::declarations`).
- **Memory ownership gaps:**
  - `OTypeObjectTypeRef` (`expressions.cpp:2028`, `dqc_parser_expr.cpp:748`, `module_intf.cpp:2006`) and `OTypeFuncRef` (`expressions.cpp:2988`, `dqc_parser_expr.cpp:766`) are allocated via `new` on every occurrence instead of cached on `OType` (like `GetPointerType()`/`GetSliceType()`), leaking across AST rewrites.
  - `OModule::~OModule()` (`ast/dq_module.cpp:32`) never deletes `OModule::declarations` (`ODecl*`) or module-owned `OValSym*`/`OType*` instances, and `main()` never deletes `g_module`, `g_defines`, `g_builtins`, or `ll_module`.
- **Attribute & type-property sprawl:** Every `OValSym` carries ~20 individual `attr_*` booleans (`ast/symbols.h:596–619`), and adding one attribute requires synchronized edits in 6 files (`attributes.h/cpp`, `dqc_parser_expr.cpp`, `symbols.h/cpp`, `dqm_if.h`, `module_intf.cpp`). Similarly, "is type default-initialized without `=`?" is checked via manual `ETypeKind` lists in `ParseStmtVar` (`dqc_parser_stmt.cpp:542`) vs. `OStmtVarDecl::Generate` (`statements.cpp:198`) rather than a virtual method on `OType`.

### 6. Fragile `.dqm_if` Checksum, Sentinel Strings, and LSP Text-Scraping
- **Weak checksum:** `ODqmIfWriter::Checksum` / `ODqmIfReader::Checksum` (`ast/dqm_if.cpp:153, 220`, also duplicated) is a plain byte sum (`for (uint8_t b : adata) result += b;`), which cannot detect byte swaps, reordered records, or zero-run corruption (easy to replace with a 10-line FNV-1a 64-bit hash).
- **Magic sentinel module names:** `OModuleIntf::ReadDqmIfHeaderMetadata` (`ast/module_intf.cpp:691`) branches on magic `name` strings `"dqm_if_dump"` and `"regen_check"` instead of explicit reader options.
- **Regex / substring scraping in LSP:** `InferVariableType` (`langserver/lang_server.cpp:365`) runs a `std::regex` over raw document text, and `SymbolPosition` (`langserver/semantic_result.cpp:104`) searches raw source text for `"func " + name` to find forward-declared bodies instead of recording the implementation `OScPosition` on `OValSymFunc` when `ResolveForwardDecl` runs.

---

## 3. Recommended Refactoring Priorities

1. **Fix immediate correctness drifts first:**
   - Qualify `_DQTI_` / `_DQVT_` globals with `OModuleIntf::LinkerSymbolNameForModule`.
   - Fix `OValueBool::CalculateConstant` (preserve operand integer width & signedness, short-circuit `and`/`or`) and `GenerateDqExcNativeThrowBody` (`TARGET_PTRSIZE`).
   - Route `CallEmbStrFunc`, `OVsObject::GenerateConstructorCall`/`GenerateDestructorCall`, `OTypeAutoFree::GenerateCleanup`, and `OStmtDelete::Generate` through a shared `OScope::GenerateCallOrInvoke`-aware helper.
   - Reuse `OCompOptions::CommandLineOptionHasValue` in `main_dq_run.cpp`.
2. **Consolidate high-churn duplications onto owning objects:**
   - Unify RTL lookup/call into one helper on `OModule`/`ODqCompCodegen`; unify method signature comparison/formatting on `OTypeFunc` (with an optional `skip_first_param` flag) and reuse `AnalyzeCallCandidate` for constructors; share one `ReadDqmIfDeclarationRecord` helper between `ReadDqmIfRecords` and `ReadEmbeddedModule`; unify `ReadCompoundMethod` and `ParseQualifiedObjectFunction`.
3. **Shrink cognitive & context surface for humans and AI:**
   - Rename `OModule::declarations` (e.g. `module_decls`) vs. `OModuleBase::declarations` (`intf_decls`), cache `OTypeObjectTypeRef` on `OTypeObject`, replace manual parser flag save/restores with small RAII scope guards, and convert the 4 built-in pseudo-method parsers + CLI/build-identity options into declarative tables.
