# DQ Compiler C++ Code Analysis (`compiler/`)

Date: 2026-11-13
Scope: `/workpr/dq-lang/compiler` (109 `.cpp`/`.h` files, 56,003 lines)

---

## 1. High-Level Architectural Profile

The compiler has broad language and target feature coverage, but several structural patterns make long-term maintenance and safe edits difficult for both humans and AI tools:

- **7-level linear inheritance tower:** `ODqCompiler` inherits linearly through `ODqCompClargs` -> `ODqCompCodegen` -> `ODqCompParser` -> `ODqCompParserStmt` -> `ODqCompParserExpr` -> `ODqCompAst` -> `ODqCompBase` (`compiler/src/dqc.h:29-56`, `compiler/codegen/dqc_codegen.h:32-70`, `compiler/parser/dqc_parser.h:32-104`, `compiler/ast/dqc_ast.h:45-188`). Every compilation phase operates on a single shared `g_compiler` instance.
- **~15 process-wide mutable global singletons:** `g_opt`, `g_compiler`, `g_module`, `g_builtins`, `g_defines`, `g_namespaces`, `g_source_overlay`, plus `ll_ctx`, `ll_builder`, `ll_module`, `di_builder`, `di_unit`, `di_main_file`, `ll_loop_stack`, and `di_scope_stack` (`compiler/codegen/ll_defs.h:51-71`, `compiler/ast/dq_module.h:126`, `compiler/builtins/named_scopes.h:23`). This prevents in-process reuse and isolated unit testing (forcing `langserver` to spawn a child `dq-comp` process per edit).
- **Manual raw memory + RTTI heavy:** Across `compiler/`, `new` appears 422 times and `delete` 391 times vs. 7 uses of `unique_ptr`/`shared_ptr`, alongside 493 `dynamic_cast` occurrences.
- **No compiler warnings enabled in CMake:** `compiler/CMakeLists.txt:31` sets `-g` without `-Wall`/`-Wextra`. Running all 79 translation units from `build/compile_commands.json` through `g++ -fsyntax-only -Wall -Wextra -Wno-unused-parameter` produces **35 warnings** (`16 -Wswitch`, `11 -Wreorder`, `5 -Wunused-variable`, `1 -Wsign-compare`, `1 -Wunused-but-set-variable`, `1 -Wmissing-field-initializers`), including real latent bugs below.

---

## 2. Concrete Bugs & Immediate Reliability Hazards

### Parser & Lexer

1. **Negative sign silently dropped in integer parsing (`-Wunused-but-set-variable`):**
   `OScFeederBase::ReadInt64Value` (`compiler/parser/scf_base.cpp:557-600`) sets `signmul = -1` on `'-'` (line 576) but returns `rvalue = result;` (line 596) without multiplying by `signmul`. Callers such as `ParseAttrIntArg` (`compiler/parser/dqc_parser_expr.cpp:184-214`) silently convert `-4` into `+4` and bypass `positive_only` validation.
2. **Braces-mode `if`/`elif`/`else` control-flow bug:**
   In `ODqCompParserStmt::ParseStmtIf` (`compiler/parser/dqc_parser_stmt.cpp:2132-2194`), when `"}" == endstr` is followed by `endif`, `elif`, or `else`, lines `2178`, `2183`, and `2188` assign `endstr` and `continue` to the top of the `while` loop—where line `2134` resets `string endstr = ""` and calls `ReadStatementBlock` again on the *previous* branch instead of closing the `if` or parsing the next branch.
3. **Preprocessor directive error recovery & condition leaks:**
   - `OScFeederDq::PreprocError2` (`compiler/parser/scf_dq.cpp:848-854`) calls `if (ReadToChar('{')) CheckSymbol("}");`—searching for `'{'` instead of `'}'`.
   - `OScFeederDq::ParseDirective` (`compiler/parser/scf_dq.cpp:581`) assigns `preproc_closer_brace = "}";` (converting `const char *` to `bool`).
   - `StartConditionalBranch` (`compiler/parser/scf_dq.cpp:880-883`) allocates `new OScfCondition(...)`, and `#endif` (`compiler/parser/scf_dq.cpp:1041-1042`) pops `curcond = curcond->parent;` without `delete`.
