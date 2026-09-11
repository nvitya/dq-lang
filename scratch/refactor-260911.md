# DQ Compiler Codebase Analysis Report
**Date:** 2026-09-11  
**Scope:** `compiler/` directory (~52,000 LOC across ~80 C++ source and header files)  
**Objective:** Identify code duplications, overly long functions, and free/static functions that should be member functions per project guidelines (*"Put helper behavior on the objects that own the responsibility, avoiding new broad file-static helper clusters"*).

---

## Table of Contents
1. [Code Duplications](#1-code-duplications)
2. [Very Long Functions](#2-very-long-functions)
3. [Functions That Could Be Made Member Functions](#3-functions-that-could-be-made-member-functions)
   - [A. Method Signatures & Matching (`OValSymFunc`)](#a-method-signatures--matching-ovalsymfunc)
   - [B. Function Types (`OTypeFunc`)](#b-function-types-otypefunc)
   - [C. Expression Helpers (`OPropertyExpr`, `OExpr`)](#c-expression-helpers-opropertyexpr-oexpr)
   - [D. Scope & Module Helpers (`OScope` & `TDQModule`)](#d-scope--module-helpers-oscope--tdqmodule)
   - [E. Type Interface Serialization (`OType` / `ODqmIfWriter`)](#e-type-interface-serialization-otype--odqmifwriter)
4. [Key Architectural Recommendations](#4-key-architectural-recommendations)
5. [Implemented and Ignored](#5-implemented-and-ignored)
   - [A. Implemented](#a-implemented)
   - [B. Ignored](#b-ignored)

---

## 1. Code Duplications

### Structural & Algorithmic Duplications

1. **Built-in Type Method Call Parsing Boilerplate**:
   - [`ParseDynArrayMethod`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L1691-L1737), [`ParseCStringMethod`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L1913-L1958), [`ParseStringMethod`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L2056-L2100), and [`ParseAnyValueMethod`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L2270-L2304) share identical sequences for:
     - Opening parenthesis check
     - Context preservation and restoration (`array_index_context_len`, `array_index_context_lval`, `array_index_context_wchar`)
     - Raw argument parsing and cleanup (`free_and_fail`, `check_count` lambdas)

2. **Unknown Member Error Recovery in Postfix**:
   - [`compiler/parser/dqc_parser_expr.cpp:2526-2535`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L2526-L2535) and [`compiler/parser/dqc_parser_expr.cpp:2675-2684`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L2675-L2684) repeat the exact 10-line error-reporting and dummy argument recovery block:
     ```cpp
     Error(DQERR_MEMBER_UNKNOWN, membername, lval->ptype->name);
     if (scf->CheckSymbol("("))
     {
       vector<TRawCallArg> rawargs;
       ParseRawCallArguments(membername, rawargs);
       FreeRawCallArguments(rawargs);
       delete result;
       result = new OInvalidCallExpr();
     }
     return result;
     ```

3. **Polymorphic Method Registration**:
   - In [`OTypeObject::CollectVirtualMethods`](file:///lindata2/workpr/dq-lang/compiler/types/otype_compound.cpp#L543-L565) and [`compiler/types/otype_compound.cpp:574-596`](file:///lindata2/workpr/dq-lang/compiler/types/otype_compound.cpp#L574-L596), the 22-line virtual slot calculation and abstract checking logic is duplicated between direct method symbols and methods unpacked from overload sets.

4. **C Varargs Default Argument Promotions**:
   - In [`OFuncCallExpr::Generate`](file:///lindata2/workpr/dq-lang/compiler/ast/expressions.cpp#L2514-L2535) and [`OFuncPtrCallExpr::Generate`](file:///lindata2/workpr/dq-lang/compiler/ast/expressions.cpp#L2886-L2905), the LLVM promotion loop (`float -> double`, small integer sign/zero extension to `i32`) is repeated verbatim across 20 lines.

5. **Dynamic RTTI Traversal Loop Generation**:
   - In [`OTryCastExpr::Generate`](file:///lindata2/workpr/dq-lang/compiler/ast/expressions.cpp#L3584-L3630) and [`OIsExpr::Generate`](file:///lindata2/workpr/dq-lang/compiler/ast/expressions.cpp#L3681-L3730), ~40 lines of LLVM IR emission (vtable slot 0 lookup, loop block creation, PHI node, and typeinfo hierarchy walk) are duplicated.

6. **Pointer Arithmetic Type Validation**:
   - [`FinalizeStmtAssign`](file:///lindata2/workpr/dq-lang/compiler/ast/dqc_ast.cpp#L1903-L1915) (property assignment) and [`compiler/ast/dqc_ast.cpp:2002-2015`](file:///lindata2/workpr/dq-lang/compiler/ast/dqc_ast.cpp#L2002-L2015) (lvalue assignment) duplicate pointer opacity checks and integer offset type checks.

7. **Atomic Compilation Artifact Publishing**:
   - [`ODqCompCodegen::EmitObject`](file:///lindata2/workpr/dq-lang/compiler/codegen/dqc_codegen.cpp#L619-L645) and [`ODqCompCodegen::EmitBitcode`](file:///lindata2/workpr/dq-lang/compiler/codegen/dqc_codegen.cpp#L654-L676) duplicate directory creation, temporary file name creation, output stream error checks, and atomic file replacement.

---

## 2. Very Long Functions

| Lines | Function | File & Location | Primary Cause of Length |
|---|---|---|---|
| **583** | `ODqCompParserExpr::ParsePostfix` | [`dqc_parser_expr.cpp:2382-2964`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L2382-L2964) | Monolithic postfix loop handling `[]`, `.`, `()`, `^`, slicing, method calls, properties, enum ordinals, and pointers in one nested construct (up to 8 indent levels). |
| **481** | `ODqCompParser::FinishFunctionDecl` | [`dqc_parser.cpp:1613-2093`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser.cpp#L1613-L2093) | Attribute validation, inline assembly hints, forward declaration resolution, and three large nested lambdas (`read_function_body`, `declare_function`, `fill_forward_decl`). |
| **396** | `ODqCompParserExpr::ParseExprPrimary` | [`dqc_parser_expr.cpp:3027-3422`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L3027-L3422) | Hand-rolled token dispatch for casts, `()`, `[]`, `{}`, contextual identifiers (`$end`, `$last`), literals, keywords, intrinsics, and variable references. |
| **371** | `ODqCompParser::ParseObjectDecl` | [`dqc_parser.cpp:2522-2892`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser.cpp#L2522-L2892) | Header parsing, attributes, inheritance parsing, member loops (fields, methods, properties, constructors), and forward declaration validation. |
| **333** | `ODqCompParserExpr::ParseTypeSpec` | [`dqc_parser_expr.cpp:614-946`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L614-L946) | Pointer types (`^T`), fixed arrays (`[N]T`), slices (`[]T`), dynamic arrays (`[*]T`), function pointer types, qualified paths, and built-in type mapping. |
| **325** | `ODqCompParserStmt::ReadStatementBlock` | [`dqc_parser_stmt.cpp:934-1258`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_stmt.cpp#L934-L1258) | Block delimiter matching (`: ... end` vs `{ ... }`) mixed with keyword dispatching and inlined handling of `break`, `continue`, `delete`, and assignments. |
| **313** | `ODqCompParserExpr::ParseFunctionSignature` | [`dqc_parser_expr.cpp:949-1261`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L949-L1261) | Parameter mode parsing, default arguments, return type specs, parameter attributes, and varargs. |
| **311** | `ODqCompParserStmt::ParseStmtFor` | [`dqc_parser_stmt.cpp:1509-1819`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_stmt.cpp#L1509-L1819) | Both classic for loops (`for i := 0; i < N; ++i`) and range for loops (`for elem in coll`) with step evaluation and direction analysis. |
| **291** | `ODqCompParserStmt::ParseStmtVar` | [`dqc_parser_stmt.cpp:133-423`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_stmt.cpp#L133-L423) | Handling multiple syntax forms: standard `var x : T = v`, type-inferred `var x = v`, fixed-object initialization `var x <- T(...)`, and global vs local scopes. |
| **272** | `ODqCompParser::ParseUseStatement` | [`dqc_parser.cpp:612-883`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser.cpp#L612-L883) | Path resolution, package lookups, alias mappings, selective import lists (`only`, `exclude`), and re-export validation. |
| **272** | `ODqCompParser::ReadObjectProperty` | [`dqc_parser.cpp:2248-2519`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser.cpp#L2248-L2519) | Property syntax parsing, index declaration validation, getter/setter accessor matching, and field backing generation. |
| **265** | `OProcessRunner::Run` (POSIX) | [`processrunner.cpp:620-884`](file:///lindata2/workpr/dq-lang/compiler/utils/processrunner.cpp#L620-L884) | Pipe creation, `fork()`, descriptor redirection, `execvp()`, non-blocking `poll()` loop, timeout tracking, and process exit status reaping. |
| **244** | `ODqLanguageServer::Handle` | [`lang_server.cpp:685-928`](file:///lindata2/workpr/dq-lang/compiler/langserver/lang_server.cpp#L685-L928) | Giant `if/else` dispatch tree handling all JSON-RPC LSP requests inline. |
| **233** | `OCompOptions::ProcessCommandLineOpts` | [`comp_options.cpp:393-625`](file:///lindata2/workpr/dq-lang/compiler/src/comp_options.cpp#L393-L625) | Linear command-line switch parser. |
| **230** | `ODqCompiler::Run` | [`dqc.cpp:505-734`](file:///lindata2/workpr/dq-lang/compiler/src/dqc.cpp#L505-L734) | Compiler pipeline coordinator (module resolution, parsing, dependency ordering, AST analysis, codegen, linking, and cleanup). |
| **229** | `ODqCompParserExpr::ParseSingleAttribute` | [`dqc_parser_expr.cpp:288-516`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L288-L516) | Long switch over attribute string names (`[[packed]]`, `[[section]]`, `[[align]]`, `[[inline]]`, etc.) and argument validation. |
| **220** | `ODqCompParserExpr::ParseDynArrayMethod` | [`dqc_parser_expr.cpp:1691-1910`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L1691-L1910) | Dispatch for all dynamic array member functions (`append`, `insert`, `clear`, `resize`, `clone`, etc.). |
| **213** | `ODqCompParserExpr::ParseStringMethod` | [`dqc_parser_expr.cpp:2056-2268`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L2056-L2268) | Dispatch for all `str` and `strview` member methods. |
| **210** | `OValueInt::CalculateConstant` | [`otype_int.cpp:82-291`](file:///lindata2/workpr/dq-lang/compiler/types/otype_int.cpp#L82-L291) | Inlined evaluation covering 15 different expression types for integer constexpr. |
| **209** | `ODqCompParser::ParseStructDecl` | [`dqc_parser.cpp:1192-1400`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser.cpp#L1192-L1400) | Struct header parsing, attribute application, member fields loop, and packing layout calculation. |
| **198** | `ODqCompAst::BindCallArguments` | [`dqc_ast.cpp:1528-1725`](file:///lindata2/workpr/dq-lang/compiler/ast/dqc_ast.cpp#L1528-L1725) | Positional & default argument resolution, type conversion cost calculation, and error generation. |
| **196** | `OCompareExpr::Generate` | [`expressions.cpp:1337-1532`](file:///lindata2/workpr/dq-lang/compiler/ast/expressions.cpp#L1337-L1532) | Comparison IR emission branching over integer, float, pointer, boolean, string, enum, and anyvalue. |
| **196** | `ODqCompAst::FinalizeStmtAssign` | [`dqc_ast.cpp:1874-2069`](file:///lindata2/workpr/dq-lang/compiler/ast/dqc_ast.cpp#L1874-L2069) | Assignment validation covering read-only symbols, property setters, compound assignments, string concats, and pointer arithmetic. |
| **184** | `OModuleIntf::WriteInterfaceRecords` | [`module_intf.cpp:1582-1765`](file:///lindata2/workpr/dq-lang/compiler/ast/module_intf.cpp#L1582-L1765) | Serializing module metadata, dependencies, types, symbols, and constants into binary `.dqm` records. |
| **177** | `OModuleIntf::ReadFunctionDecl` | [`module_intf.cpp:2598-2774`](file:///lindata2/workpr/dq-lang/compiler/ast/module_intf.cpp#L2598-L2774) | Deserializing function declarations and parameter signatures from `.dqm`. |
| **176** | `ODqCompParser::ParseEnumDecl` | [`dqc_parser.cpp:990-1165`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser.cpp#L990-L1165) | Enum parsing, underlying type spec, item evaluation, auto-increment values, and duplicate checks. |
| **174** | `ODqCompParserExpr::ParseComparison` | [`dqc_parser_expr.cpp:1350-1523`](file:///lindata2/workpr/dq-lang/compiler/parser/dqc_parser_expr.cpp#L1350-L1523) | Relational operators, empty array literal comparison rewrite, and chaining checks. |
| **173** | `OStmtTry::Generate` | [`statements.cpp:971-1143`](file:///lindata2/workpr/dq-lang/compiler/ast/statements.cpp#L971-L1143) | Try/catch/finally block setup, landing pad generation, type matching, and exception unwinding. |
| **169** | `OModuleIntf::ReadPropertyDecl` | [`module_intf.cpp:2872-3040`](file:///lindata2/workpr/dq-lang/compiler/ast/module_intf.cpp#L2872-L3040) | Deserializing property records and matching accessor linkages from `.dqm`. |
| **158** | `OValSymFunc::GenerateFuncBody` | [`otype_func.cpp:1119-1276`](file:///lindata2/workpr/dq-lang/compiler/types/otype_func.cpp#L1119-L1276) | LLVM function entry block creation, parameter allocas, debug info setup, statement body emission, and implicit returns. |

---

---

## 3. Key Architectural Recommendations

1. **Decompose Top-Heavy Parser Functions**:
   - Break [`ODqCompParserExpr::ParsePostfix`](file:///lindata/workvc/dq-lang/compiler/parser/dqc_parser_expr.cpp#L2382) (583 lines) into distinct helper member methods: `ParsePostfixDotMember`, `ParsePostfixIndexOrSlice`, `ParsePostfixCall`, and `ParsePostfixPointerOps`.
   - Break [`ODqCompParserStmt::ReadStatementBlock`](file:///lindata/workvc/dq-lang/compiler/parser/dqc_parser_stmt.cpp#L934) (325 lines) into an isolated `ParseStatement()` dispatcher, separating loop control and block boundary scanning from individual statement parsing.
   - Extract the nested lambdas in [`ODqCompParser::FinishFunctionDecl`](file:///lindata/workvc/dq-lang/compiler/parser/dqc_parser.cpp#L1613) (481 lines) into private class member functions (`ReadFunctionBody`, `DeclareFunctionSymbol`, `ResolveForwardDecl`).

---

## 4. Implemented and Ignored

### A. Implemented

1. **LValue Cloning (was 1.A.1, 3.G)**:
   - **Original Issue:** `CloneContextLValue` in [`compiler/parser/dqc_parser.cpp`](file:///lindata/workvc/dq-lang/compiler/parser/dqc_parser.cpp#L40-L56) and `CloneContextLValue` in [`compiler/parser/dqc_parser_expr.cpp`](file:///lindata/workvc/dq-lang/compiler/parser/dqc_parser_expr.cpp#L171-L187) were 100% identical 17-line static functions with manual `dynamic_cast` checks. In `dqc_parser.cpp` it was completely unused dead code.
   - **Resolution:** Replaced with virtual `virtual OLValueExpr * Clone() const` on [`OLValueExpr`](file:///lindata/workvc/dq-lang/compiler/ast/expressions.h#L74) and overrides on [`OLValueVar`](file:///lindata/workvc/dq-lang/compiler/ast/expressions.h#L89) and [`OLValueMember`](file:///lindata/workvc/dq-lang/compiler/ast/expressions.h#L119) in [`compiler/ast/expressions.cpp`](file:///lindata/workvc/dq-lang/compiler/ast/expressions.cpp). Removed both static functions and updated the call site in [`dqc_parser_expr.cpp`](file:///lindata/workvc/dq-lang/compiler/parser/dqc_parser_expr.cpp).

2. **TextFormat RTL Function Lookup (was 1.A.2)**:
   - **Original Issue:** `TextFormatFunc` in [`compiler/types/otype_cstring.cpp`](file:///lindata/workvc/dq-lang/compiler/types/otype_cstring.cpp#L93-L105) and `TextFormatFunc` in [`compiler/types/otype_string.cpp`](file:///lindata/workvc/dq-lang/compiler/types/otype_string.cpp#L150-L162) were identical 14-line static functions looking up symbols in `__dq_textformat`.
   - **Resolution:** Declared `TextFormatFunc` and `CallTextFormatFunc` in [`compiler/types/otype_string.h`](file:///lindata/workvc/dq-lang/compiler/types/otype_string.h) and defined them non-static in [`compiler/types/otype_string.cpp`](file:///lindata/workvc/dq-lang/compiler/types/otype_string.cpp). Deleted duplicated static functions in [`compiler/types/otype_cstring.cpp`](file:///lindata/workvc/dq-lang/compiler/types/otype_cstring.cpp) and passed `scope` to `CallTextFormatFunc` for proper exception invoke handling.

3. **Property Accessor Signature Matching (was 1.1, 3.E, 4.4)**:
   - **Original Issue:** `MatchPropertyMethod` in [`compiler/parser/dqc_parser.cpp`](file:///lindata/workvc/dq-lang/compiler/parser/dqc_parser.cpp) and `ImportedPropertyMethodMatches` in [`compiler/ast/module_intf.cpp`](file:///lindata/workvc/dq-lang/compiler/ast/module_intf.cpp) executed the exact same algorithm to validate whether a candidate getter/setter method matched a property's indices, value type, and modes.
   - **Resolution:** Unified logic into [`OValSymProperty::MatchMethod(OValSymFunc * method, bool write) const`](file:///lindata/workvc/dq-lang/compiler/ast/symbols.h) returning `enum EPropertyAccessorMismatch { PAM_NONE, PAM_TYPE, PAM_SIGNATURE, PAM_MODE }` and added `OValSymProperty::SameType(OType * other) const` in [`compiler/ast/symbols.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/ast/symbols.h). Removed both static duplicate functions and updated callers in `dqc_parser.cpp` and `module_intf.cpp`.

4. **Dynamic Array Operations Encapsulation (was 3.A & 4.1)**:
   - **Original Issue:** 27 free functions in `compiler/types/otype_array.h` took `(OScope * scope, OTypeDynArray * dyntype, ...)`.
   - **Resolution:** Moved all 27 functions to member methods on `OTypeDynArray` in [`compiler/types/otype_array.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_array.h) (`dyntype->GenerateDataPtr(...)`, `dyntype->GenerateLength(...)`, `dyntype->GenerateAppend(...)`, etc.). Also moved `DynArrayElementStorageType` and `DynArrayTypeInfo` to member methods on `OTypeDynArray`. Updated call sites across AST statements, expressions, codegen, and compound types.

5. **String Operations & Type Hierarchy Encapsulation (was 3.B & 4.1)**:
   - **Original Issue:** Over 20 free functions for string operations and 3 free functions for type classification (`IsTextSourceType`, `IsStringComparableTextType`, `IsStringFamilyTextType`).
   - **Resolution:**
     - Added `IsTextSource()`, `IsStringComparable()`, and `IsStringFamily()` directly on `OType` in [`compiler/ast/symbols.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/ast/symbols.h).
     - Created a shared base class `OTypeString : public OType` in [`compiler/types/otype_string.h`](file:///lindata/workvc/dq-lang/compiler/types/otype_string.h) for common string behavior (`OTypeDynString` and `OTypeStrView`).
     - Added virtual and common member methods on `OTypeString`: `GenerateLength`, `GeneratePChar`, `GenerateGetChar`, `GenerateCharAddress`, `GenerateSlice`, `GenerateMetaField`, `GenerateWcLen`, `GenerateWCharAt`, `GenerateWCharSlice`, `GenerateToWchars`, `GenerateEqual`.
     - Added lifecycle and mutation methods on `OTypeDynString`: `GenerateCapacity`, `GenerateRefCount`, `GenerateCreate`, `GenerateIncRef`, `GenerateDestroy`, `GenerateAssignExpr`, `GenerateSetChar`, `GenerateMethodCall`, `GenerateConcat`, `GenerateConcatFromStringValue`.
     - Updated all call sites across AST expressions, statements, codegen, array types, compound types, anyvalue types, and func types.

6. **CString Operations Encapsulation (was 3.C & 4.1)**:
   - **Original Issue:** Free functions `GenerateCStringDataPtr`, `GenerateCStringMetaField`, `GenerateCStringMethodCall` in `compiler/types/otype_cstring.h` alongside existing member functions.
   - **Resolution:** Converted to member methods on `OTypeCString` in [`compiler/types/otype_cstring.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_cstring.h): `GenerateDataPtr`, `GenerateMetaField`, and `GenerateMethodCall`. Moved helper enums `ECStringMetaField` and `ECStringMethod` before `OTypeCString` and updated call sites in `compiler/ast/expressions.cpp`.

7. **Compound & Struct Type Helpers Encapsulation (was 3.D & 4.1)**:
   - **Original Issue:** File-static functions `CollectStructInitFields` and `AnalyzeStructLiteral` in `otype_compound.cpp`, and `PropertyAccessorVisibleFrom` in `dqc_parser.cpp`.
   - **Resolution:**
     - Moved `CollectStructInitFields` to `OCompoundType::CollectInitFields(const vector<unsigned> & prefix, vector<SStructInitField> & result)`.
     - Moved `AnalyzeStructLiteral` to `OCompoundType::AnalyzeLiteral(OStructLit * literal, uint32_t aflags, bool do_convert)`.
     - Moved `PropertyAccessorVisibleFrom` to `OCompoundType::IsAccessorVisible(OValSym * accessor, OTypeObject * context_owner) const`.
     - Declared `struct SStructInitField` in [`compiler/types/otype_compound.h`](file:///lindata/workvc/dq-lang/compiler/types/otype_compound.h) and updated callers in `otype_compound.cpp` and `dqc_parser.cpp`.

8. **Function Signature, State & Parameter Mode Formatting (was 3.E)**:
   - **Original Issue:** Free helper functions `FunctionSignature`, `FunctionState`, `FuncTypeOf`, `IsImplicitReceiverParam`, and `ParamModeText` in `compiler/ast/module_intf.cpp`.
   - **Resolution:**
     - Added `OFuncParam::ModeText()` and `OFuncParam::ModeText(EParamMode)` on `OFuncParam` in [`compiler/types/otype_func.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_func.h).
     - Added `OValSymFunc::GetTypeFunc()`, `OValSymFunc::IsImplicitReceiver()`, `OValSymFunc::SignatureText()`, and `OValSymFunc::StateText()` on `OValSymFunc` in [`compiler/types/otype_func.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_func.h).
     - Removed static helper cluster from `compiler/ast/module_intf.cpp` and updated `WriteFunctionDump` to call the new methods.

9. **Shared Infrastructure Deduplication (was 4.3)**:
   - **Original Issue:**
     - `OTryCastExpr::Generate` and `OIsExpr::Generate` contained duplicate LLVM IR generation for polymorphic RTTI vtable/typeinfo inspection loops.
     - `MatchesOverloadDeclIdentity` and `MatchesSignature` in `OTypeFunc` duplicated 45 lines of signature comparison logic differing only by whether parameter modes are checked.
   - **Resolution:**
     - Added `OTypeObject::GenerateInstanceOf` and `OTypeObject::GenerateDynamicCast` to [`compiler/types/otype_compound.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_compound.h). Reduced `OTryCastExpr::Generate` and `OIsExpr::Generate` in [`compiler/ast/expressions.cpp`](file:///lindata/workvc/dq-lang/compiler/ast/expressions.cpp) to direct delegates.
     - Added `bool check_modes = true` parameter to `OTypeFunc::MatchesSignature` and inlined `MatchesOverloadDeclIdentity(other)` as `MatchesSignature(other, false)` in [`compiler/types/otype_func.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_func.h).

10. **Adopt RAII for `TRawCallArg` (was 4.2)**:
    - **Original Issue:** `TRawCallArg` did not own its expression, requiring 37 manual calls to `FreeRawCallArguments(rawargs)` across `dqc_parser.cpp`, `dqc_parser_stmt.cpp`, and `dqc_parser_expr.cpp` at every early return and block exit.
    - **Resolution:**
      - Added destructor `~TRawCallArg()` to [`compiler/ast/dqc_ast.h`](file:///lindata/workvc/dq-lang/compiler/ast/dqc_ast.h) that invokes `OExpr::DeleteTree(expr)` on unconsumed expressions.
      - Implemented move constructor and move assignment, deleted copy constructor and copy assignment to enforce single ownership, and added `TakeExpr()`.
      - Simplified `ODqCompAst::FreeRawCallArguments` in [`compiler/ast/dqc_ast.cpp`](file:///lindata/workvc/dq-lang/compiler/ast/dqc_ast.cpp) to `rawargs.clear()`.
      - Removed all 37 manual `FreeRawCallArguments(rawargs)` calls across `dqc_parser.cpp`, `dqc_parser_stmt.cpp`, and `dqc_parser_expr.cpp`.

11. **Method Signatures & Matching (was 3.A)**:
    - **Original Issue:** `CloneMethodVisibleSignature` in `compiler/ast/expressions.cpp` and `ConstructorUserSignaturesMatch` in `compiler/types/otype_compound.cpp` were static free functions operating directly on `OValSymFunc`.
    - **Resolution:** Added `OValSymFunc::CloneVisibleSignature()` and `OValSymFunc::UserSignaturesMatch(OValSymFunc * other)` directly to `OValSymFunc` in [`compiler/types/otype_func.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_func.h). Removed static helpers and updated call sites in `compiler/ast/expressions.cpp` and `compiler/types/otype_compound.cpp`.

12. **Function Types Formatting & Call Type Generation (was 3.B)**:
    - **Original Issue:** Free functions `FuncTypeName` and `FuncRefTypeName` in `compiler/types/otype_func.cpp`, and `CreateObjectFuncRefLlCallType` in `compiler/ast/expressions.cpp`.
    - **Resolution:** Added `FormattedTypeName()`, `FormattedRefTypeName(bool object_ref)`, and `CreateObjectRefLlCallType()` directly as member methods on `OTypeFunc` in [`compiler/types/otype_func.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_func.h). Maintained backwards-compatible inline bridge wrappers for existing call sites.

13. **Expression Helpers Encapsulation (was 3.C)**:
    - **Original Issue:** Static functions `GeneratePropertyReceiver`, `GeneratePropertyExplicitArgument`, `GeneratePropertyCallArgs`, and `GeneratePropertyFieldAddress` in `compiler/ast/expressions.cpp` handled property code generation. In `compiler/parser/dqc_parser_stmt.cpp`, `ExceptionObjectTypeFromExpr` extracted object types from expressions.
    - **Resolution:** Added `GenerateReceiver`, `GenerateExplicitArgument`, `GenerateCallArgs`, and `GenerateFieldAddress` as member methods on `OPropertyExpr` in [`compiler/ast/expressions.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/ast/expressions.h). Added `OExpr::GetExceptionObjectType()` to `OExpr` in [`compiler/ast/symbols.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/ast/symbols.h) and updated statement parsing.

14. **Scope & Module Helpers (was 3.D)**:
    - **Original Issue:** Static functions `ExceptionBaseType`, `AddMethodUseRootScopes`, and `FindModuleUseByNamespace` in `compiler/parser/dqc_parser_stmt.cpp`, and `EmitExpressionExceptionCheck` in `compiler/ast/expressions.cpp`.
    - **Resolution:** Added `GetExceptionBaseType()`, `AddMethodUseRootScopes(root_scope)`, and `EmitExpressionExceptionCheck()` directly onto `OScope` in [`compiler/ast/symbols.h`](file:///lindata/workvc/dq-lang/compiler/ast/symbols.h) / [`compiler/ast/oscope.cpp`](file:///lindata/workvc/dq-lang/compiler/ast/oscope.cpp). Added `FindModuleUseByNamespace(namespace_name)` directly to `OModuleBase` in [`compiler/ast/symbols.h`](file:///lindata/workvc/dq-lang/compiler/ast/symbols.h). Updated callers across parser and expression codegen.

15. **Type Interface Serialization & Effective Alignment (was 3.E)**:
    - **Original Issue:** `WriteDqmIfTypeRef`, `WriteDqmIfTypeSpecInner`, and `EffectiveStorageAlign` in `compiler/ast/symbols.cpp` were free/static functions operating on `OType`.
    - **Resolution:** Added `WriteDqmIfTypeRef(writer, arecid)`, `WriteDqmIfTypeSpecInner(writer)`, and `EffectiveAlign(attr_align)` as member methods on `OType` in [`compiler/ast/symbols.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/ast/symbols.h). Retained inline free functions for non-intrusive caller compatibility.

16. **Array LLVM IR Loop Generation (was 1.8)**:
    - **Original Issue:** `GetTypeDestroyFunc` and `GetTypeCopyFunc` in `compiler/types/otype_array.cpp` duplicated identical IR block setup (`entry`, `loop.cond`, `loop.body`, `loop.inc`, `loop.end`), index counter alloca, offset calculation, and element address GEPs.
    - **Resolution:** Factored loop skeleton into template helper `EmitArrayElementLoop` in [`compiler/types/otype_array.cpp`](file:///lindata/workvc/dq-lang/compiler/types/otype_array.cpp), deduplicating 45 lines of repetitive LLVM IR emission.

17. **Redundant Branch in Compound Type Conversion (was 1.9)**:
    - **Original Issue:** `OCompoundType::ConvertFromExpr` in `compiler/types/otype_compound.cpp` contained an `if (IsUnion())` branch with 17 lines of type checking and error reporting identical to the fallback branch directly below it.
    - **Resolution:** Removed the redundant `if (IsUnion())` block in [`compiler/types/otype_compound.cpp`](file:///lindata/workvc/dq-lang/compiler/types/otype_compound.cpp).

18. **Function Signature Matching (was 1.10)**:
    - **Original Issue:** `MatchesOverloadDeclIdentity` and `MatchesSignature` in `OTypeFunc` duplicated 40 lines of parameter and return type validation.
    - **Resolution:** Unified via `OTypeFunc::MatchesSignature(other, bool check_modes)` and inlined `MatchesOverloadDeclIdentity(other)` as `MatchesSignature(other, false)` in [`compiler/types/otype_func.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_func.h).

19. **Character Subtype Forwarding (was 1.11)**:
    - **Original Issue:** `OTypeChar`, `OTypeChar16`, and `OTypeWchar` each duplicated implementations of `CreateDiType`, `ConvertFromExpr`, and `GetConversionCostFromExpr`.
    - **Resolution:** Created intermediate base class `OTypeCharBase : public OTypeInt` in [`compiler/types/otype_char.{h,cpp}`](file:///lindata/workvc/dq-lang/compiler/types/otype_char.h) implementing these three methods once, with `OTypeChar`, `OTypeChar16`, and `OTypeWchar` inheriting from it.

### B. Ignored

1. **String Scanner Duplication (was 1.A.3)**:
   - Source scanning primitives in [`OScFeederBase`](file:///lindata/workvc/dq-lang/compiler/parser/scf_base.cpp#L816-L915) (`ReadDecimalNumbers`, `ReadHexNumbers`, `ReadBinNumbers`, `ReadQuotedString`) and [`TStrParseObj`](file:///lindata/workvc/dq-lang/compiler/utils/strparse.cpp#L310-L490) share near-identical scanning loops and quote-escape logic.
   - **Decision:** Kept separate as-is for code efficiency and readability (avoids coupling tokenizer scanner primitives with general string parsing).

2. **String Literal Escaping (was 1.A.4)**:
   - [`EscapeStringLiteral`](file:///lindata/workvc/dq-lang/compiler/ast/module_intf.cpp#L96-L128) in [`compiler/ast/module_intf.cpp`](file:///lindata/workvc/dq-lang/compiler/ast/module_intf.cpp) vs [`JsonEscape`](file:///lindata/workvc/dq-lang/compiler/utils/dq_utils.cpp#L23-L44) in [`compiler/utils/dq_utils.cpp`](file:///lindata/workvc/dq-lang/compiler/utils/dq_utils.cpp).
   - **Decision:** Kept separate as-is for code efficiency and readability.

