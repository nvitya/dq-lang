**Dq-test-framework-spec**

# DQ Compiler Test Framework Specification

## 1. Purpose

This document specifies a dedicated C++ test framework for the DQ compiler.

The framework is intended for **compiler regression testing**, not generic C++ unit testing.
It must support:

- easy creation of new tests
- stable verification of expected compiler diagnostics
- detection of unexpected compiler errors, warnings, and hints
- execution of successfully compiled test programs
- verification of runtime stdout, stderr, and exit code
- high parallel throughput
- strong logging and artifact collection for failed tests

The framework shall be implemented in C++ and shall not depend on Python.

---

## 2. Design Goals

### 2.1 Primary goals

1. **Self-contained test files**
   A test should normally consist of a single `.dq` source file containing both source code and test metadata.

2. **Stable source positions**
   Editing test metadata must not change compiler source line numbers above the metadata.

3. **Simple parsing**
   The test runner should parse test metadata with simple scanning rules. A full DQ parser is not required for the test system.

4. **Stable diagnostic verification**
   Tests should primarily verify diagnostic identifiers such as `TypeSpecExpected`, not full message wording.

5. **Strict unexpected-diagnostic detection**
   The framework must detect both missing expected diagnostics and unexpected additional diagnostics.

6. **Parallel execution**
   The runner must execute tests in parallel up to a configurable worker count.

7. **Good failure investigation support**
   Failed tests must preserve logs, command lines, captured outputs, and generated artifacts.

### 2.2 Secondary goals

- support emitted LLVM IR / assembly / AST / dump verification later
- support JSON reports later
- support selective rerun of failed tests later
- support baseline files later for larger outputs

---

## 3. Non-Goals

The framework is not intended to be:

- a generic C++ unit-test framework
- a replacement for compiler-internal assertions
- a benchmark framework
- a language server test system
- a property/fuzz testing framework in the initial version

---

## 4. Overall Architecture

The framework shall be a standalone executable, tentatively named:

```text
dq-autotest
```

The runner shall:

1. discover test files under a configured test root
2. parse inline test directives from each file
3. invoke the DQ compiler as a subprocess
4. capture compile stdout, stderr, exit code, duration, and generated files
5. optionally run the produced executable
6. verify expectations
7. emit a detailed console summary
8. preserve per-test artifacts for failed tests

The default mode shall be **subprocess-based** execution of the compiler.

This is preferred over in-process compiler invocation because it provides:

- crash isolation
- cleaner timeout handling
- no shared global-state contamination between tests
- realistic end-to-end behavior

---

## 5. Test File Model

Each test is normally one `.dq` file.

The test file contains:

1. **ordinary DQ source code**
2. optional **inline line-bound expectations** using `//?...`
3. optional source structure such as `#ifdef ERRORTEST` when the author wants separate source branches for error and runtime variants

### 5.1 Stability rule

Inline expectations should be placed on the same line as the relevant source statement whenever line-sensitive diagnostic matching is required.

### 5.2 Parsing rule

The test runner shall parse test metadata directly from the raw file text.

The compiler shall receive the source file unchanged.

The runner shall not rewrite or preprocess the file before passing it to the compiler.

---

## 6. Inline Test Directives

Inline directives are attached to a source line and define expectations for either compiler diagnostics or runtime behavior.

### 6.1 Marker syntax

The marker introducer is:

```text
//?
```

This sequence is chosen because:

- it is safe inside DQ line comments
- it is easy to search for with simple text scanning
- it avoids confusion with deactivated compiler directives such as `//#define MYDEF`

### 6.2 Initial inline directives

The initial framework shall support:

```text
//?error(...)
//?warning(...)
//?hint(...)
//?check(...)
//?checkerr(...)
//?ignore(...)
//?ignoreerr(...)
//?exit(...)
```

### 6.3 Diagnostic matching key

The argument inside diagnostic directives shall primarily be the **diagnostic identifier**, for example:

```dq
var a : int = true;  //?error(TypeIncompatible)
```

The diagnostic identifier is expected to be the short code emitted by the compiler, for example:

```text
ERROR(TypeSpecExpected)
WARNING(UnusedVariable)
HINT(DeclHere)
```

### 6.4 Extended inline arguments

The framework may later support additional arguments such as message fragments, but the first version shall only require the identifier for diagnostics.

Example future extension:

```dq
//?error(TypeIncompatible, "int", "bool")
```

This is not required for the first implementation.

### 6.5 Runtime output syntax

The initial runtime output syntax shall use a simple custom parser rather than regular expressions.

Examples:

```dq
//?check(Hello)
//?check(Test1Result, 1.234)
//?check(strtest1, "abcd")
//?ignore(Debug line)
```

The framework shall support two forms:

```text
//?check(Text)
//?check(Key, Value)
//?ignore(Text)
//?ignore(Key, Value)
```

In the one-argument form, the argument is matched as literal output text for the full runtime line.

In the two-argument form, the first argument is the expected output key and the second argument is the expected value.

The expected text or value shall be matched against the runtime output as literal text.

`ignore(...)` and `ignoreerr(...)` shall use the same one-argument and two-argument forms and matching rules as `check(...)` and `checkerr(...)`, but matched lines shall be consumed without creating a required expectation.

If text is written in quotes in the directive, the quotes are part of the expected output text and must also appear in the runtime output.

For example:

```dq
//?check(strtest1, "abcd")
```

matches output such as:

```text
strtest1 = "abcd"
```

and does not match:

```text
strtest1 = abcd
```

String values are recommended to be printed with quotes in program output so that string expectations are visually clear and unambiguous.