4. **Scanner bounds & float exponent bugs:**
   - `OScFeederBase::SearchPattern` (`compiler/parser/scf_base.cpp:396`) uses `while (cps < bufend - csslen)` (UB when `bufend - cps < csslen` or `bufend == nullptr`, and misses matches ending right at `bufend`).
   - `OScFeederBase::ReadFloatFracExp` (`compiler/parser/scf_base.cpp:950-995`) dereferences `*curp` without checking `curp < bufend`, rejects `'+'` in exponents (`1.0e+5`), and accepts `'-'` mid-digits (`1.0e5-2`).
5. **Prefix keyword matching:**
   `CheckSymbol` (unlike `CheckKeyword`) does not check identifier boundaries and is used for keywords like `"true"`, `"false"`, `"nil"`, `"else"`, `"endif"`, `"func"`, and `"union"`, risking misparses on identifiers such as `true_flag` or `elsewhere`.

### AST, Types & Codegen

6. **Cross-function invalid LLVM IR in `OTypeEmbStr`:**
   `OTypeEmbStr::GenerateDescriptor` (`compiler/types/otype_embstr.cpp:190-223`) caches `embstraddr -> CreateEntryBlockAlloca(...)` inside `descriptor_caches` on persistent `OTypeEmbStr` type instances. Accessing a global `embstr(N)` from two functions causes the second function to reference an `alloca` instruction from the first function's entry block.
7. **Signedness mismatch in LLVM integer conversion:**
   - `ToNativeInt` (`compiler/codegen/ll_defs.cpp:60-82`) uses `CreateZExt` unconditionally for `< 64`-bit integers instead of `CreateSExt`.
   - `OTypeDynArray::GenerateElementAddress` (`compiler/types/otype_array.cpp:728-745`) calls `ToNativeUInt(index)` (zero-extending small ints) and then executes signed `ICmpSGE`/`ICmpSLT`.
8. **Constant-folding `SIGFPE` / C++ UB:**
   `OValueInt::CalculateConstant` (`compiler/types/otype_int.cpp:219-253`) performs `vleft.value / vright.value` and `%` without checking for `0` or `INT64_MIN / -1`, performs unchecked shifts (`<<`, `>>` with signed right shift on unsigned types), and triggers signed overflow UB on `+`, `-`, `*`.
9. **Unchecked null pointer dereferences & AST leaks:**
   - `OModule::TypeDeclared` (`compiler/ast/dq_module.cpp:492-508`) and `OModule::ValSymDeclared` (`compiler/ast/dq_module.cpp:510-526`) dereference `*rtype` / `*rvalsym` unconditionally even though `compiler/ast/dq_module.h:122-123` defaults both to `nullptr`.
   - `OModule::~OModule` (`compiler/ast/dq_module.cpp:36-48`) never deletes `declarations`.
   - `ODqCompAst::CreateBinExpr` (`compiler/ast/dqc_ast.cpp:554-625`) defines `FreeLeftRight` but returns `nullptr` on error paths without calling it (and dereferences `left->ptype->kind` without null-checking `ptype`).
   - `OStmt::Generate` (`compiler/ast/statements.h:43-46`) logs `typeid(this).name()` (pointer type `"P5OStmt"` instead of `typeid(*this)`), and `OStmtTry::Generate` (`compiler/ast/statements.cpp:966-980`) emits branches after `bb_after_handlers` without checking insert-block terminators when all branches return/raise.
10. **Missing exception-unwind cleanup & constructor side effects:**
    - `ODynamicNewObjectExpr::Generate` (`compiler/ast/expressions.cpp:2203-2220`) calls constructor arguments and the constructor without `OCallLifetimeCleanup`, leaking temporary strings/dynarrays.
    - `OLValueVar::OLValueVar` (`compiler/ast/expressions.cpp:213-218`) mutates `g_compiler->curvsfunc->AddCapture(avalsym)` inside its constructor, so cloning or constructing temporary `OLValueVar`s during codegen silently alters captures.

