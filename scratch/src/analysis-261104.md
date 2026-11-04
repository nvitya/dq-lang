# DQ Compiler & RTL Codebase Analysis Report
**Date:** 2026-11-04  
**Scope:** `compiler/` and `stdpkg/rtl/` (~63,000 LOC across 136 files)  
**Objective:** Identify code duplications, unclear or inconsistent solutions, dead/leftover code, and harder-to-maintain parts across the C++ compiler and its tightly coupled DQ runtime library.

---

## Table of Contents
1. [Code Duplications](#1-code-duplications)
   - [1.1 Managed/Owned Type Cleanup & Initialization Dispatch](#11-managedowned-type-cleanup--initialization-dispatch-duplicated-in-5-places)
   - [1.2 RTTI TypeInfo Hierarchy Walk & VTable Slot Lookup](#12-rtti-typeinfo-hierarchy-walk--vtable-slot-lookup)
   - [1.3 Builtin Type Method & Slice Parsing/Lowering Boilerplate](#13-builtin-type-method--slice-parsinglowering-boilerplate)
   - [1.4 RTL Function Lookup & LLVM Integer Cast Helpers](#14-rtl-function-lookup--llvm-integer-cast-helpers)
   - [1.5 Compound Type Declarations, Member Lookup & Property Accessors](#15-compound-type-declarations-member-lookup--property-accessors)
   - [1.6 Overload Candidate Analysis & Call Argument Lowering](#16-overload-candidate-analysis--call-argument-lowering)
   - [1.7 Parallel `ConvertFromExpr` and `GetConversionCostFromExpr` Logic](#17-parallel-convertfromexpr-and-getconversioncostfromexpr-logic)
   - [1.8 Diagnostic Method Overload Explosion in `ODqCompBase`](#18-diagnostic-method-overload-explosion-in-odqcompbase)
   - [1.9 Atomic Compilation Artifact Publishing](#19-atomic-compilation-artifact-publishing)
2. [Unclear Solutions, Inconsistencies & Dead Remnants](#2-unclear-solutions-inconsistencies--dead-remnants)
   - [2.1 Inconsistent `OExpr` & `OStmt` Destruction (`delete` vs `OExpr::DeleteTree`)](#21-inconsistent-oexpr--ostmt-destruction-delete-vs-oexprdeletetree)
3. [Harder-to-Maintain Parts & Architectural Bottlenecks](#3-harder-to-maintain-parts--architectural-bottlenecks)
   - [3.1 Single-Inheritance "God Object" Compiler Hierarchy & Global State](#31-single-inheritance-god-object-compiler-hierarchy--global-state)
   - [3.2 Oversized Translation Units & Very Long Functions](#32-oversized-translation-units--very-long-functions)
4. [Fixed Issues](#4-fixed-issues)
   - [2.2 Leftover Polling-Based Exception Machinery (Compiler & `stdpkg/rtl/exception.dq`)](#22-leftover-polling-based-exception-machinery-compiler--stdpkgrtlexceptiondq)
   - [2.3 Unused Legacy Free-Function Wrappers & Dead AST/Codegen Helpers](#23-unused-legacy-free-function-wrappers--dead-astcodegen-helpers)
   - [2.4 Subtle Bugs & Inconsistencies](#24-subtle-bugs--inconsistencies)
   - [3.3 Hardcoded Magic Bitflags in DQM Interface Serialization](#33-hardcoded-magic-bitflags-in-dqm-interface-serialization)

---

## 1. Code Duplications

### 1.1 Managed/Owned Type Cleanup & Initialization Dispatch (Duplicated in 5+ Places)
`OType` already defines virtual hooks `RequiresCleanup()`, `GenerateCleanup(OScope *, LlValue *)`, and `GenerateAssignment(OScope *, LlValue *, OExpr *, bool)` (`compiler/ast/symbols.h:350-352`). However, only `OTypeAutoFree`, `OTypeAnyValue`, and `OTypeArray` currently implement `RequiresCleanup()` / `GenerateCleanup()`. Because `OTypeDynArray` (`TK_DYN_ARRAY`), `OTypeDynString` (`TK_DYNSTR`), and fixed-storage `OVsObject` are not unified under those virtual hooks, manual `dynamic_cast` chains are repeated across five separate subsystems:

1. **`OScope::DefineValSym`** (`compiler/ast/oscope.cpp:104-142`):
   5 `if / else if` branches checking `AsAutoFreeType`, `RequiresCleanup()`, `objsym->IsFixedObjectStorage() || TK_DYN_ARRAY`, `TK_DYNSTR`, and `TK_ANYVALUE`.
   *(Note: Both `AsAutoFreeType` and `OTypeAnyValue` already return `RequiresCleanup() == true`, making the 1st and 5th branches redundant with the 2nd branch.)*
2. **`OScope::EmitOwnedObjectDestructors`** (`compiler/codegen/oscope_codegen.cpp:36-78`):
   Repeats the same 6-way dispatch (`AsAutoFreeType`, `RequiresCleanup()`, `OVsObject::IsFixedObjectStorage()`, `OTypeDynArray`, `OTypeDynString`, `OTypeAnyValue`).
3. **`OTypeObject::GenerateFieldDestructors`** (`compiler/types/otype_compound.cpp:693-730`):
   Repeats the same 5-way dispatch for object fields.
4. **`GenerateElementDestructor`** (`compiler/types/otype_array.cpp:146-200`):
   Repeats the same dispatch (`RequiresCleanup()`, `OTypeDynArray`, `OTypeDynString`, `OTypeAnyValue`, `TK_OBJECT`, `TK_ARRAY`, `IsCompound()`) for dynamic array elements.
5. **`OStmtVarDecl::Generate` & `OStmtReturn::Generate`** (`compiler/ast/statements.cpp:158-198, 253-325`):
   Manually branches on `AsAutoFreeType`, `RequiresCleanup()`, `OTypeDynArray`, `OTypeDynString`, and `OTypeAnyValue` even though all of them already override `OType::GenerateAssignment`.

**Recommendation:** Have `OTypeDynArray` and `OTypeDynString` override `RequiresCleanup()` and `GenerateCleanup()`, and add `OValSym::RequiresScopeCleanup()` / `OValSym::GenerateScopeCleanup(OScope *)` (overridden by `OVsObject` for fixed storage). All 5 sites collapse into a single virtual call.

---

### 1.2 RTTI TypeInfo Hierarchy Walk & VTable Slot Lookup
- **Duplicate RTTI Loop Emission**:
  `OTypeObject::GenerateInstanceOf` (`compiler/types/otype_compound.cpp:735-805`) and `ODqCompCodegen::GenerateExceptBranchMatch` (`compiler/codegen/dqc_codegen.cpp:163-242`) emit the exact same ~60-line LLVM IR loop: null-checking the object pointer, loading `vtable` from `vtable_field_index`, loading `TypeInfo` from `vtable[0]`, and looping over parent `TypeInfo` pointers at `phi_ti[1]` until `phi_ti == target_ti` or `null`.
  `GenerateExceptBranchMatch` can simply call `branch->exception_type->GenerateInstanceOf(nullptr, exc_type, ll_exc)` and branch on the resulting `i1`.
- **Repeated VTable Root & Slot Lookup**:
  Finding the polymorphic root object (`while (root->base_type) root = root->GetBaseObject();`), GEPing `root->vtable_field_index`, and loading a vtable slot is duplicated in 6 places:
  - `OTypeObject::GenerateVTableStore` (`compiler/types/otype_compound.cpp:653-664`)
  - `OTypeObject::GenerateInstanceOf` (`compiler/types/otype_compound.cpp:761-767`)
  - `OTypeAutoFree::GenerateCleanup` (`compiler/ast/symbols.cpp:404-415`)
  - `OStmtDelete::Generate` (`compiler/ast/statements.cpp:477-488`)
  - `GenerateFunctionCall` (`compiler/ast/expressions.cpp:1399-1416`)
  - `BuildObjectFuncRefValue` (`compiler/ast/expressions.cpp:1516-1531`)
  - `OTypeNameExpr::Generate` (`compiler/ast/expressions.cpp:2005-2016`)

  **Recommendation:** Add `OTypeObject::RootPolymorphicObject()` and `OTypeObject::GenerateVTableSlotLoad(LlValue * obj_addr, size_t slot_index, const string & name)` on `OTypeObject`.

---

### 1.3 Builtin Type Method & Slice Parsing/Lowering Boilerplate
- **Method Parser Boilerplate**:
  `ParseDynArrayMethod` (`compiler/parser/dqc_parser_expr.cpp:1330-1547`), `ParseEmbStrMethod` (`dqc_parser_expr.cpp:1610-1748`), `ParseStringMethod` (`dqc_parser_expr.cpp:1750-2009`), and `ParseAnyValueMethod` (`dqc_parser_expr.cpp:2011-2142`) each repeat:
  - Saving and restoring `array_index_context_len`, `array_index_context_lval`, and `array_index_context_wchar` (an RAII guard struct would replace all 6 manual save/restore blocks in `dqc_parser_expr.cpp`).
  - Identical `free_and_fail` and `check_count(mincnt, maxcnt)` lambdas.
  - Identical post-parse argument conversion loops.
- **Duplicate Slice Parsing**:
  `ParsePostfixIndexOrSlice` (`compiler/parser/dqc_parser_expr.cpp:1985-2065`) and `.wchar[...]` in `ParsePostfixDotMember` (`dqc_parser_expr.cpp:3025-3086`) duplicate the 60-line `[start:end]` / `[start::end]` slice vs single-index parser.
- **Duplicate Method-Call & Meta-Field AST Nodes**:
  - `ODynArrayMethodCallExpr`, `OEmbStrMethodCallExpr`, `OStringMethodCallExpr`, and `OAnyValueMethodCallExpr` (`compiler/ast/expressions.h:428-660`) share identical `FoldChildren()`, `DeleteChildTree()`, and `OCallLifetimeCleanup` `Generate()` scaffolding.
  - `OArrayMetaFieldExpr`, `OEmbStrMetaFieldExpr`, and `OStringMetaFieldExpr` (`compiler/ast/expressions.h:371, 603, 646`) are likewise near-identical wrappers around a receiver expression and a field enum.

---

### 1.4 RTL Function Lookup & LLVM Integer Cast Helpers
- **Repeated RTL Function Lookup & Call Helpers**:
  Five type implementation files define near-identical static lookup + `GenerateCallOrInvoke` helpers:
  - `DynStrFunc` / `CallDynStrFunc` and `TextFormatFunc` / `CallTextFormatFunc` (`compiler/types/otype_string.cpp:110-194`)
  - `EmbStrFunc` / `CallEmbStrFunc` (`compiler/types/otype_embstr.cpp:77-109`) — *note: `CallEmbStrFunc` uses `ll_builder.CreateCall` directly without `scope->GenerateCallOrInvoke`!*
  - `AnyValueFunc` / `CallAnyValueFunc` (`compiler/types/otype_anyvalue.cpp:89-121`)
  - `DynArrayFunc` / `CallDynArrayFunc` (`compiler/types/otype_array.cpp:75-107`)
  - `OTypeArraySlice::CallRuntimeFunc` (`compiler/types/otype_array.cpp:477-492`)
  - `EnumRtlFunc` (`compiler/types/otype_enum.cpp:27-39`)

  **Recommendation:** Centralize into a single helper (e.g. `OScope::CallRtlFunc(const string & ns, const string & name, vector<LlValue *> args, int overload_arg_count = -1)`).
- **Repeated Static LLVM Type/Constant/Cast Helpers**:
  `LlPtrType()`, `LlU32()`, `LlI32()`, `LlNativeInt()`, `LlBool()`, `ToU32()`, `ToNativeUInt()` are re-implemented as `static` helpers across `otype_string.cpp`, `otype_embstr.cpp`, `otype_array.cpp`, `otype_anyvalue.cpp`, and `expressions.cpp` alongside `ToNativeInt()` and `ToCharValue()` in `compiler/codegen/ll_defs.cpp`. Moving these small helpers to `ll_defs.h` / `ll_defs.cpp` will remove ~120 lines of boilerplate.

---

### 1.5 Compound Type Declarations, Member Lookup & Property Accessors
- **100% Identical Member Lookup Methods on `OTypeObject` vs `OCompoundType`**:
  In `compiler/types/otype_compound.cpp:299-339`, `OTypeObject::FindObjectMemberSymbol`, `OTypeObject::FindMemberSymbol`, `OTypeObject::FindObjectFieldIndex`, and `OTypeObject::FindFieldIndex` are line-for-line identical to `OCompoundType::FindMemberSymbol` and `OCompoundType::FindFieldIndex` (`otype_compound.cpp:923-953`), because `OTypeObject::GetBaseObject()` is simply `static_cast<OTypeObject *>(base_type)`.
  Also, `OCompoundType::CreateValSym` (`otype_compound.cpp:909-912`) is a trivial override that just calls `OType::CreateValSym`.
- **Compound Declaration Parser Duplication**:
  `ParseStructDecl` (`compiler/parser/dqc_parser.cpp:896-1088`), `ParseUnionDecl` (`dqc_parser.cpp:1174-1255`), and `ParseObjectDecl` (`dqc_parser.cpp:1985-2315`) duplicate:
  - Identifier + `CheckSpecialReservedRootName` + `ParseAttributes(false)`
  - Lookup of existing incomplete forward declaration in `cur_mod_scope` / `g_module->scope_pub`
  - Creation of `OCompoundType` / `OTypeObject` with `incomplete = true`, `DeclareType`, `discard_failed_new_decl`
  - Base type `(BaseType)` parsing (`ParseStructDecl` lines 959-997 vs `ParseObjectDecl` lines 2048-2079)
  - Completion of forward declarations (`if (is_forward_def) g_module->OModuleBase::declarations.push_back(new OIntfDecl(..., false));`)
- **Fixed Embedded Object Declaration (`<- Type(...)` / `<- Type ..`)**:
  Duplicated between `ODqCompParser::ParseObjectDecl` (`compiler/parser/dqc_parser.cpp:2151-2218`) and `ODqCompParserStmt::ParseStmtVar` (`compiler/parser/dqc_parser_stmt.cpp:166-222, 337-408`).
- **Property Accessor Resolution (`resolve_accessor`)**:
  `ODqCompParser::ReadObjectProperty` (`compiler/parser/dqc_parser.cpp:2320-2412`) and `OModuleIntf::ReadPropertyDecl` (`compiler/ast/module_intf.cpp:2697-2745`) both implement `resolve_accessor` lambdas that look up the member symbol on `object_type`, validate field vs method/overload-set accessors using `property->MatchMethod(candidate, write)`, and assign `raccessor` and `rdecl_type`.
- **Virtual Method Registration in `OTypeObject::UpdateObjectInheritanceFlags`**:
  In `compiler/types/otype_compound.cpp:436-492`, the 24-line block handling `attr_is_virtual`, `attr_is_override`, `FindVirtualBaseMethod`, `FindVirtualSlot`, and `attr_is_abstract` is duplicated for single `OValSymFunc *` members and `OValSymOverloadSet *` members.

---

### 1.6 Overload Candidate Analysis & Call Argument Lowering
- **Constructor vs Function Overload Resolution**:
  `OTypeObject::FindConstructorForArgs` (`compiler/types/otype_compound.cpp:85-215`) manually re-implements the exact same parameter count check, conversion cost accumulation, `ref`/`refin`/`refout`/`refnil` lvalue binding validation, default parameter counting, and `CompareCallCandidateScore` tie-breaking that already exist in `OTypeFunc::AnalyzeCallCandidate` (`compiler/types/otype_func.cpp:365-452`) and `ODqCompParserExpr::ParseExprOverloadCallWithRawArgs` (`compiler/parser/dqc_parser_expr.cpp:4398-4465`).
- **Call Argument Generation, C Varargs Promotions & `OCallLifetimeCleanup`**:
  - C varargs default promotions (`float -> double`, `<32-bit int -> sext/zext i32`) are duplicated verbatim in `OCallExpr::Generate` (`compiler/ast/expressions.cpp:1357-1381`) and `OIndirectCallExpr::Generate` (`compiler/ast/expressions.cpp:1639-1660`).
  - The loop evaluating call arguments and registering them with `OCallLifetimeCleanup::CaptureArgument` is repeated in 6 places: `OCallExpr::Generate`, `OIndirectCallExpr::Generate`, `ONewExpr::Generate` (`expressions.cpp:945-961`), `OVsObject::GenerateConstructorCall` (`otype_compound.cpp:866-881`), `OStmtObjectCall::Generate` (`statements.cpp:364-379`), and `OStmtInheritedCall::Generate` (`statements.cpp:405-420`).

---

### 1.7 Parallel `ConvertFromExpr` and `GetConversionCostFromExpr` Logic
In 14 `OType` subclasses (`OTypePointer`, `OTypeInt`, `OTypeFloat`, `OTypeCharBase`, `OTypeArray`, `OTypeArraySlice`, `OTypeDynArray`, `OTypeEmbStr`, `OTypeStrSlice`, `OTypeDynString`, `OTypeRoStr`, `OTypeObject`, `OTypeObjectTypeRef`, `OTypeFuncRef`), `ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)` and `GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)` maintain parallel condition trees that must stay in sync whenever conversion rules change.
*(Note: `OCompoundType::AnalyzeLiteral` in `otype_compound.cpp:1182` solved this cleanly by taking a `bool convert` parameter and returning `-1` on failure or `>= 0` cost on success.)*

---

### 1.8 Diagnostic Method Overload Explosion in `ODqCompBase`
In `compiler/src/dqc_base.h:48-87` and `compiler/src/dqc_base.cpp:138-368`, `Error`, `ErrorTxt`, `RootStatementError`, `StatementError`, `Warning`, and `Hint` each declare 4 overloads (24 methods total, ~130 lines of forwarding boilerplate) just to supply default `""` values for `par1`, `par2`, and `par3`. Using default parameter values reduces these 24 declarations and 18 forwarding bodies to 6 methods. Also, the `bool atryrecover = true` parameter on all 8 `RootStatementError` and `StatementError` overloads is completely unused in `dqc_base.cpp`.

---

### 1.9 Atomic Compilation Artifact Publishing
`ODqCompCodegen::EmitObject` (`compiler/codegen/dqc_codegen.cpp:621-656`) and `ODqCompCodegen::EmitBitcode` (`dqc_codegen.cpp:658-711`) duplicate parent-directory creation (`ArtifactEnsureParentDir`), temporary path creation (`ArtifactTempPathFor`), `llvm::raw_fd_ostream` opening/cleanup, and `ArtifactAtomicReplace`.

---

## 2. Unclear Solutions, Inconsistencies & Dead Remnants

### 2.1 Inconsistent `OExpr` & `OStmt` Destruction (`delete` vs `OExpr::DeleteTree`)
The AST currently has two conflicting conventions for freeing `OExpr` trees:
- `OExpr::DeleteTree(OExpr * expr)` (`compiler/ast/expressions.cpp:107-115`) calls `expr->DeleteChildTree()` and then `delete expr;`.
- Most `OExpr` subclasses only free their children inside `DeleteChildTree()` and have a default trivial `~OExpr()` destructor.
- However, **four `OExpr` subclasses** also define custom destructors:
  - `OIifExpr::~OIifExpr()` (`expressions.cpp:1058-1063`) calls `delete condition; delete true_expr; delete false_expr;`
  - `OFloatRoundExpr::~OFloatRoundExpr()` (`expressions.cpp:1286-1289`) calls `delete src;`
  - `OCallExpr::~OCallExpr()` (`expressions.cpp:1447-1454`) calls `for (OExpr * arg : args) delete arg;`
  - `OIndirectCallExpr::~OIndirectCallExpr()` (`expressions.cpp:1691-1700`) calls `OExpr::DeleteTree(callee); for (OExpr * arg : args) delete arg;`
- Meanwhile, `ODqCompAst::FreeLeftRight(OExpr * left, OExpr * right)` (`compiler/ast/dqc_ast.cpp:395-400`) and dozens of error recovery paths in `dqc_parser_expr.cpp`, `dqc_parser_stmt.cpp`, and `dqc_ast.cpp` (`FinalizeStmtAssign`, `ParseStmtVar`, `ParseComparison`, etc.) call raw `delete expr;` instead of `OExpr::DeleteTree(expr);`.
  - When `delete expr;` is called on an `OBinExpr`, `OCompareExpr`, `OLValueMember`, `OLValueIndex`, etc., `DeleteChildTree()` is **never called**, leaking all child expressions.
  - Even inside `OIifExpr::~OIifExpr()` and `OCallExpr::~OCallExpr()`, calling raw `delete` on a child `OBinExpr` leaks the grandchildren.
- Similarly, in `compiler/ast/statements.h`, `OStmtFor`, `OStmtRaise`, `OStmtObjectCall`, `OStmtInheritedCall`, and `OStmtPropertyAssign` free their owned `OExpr *` members in their destructors, whereas `OStmtReturn`, `OStmtVarDecl`, `OStmtAssign`, `OStmtModifyAssign`, `OStmtVoidCall`, `OStmtDelete`, `OStmtWhile`, and `OIfBranch` do **not** delete their `OExpr *` members in their destructors.

**Recommendation:** Move child destruction into `virtual ~OExpr()` overrides on each `OExpr` subclass (or make `~OExpr` / subclass destructors call `DeleteTree` on children), remove `DeleteChildTree()`, make `OExpr::DeleteTree(expr)` a simple alias for `delete expr;`, and add destructors to the remaining `OStmt` / `OIfBranch` classes.

---

## 3. Harder-to-Maintain Parts & Architectural Bottlenecks

### 3.1 Single-Inheritance "God Object" Compiler Hierarchy & Global State
The compiler is structured as a 7-level linear inheritance chain:
`ODqCompBase` -> `ODqCompAst` -> `ODqCompParserExpr` -> `ODqCompParserStmt` -> `ODqCompParser` -> `ODqCompCodegen` -> `ODqCompClargs` -> `ODqCompiler` (`g_compiler`), coupled with global singletons (`ll_ctx`, `ll_builder`, `ll_module`, `di_builder`, `g_module`, `g_builtins`, `g_defines`, `g_namespaces`).
- Every parser state variable (`curblock`, `curscope`, `curvsfunc`, `loop_depth`, `except_depth`, `array_index_context_*`, `supress_varinit_check`, `suppress_access_read_check`, `attr`) lives as a mutable field on the compiler object and must be manually saved and restored across recursive contexts (`ParseSubFunction`, `ParseDefineConst`, `ParseDefineCondition`, `ParseDynArrayMethod`, etc.).
- Grouping expression/statement parser context into a small value struct (restored via `llvm::SaveAndRestore` or RAII guards) would eliminate forgotten state-restore bugs.

### 3.2 Oversized Translation Units & Very Long Functions
Several files and functions have grown very large and mix distinct responsibilities:
- **`compiler/parser/dqc_parser_expr.cpp` (5,570 lines)**: Combines RTL implicit-use helpers, `[[...]]` attribute parsing, type specifier parsing (`ParseTypeSpec`), function signature parsing, operator precedence parsing, postfix/member dispatch, builtin intrinsics (`iif`, `Defined`, `FirstInt`, `SizeOf`, `OffsetOf`, `Ord`, `TryCast`, `TypeName`), and all builtin type method parsers (`ParseDynArrayMethod`, `ParseEmbStrMethod`, `ParseStringMethod`, `ParseAnyValueMethod`). Extracting builtin-type method parsing and intrinsic parsing into dedicated files or onto their respective `OType` classes would cut `dqc_parser_expr.cpp` nearly in half.
- **`compiler/ast/expressions.cpp` (4,002 lines)** and **`compiler/ast/module_intf.cpp` (3,731 lines)**:
  - `ODqCompParserStmt::ParseStmtFor` (`dqc_parser_stmt.cpp:1665-2045`, ~380 lines) handles `for..in` array iteration and `for..to/downto/count/downcount/while` integer loops in a single function.
  - `ODqCompParserStmt::ParseStmtVar` (`dqc_parser_stmt.cpp:136-456`, ~320 lines) interleaves embedded object construction (`<-`), type inference (`?`), `tryfrom`, global constant-init evaluation, and local variable initialization.
  - `ODqCompParserExpr::ParseExprPrimary` (`dqc_parser_expr.cpp:3245-3618`, ~373 lines) and `ParsePostfixDotMember` (`dqc_parser_expr.cpp:2865-3153`, ~288 lines).

---

## 4. Fixed Issues

### 2.2 Leftover Polling-Based Exception Machinery (Compiler & `stdpkg/rtl/exception.dq`)
1. **`OScope::EmitExpressionExceptionCheck()`**:
   - **Resolution:** Removed `OScope::EmitExpressionExceptionCheck()`, the `EmitExpressionExceptionCheck(scope)` wrapper in `compiler/ast/expressions.h`, and all call sites across `expressions.cpp`, `otype_string.cpp`, `otype_array.cpp`, and `otype_enum.cpp`.
2. **`ODqCompCodegen::DqExceptionActiveValue()`**:
   - **Resolution:** Removed `ODqCompCodegen::DqExceptionActiveValue()` and the 6 dead `if (active)` branches in `compiler/ast/statements.cpp` (`OStmtReturn::Generate`, `OStmtWhile::Generate`, `OStmtFor::Generate`, `OBreakStmt::Generate`, `OContinueStmt::Generate`).
3. **Unused methods on `ODqCompCodegen`**:
   - **Resolution:** Removed `DqClearException()`, `DqBeginCatch()`, `DqEndCatch()`, and `EmitExceptionEscapeCheck()` from `compiler/codegen/dqc_codegen.h` and `compiler/codegen/dqc_codegen.cpp`.
4. **Unused runtime state and functions in `stdpkg/rtl/exception.dq`**:
   - **Resolution:** Removed `active_exception`, `handled_exception`, `DqSetActiveException`, `DqExcRethrow`, `DqExcActive`, `DqExcCurrent`, `DqExcClear`, `DqExcBeginCatch`, `DqExcEndCatch`, and `DqExcPrintTypeName` from `stdpkg/rtl/exception.dq`, along with the dead `PrintUncaughtException()` / `if DqExcActive():` checks in `stdpkg/rtl/rtl_linux.dq` and `stdpkg/rtl/rtl_windows.dq`.

### 2.3 Unused Legacy Free-Function Wrappers & Dead AST/Codegen Helpers
1. **Redundant Free Functions in `otype_string.{h,cpp}`**:
   - **Resolution:** Removed all unused `GenerateString*` forwarding stubs from `compiler/types/otype_string.h` and `compiler/types/otype_string.cpp`, and moved `GenerateStringMethodCall` to `OTypeString::GenerateMethodCall` (called by `OStringMethodCallExpr::Generate`).
2. **Unused AST Node Classes in `expressions.{h,cpp}`**:
   - **Resolution:** Removed unused `OEmbStrSizeExpr` and `OEmbStrToDescExpr` from `compiler/ast/expressions.h` and `compiler/ast/expressions.cpp`.
3. **Unused ~95-Line IR Generator in `otype_embstr.cpp`**:
   - **Resolution:** Removed unused static functions `GetEmbStrCopySource`, `EmitSizedEmbStrCopy`, and `LlNativeIntType` from `compiler/types/otype_embstr.cpp`.
4. **Duplicate Mid-File `#include` Directives in `dqc_ast.cpp`**:
   - **Resolution:** Removed the duplicate mid-file `#include "otype_anyvalue.h"` and `#include "../types/otype_anyvalue.h"` from `compiler/ast/dqc_ast.cpp`.
5. **Redundant Implicit Constructor/Destructor Logic in `ODqCompParser::ParseObjectDecl`**:
   - **Resolution:** Simplified implicit constructor generation in `ODqCompParser::ParseObjectDecl` (`compiler/parser/dqc_parser.cpp`) to pass `inherited_ctor` directly to `AddGeneratedObjectConstructor`, and removed the redundant `needs_implicit_dtor` constant check around `AddGeneratedObjectDestructor`.

### 2.4 Subtle Bugs & Inconsistencies
1. **Inline Assembly Dialect Hardcoding in `OValSymFunc::GenerateFuncBody`**:
   - **Resolution:** Updated `OValSymFunc::GenerateFuncBody` (`compiler/types/otype_func.cpp`) to query `InlineAsmTargetForArch(g_opt.target.arch)->dialect` instead of checking `"x86_64" == g_opt.target.arch`.
2. **Double Allocation (Memory Leak) of `di_builder` in `ll_defs.cpp`**:
   - **Resolution:** Removed the redundant `di_builder = new LlDiBuilder(*ll_module);` allocation from `ll_init_debug_info()` (`compiler/codegen/ll_defs.cpp`), reusing the instance created in `ll_defs_init()`.
3. **`compiler/src/rtlint.h` vs `stdpkg/rtl/rtlint.dq` Out of Sync**:
   - **Resolution:** Added `DQTK_UNION = 18` and `DQTK_OBJECT_TYPE = 30` to `stdpkg/rtl/rtlint.dq`.
4. **String Allocation on Every Identifier Check in `ODqCompBase`**:
   - **Resolution:** Replaced the pipe-delimited string searches in `ReservedWord`, `RootStatementWord`, and `IsBlockCloserWord` (`compiler/src/dqc_base.h`, `compiler/src/dqc_base.cpp`) with `string_view` parameters and `static const unordered_set<string_view>` lookups, adding `"enum"` to `RootStatementWord` and removing `"endclass"` from `IsBlockCloserWord`.

### 3.3 Hardcoded Magic Bitflags in DQM Interface Serialization
Symbol attribute flags in `.dqm` interface files (`1u << 0` through `1ull << 19`) were written and read using raw shift literals in three separate files:
- `OValSym::WriteDqmIfAttributes` (`compiler/ast/symbols.cpp`)
- `OValSymFunc::WriteDqmIfFunction` (`compiler/types/otype_func.cpp`)
- `ApplyDqmIfAttributes` and `OModuleIntf::ReadFunctionDecl` (`compiler/ast/module_intf.cpp`)

**Resolution:** Defined `enum EDqmIfAttrFlag : uint64_t` (`DQMIF_ATTRF_*`) in `compiler/ast/dqm_if.h` and replaced all raw bit-shift literals across serialization and deserialization sites.