### 6.6 Runtime output line matching rules

Each `check(...)` expectation shall match one line from the program stdout stream.

Each `checkerr(...)` expectation shall match one line from the program stderr stream.

Each `ignore(...)` directive shall consume at most one line from the program stdout stream.

Each `ignoreerr(...)` directive shall consume at most one line from the program stderr stream.

For a directive of the form:

```text
//?check(Text)
```

the matched runtime line shall exactly equal `Text`.

Example of accepted match for:

```dq
//?check(Hello)
```

```text
Hello
```

For a directive of the form:

```text
//?check(Key, Value)
```

the matched runtime line shall satisfy all of the following:

1. the line shall begin with `Key`
2. zero or more spaces may follow the key
3. an `=` character may optionally appear
4. zero or more spaces may follow the optional `=`
5. the remainder of the line shall exactly equal `Value`

Examples of accepted matches for:

```dq
//?check(Test1Result, 1.234)
```

```text
Test1Result=1.234
Test1Result = 1.234
Test1Result    1.234
```

Example of accepted match for:

```dq
//?check(strtest1, "abcd")
```

```text
strtest1="abcd"
```

The framework shall not require the `check(...)` or `checkerr(...)` expectations to appear in any particular order.

The runner shall consider `check(...)`, `checkerr(...)`, `ignore(...)`, and `ignoreerr(...)` as unordered matches against the set of produced output lines in the corresponding stream.

Within one test file, every `check(...)` directive shall be unique.

Within one test file, every `checkerr(...)` directive shall be unique.

Within one test file, every `ignore(...)` directive shall be unique.

Within one test file, every `ignoreerr(...)` directive shall be unique.

If duplicate `check(...)`, `checkerr(...)`, `ignore(...)`, or `ignoreerr(...)` directives are present in the same test file, the runner shall report this as a test specification error before runtime matching begins.

Any stdout or stderr line that is not matched by a corresponding `check(...)`, `checkerr(...)`, `ignore(...)`, or `ignoreerr(...)` directive shall be considered unchecked output.

The test system shall report unchecked output from the runtime execution.

Unchecked output shall not by itself cause the test to fail unless a stricter mode is added in a later version.

When a runtime variant fails for any reason, the failure report shall include:

- unmatched stdout lines
- unmatched stderr lines
- expected `check(...)` entries that were not matched
- expected `checkerr(...)` entries that were not matched
- matched `ignore(...)` entries when useful for diagnosis
- matched `ignoreerr(...)` entries when useful for diagnosis

### 6.7 Placement of runtime directives

Runtime directives need not be attached to the source line that produces the output.

They may appear:

- on the same line as test code
- multiple times on the same physical line after one `//?` introducer, separated by commas
- on separate lines after the relevant code
- grouped together at the end of the file

The parser shall collect all runtime directives from the full file, regardless of placement.

Example:

```dq
some_test_code();  //?check(Test1Result, 1.234), check(strtest1, "abcd")
```

When more than one test instruction appears in one line, they shall be separated by commas after the same `//?` marker.

The parser shall treat `//?` as the start of an inline expectation list and parse additional directives on that line as comma-separated entries.

---

## 7. Variant Generation Rules

The runner shall decide which test variants to create by scanning the inline markers found in the file.

### 7.1 Error variant trigger

If at least one `//?error(...)` marker is found anywhere in the source file, the runner shall create an **error variant** for that file.

The error variant:

- shall compile the file with `-dERRORTEST`
- shall expect compilation to fail
- shall validate the reported compiler diagnostics against the `//?error(...)`, `//?warning(...)`, and `//?hint(...)` markers that apply to that variant

If the error variant compiles successfully, the error variant fails.

### 7.2 Runtime variant trigger

If at least one runtime marker is found anywhere in the source file, the runner shall create a **runtime variant** for that file.

Initial runtime markers are:

- `//?check(...)`
- `//?checkerr(...)`
- `//?ignore(...)`
- `//?ignoreerr(...)`
- `//?exit(...)`

The runtime variant:

- shall compile the file without `-dERRORTEST`
- shall expect compilation to succeed
- shall run the produced executable
- shall validate the runtime expectations declared by runtime markers

If the runtime variant does not compile, the runtime variant fails immediately.

### 7.3 Combined variants

A single source file may legitimately create:

- only an error variant
- only a runtime variant
- both an error variant and a runtime variant

The presence or absence of `#ifdef ERRORTEST` does not determine whether a variant exists.

Variant creation is determined only by the presence of matching `//?` markers.

### 7.4 Marker interpretation by variant

Markers shall be interpreted only in the variant they belong to.

Specifically:

- In the error variant, runtime markers shall be ignored.
- In the runtime variant, `//?error(...)`, `//?warning(...)`, and `//?hint(...)` markers intended for the error variant shall be ignored.

This rule ensures that a file can contain both compile-error and runtime expectations without semantic conflict.

### 7.5 Source structure

Authors may use a root-level `#ifdef ERRORTEST` / `#else` / `#endif` split to keep the error and runtime source paths fully separated.

This structure is recommended for combined tests, but it is not required by the framework.

Pure error tests may exist without `#ifdef ERRORTEST`, and files may also contain `#ifdef ERRORTEST` without defining both variants.

### 7.6 Reporting

When a file produces more than one variant, each variant shall be reported separately.

Recommended naming:

- `file.dq:error`
- `file.dq:run`

For failed runtime variants, the preserved artifacts and console report shall include the full stdout stream, the full stderr stream, and the subset of output lines that remained unchecked after expectation matching.

---