### Driver, LSP, Process & Locking

11. **Unsafe crash/signal handling & 8-bit exit code wrap:**
    - `signal_handler` (`compiler/src/main_dq_comp.cpp:130-142`) calls `cerr`, `std::stacktrace::current()`, `cout`, and `exit(signal)` inside a `SIGSEGV` handler (all async-signal-unsafe).
    - `main` (`compiler/src/main_dq_comp.cpp:320-335`) returns `g_compiler->errorcnt` directly (`return r;`); on POSIX, 256 errors wraps to exit status `0` (success).
12. **LSP crash & hang vectors:**
    - `ODqLanguageServer::Run` (`compiler/langserver/lang_server.cpp:408-431`) has no `try/catch` around `Handle()` or `FlushPendingAnalysis()`; any `TJsonNode` or compiler exception hits `set_terminate` -> `abort()`.
    - `ODqLanguageServer::RunWorker` (`compiler/langserver/lang_server.cpp:541-563`) leaves `exec_timeout_ms = -1`, so a stalled worker hangs the single-threaded LSP forever.
13. **Artifact lock & JSON parser hazards:**
    - `OArtifactLock::Unlock` (`compiler/utils/artifact_lock.cpp:153-173`) unlinks the lock file before `flock(fd, LOCK_UN)` on exclusive locks, and attempts a non-atomic `LOCK_EX | LOCK_NB` upgrade on shared locks; the Windows path papers over `FILE_FLAG_DELETE_ON_CLOSE` `ERROR_ACCESS_DENIED` races with a fixed 2-second retry loop (`compiler/utils/artifact_lock.cpp:76-81`).
    - `ParseJsonString` (`compiler/utils/jsontools.cpp:114-140`) can advance past `'\0'` on a trailing backslash `"abc\` and lacks EOF termination checks; `TJsonNode` parsing uses unbounded recursion.

---

## 3. Major Code & Logic Duplications

| Area | Duplicated Pattern | Locations & Drift Impact |
| :--- | :--- | :--- |
| **CLI options (6x)** | Option parsing, space-separated value lists, and child-arg reconstruction | `compiler/src/comp_options.cpp:512-835`, `compiler/src/main_dq_comp.cpp:166-194`, `compiler/tools/main_dq_run.cpp:47-128`, `compiler/ast/module_intf.cpp`, `compiler/langserver/lang_server.cpp:541-563`. **Already drifted:** `dq-run` only knows `"-o"` (`--target arm64-linux foo.dq` misparses), and LSP workers omit `--cpu-features`, `--exceptions`, `--dynstrings`, `--sysroot`, and `--lto`. |
| **Built-in method parsers (4x)** | `(` check, manual `array_index_context_*` save/restore, `free_and_fail`/`check_count` lambdas, post-dispatch loops | `compiler/parser/dqc_parser_expr.cpp:1820-2712` (`ParseDynArrayMethod`, `ParseEmbStrMethod`, `ParseStringMethod`, `ParseAnyValueMethod`), plus duplicated index/slice parsing in `ParsePostfixIndexOrSlice` vs `.wchar[...]`. |
| **AST `FoldChildren` (19x)** | `OExpr * tmp = m; OExpr::FoldTree(&tmp); m = static_cast<OLValueExpr *>(tmp);` | Repeated across 19 `OLValueExpr*` child-holding nodes in `compiler/ast/expressions.cpp`. |
| **LLVM int cast & type helpers (5x)** | `getType() == dst` / `getIntegerBitWidth()` / `Ext` / `Trunc` and `LlPtrType`/`LlU32`/`LlNativeInt` | `compiler/codegen/ll_defs.cpp:60-106` (`ToNativeInt`, `ToCharValue`), plus file-local `ToNativeUInt`/`ToU32` copies in `otype_anyvalue.cpp`, `otype_array.cpp`, `otype_string.cpp`, `otype_embstr.cpp`. |
| **RTL lookup/call wrappers (7x)** | `g_namespaces` lookup -> `dynamic_cast<OValSymFunc*>` -> `throw runtime_error` -> call | Duplicated in `otype_anyvalue.cpp`, `otype_embstr.cpp`, `otype_string.cpp` (2x), `otype_array.cpp` (2x), `otype_enum.cpp`. `EmbStrFunc` diverged by skipping `GenerateCallOrInvoke`. |
| **EH, varargs & call cleanup** | LandingPad + `ll_exn_storage` (3x), `OCallLifetimeCleanup` orchestration (6x), C varargs promotion (2x), vtable slot dispatch (2x) | `compiler/ast/expressions.cpp:2756-2778`, `compiler/ast/expressions.cpp:3090-3109`, `compiler/ast/statements.cpp`, `compiler/ast/symbols.cpp`. |
| **JSON & ProcessRunner** | `JsonEscape` vs `AppendEncodedJsonString`; Windows/POSIX `Run` vs `RunInteractiveProcess` | `compiler/utils/dq_utils.cpp` vs `compiler/utils/jsontools.cpp`; `compiler/utils/processrunner.cpp` duplicates `BuildCmdLine`, ctor/dtor, timing, and child-spawn plumbing. |
| **DQMIF & compound parsing** | `ReadEmbeddedModule` vs `ReadDqmIfRecords`; loaded-module lookup (4x); struct/union/object member loops (3x) | `compiler/ast/module_intf.cpp`, `compiler/parser/dqc_parser.cpp` (`ParseStructDecl`, `ParseUnionMembers`, `ParseObjectDecl`). |

---

## 4. Long-Term Maintainability Bottlenecks (Humans & AI)

1. **Oversized multi-concern files & functions:**
   - **Files >1,500 lines:** `dqc_parser_expr.cpp` (5,806), `expressions.cpp` (3,815), `module_intf.cpp` (3,720), `dqc_parser.cpp` (2,941), `dqc_parser_stmt.cpp` (2,495), `otype_func.cpp` (2,004), `otype_compound.cpp` (1,797). For example, `compiler/ast/module_intf.h:52-169` / `.cpp` mixes binary `.dqm_if` serialization, topological re-export flattening, build-cache staleness, child process launching + JSONL parsing, and text dumping.
   - **Functions >300 lines:** `ParseExprPrimary` (726 lines in raw brace count), `ParseStmtFor` (494), `ParsePostfixDotMember` (408), `ParseTypeSpec` (360), `ParseStringMethod` (338), `ParseFunctionSignature` (338).
   - **`compiler/src/version.h` (1,176 lines):** Only line 14 is `#define DQ_COMPILER_VERSION`; lines 16–1176 are a changelog comment included into `main_dq_comp.cpp` and `dqc.cpp`.
2. **Error-handling fragmentation (125 `throw`s vs. `Error()`):**
   - `ast/`, `types/`, and `codegen/` throw `std::logic_error` (79 sites) and `std::runtime_error` (43 sites) for a mix of internal invariants and user-triggerable conditions (e.g., recursive layout, invalid casts), while `main()` has no `try/catch` (uncaught throws go to `my_crash_handler()` -> `abort()`).
   - Meanwhile `ODqCompBase` declares 24 individual 0–3-parameter overloads for `Error`/`ErrorTxt`/`Warning`/`Hint` (`compiler/src/dqc_base.h:59-100`).
3. **Manual flag save/restore instead of RAII:**
   While `ODqCompBaseSuppressWarningsScope` (`compiler/src/dqc_base.h:104-116`) uses RAII, `array_index_context_*`, `supress_varinit_check`, `suppress_access_read_check`, `curscope`, `curblock`, and `loop_depth` are manually saved/restored across branches and early returns (and leak across speculative rewinds).
4. **Header pollution & dead code in the build:**
   - **30 `.h` files** contain top-level `using namespace std;` (e.g., `dqc.h:24`, `ll_defs.h:25`, `symbols.h:30`).
   - `src_test/` (`compiler/src_test/test_symbols.cpp`) is dead code gated by `#define CALL_TESTCODE 0` in `compiler/src/main_dq_comp.cpp:52-56`, yet compiled and linked into `dq-comp` via `file(GLOB ...)`.
   - `types/` calls back up into `parser/` (`EnsureDynStringRtlUse`), creating cross-layer coupling.

---

## 5. Prioritized Improvement Roadmap

1. **Immediate correctness & safety fixes (high ROI, low blast radius) — SOLVED in compiler `v0.96.7`:**
   - **Solved:** Fixed `ReadInt64Value` (`signmul` applied via `uint64_t`), `ParseStmtIf` braces-mode `"}"` handling & `elif` null guard + `ReadStatementBlock` keyword matching, `PreprocError2` (`ReadToChar('}')`) & `OScFeederDq::Reset` / `preproc_closer_brace` / `#endif` condition cleanup, `ReadFloatFracExp` bounds/exponent signs, `SearchPattern` & `CheckKeyword` bounds, `PrevStr`/`Reset` init, and `OModule::TypeDeclared`/`ValSymDeclared` null guards.
   - **Solved:** Fixed `OTypeEmbStr::descriptor_caches` (keyed by `(LlFunction*, LlValue*)`) + `ESMF_PCHAR`, unified integer resizing in `ll_defs.{h,cpp}` (`ResizeIntValue`, signed `ToNativeInt`, type-aware `ToNativeInt(val, srctype)`, `ToNativeUInt`, `ToCharValue`) updating `OTypeDynArray`, `OTypeString`, `OTypeAnyValue`, `OTypeEmbStr`, `OLValueIndex`, `OArraySliceExpr`, `OArrayIndexOfExpr`, and hardened `OValueInt::CalculateConstant` (div-by-zero, `INT64_MIN/-1`, shift bounds/signedness, overflow UB, non-finite/out-of-range float casts).
   - **Solved:** Wrapped `ODynamicNewObjectExpr::Generate` in `OCallLifetimeCleanup` via shared `EmitCtorCallWithCleanup`, moved `AddCapture` out of `OLValueVar` ctor into `ODqCompAst::CreateVarExpr`, clamped `main()` exit code to `(r > 0 ? 1 : 0)`, made `SIGSEGV` handler async-signal-safe, and added `try/catch` + `exec_timeout_ms = 30000` in `ODqLanguageServer`.
   - **Solved:** Enabled `-Wall -Wextra -Wno-unused-parameter` in `compiler/CMakeLists.txt` (fixing all 35 compiler warnings cleanly), excluded `src_test/` from `dq-comp`, extracted the changelog to `/workpr/dq-lang/CHANGELOG.md`, bumped `DQ_COMPILER_VERSION` to `"0.96.7"`, and added regression autotests `autotest/tests/basic/safety_regressions.dq` and `autotest/tests/basic/safety_consterr_typecheck.dq`.
2. **High-leverage deduplication:**
   - Unify CLI option metadata in `OCompOptions` so `main_dq_comp`, `dq-run`, `ModuleChildArgs`, and `ODqLanguageServer::RunWorker` share one source of truth.
   - Extract shared helpers for: `ToNativeInt`/`ToCharValue`/`ToNativeUInt`/`ToU32` (`ll_defs.cpp`), RTL module lookup/call (`CallRtlFunc`), built-in method parser scaffolding (`dqc_parser_expr.cpp`), `OExpr::FoldLValueChild`, landing-pad/vtable emission, and `OProcessRunner` POSIX/Windows preambles.
   - Replace manual state save/restore (`array_index_context_*`, `supress_varinit_check`, `curscope`) with small RAII guard structs.
3. **Structural refactoring for long-term human & AI velocity:**
   - Split the >2,000-line god files by cohesive concern—starting with `module_intf.cpp` (`dqmif_reader/writer`, `module_cache`, `module_runner`, `module_dump`), `expressions.cpp`, and `dqc_parser_expr.cpp`.
   - Replace `ODqComp*` linear inheritance with composition (`CompilerSession` / `DiagContext` / `Parser` / `Codegen`) and gradually eliminate global singletons so subsystems become unit-testable in isolation.
