# `varargs`: Variable Arguments

**Status** — Proposed language and DQ ABI extension. The examples below describe the proposed behavior.

## Purpose

Give a DQ function a read-only list of heterogeneous arguments for the duration of a call. The motivating case is `Print("{} {}", [x, y])` on a Cortex-M0. If both values are machine integers, the caller should store two words of payload. Their types are known at compile time and can live in flash. A wrapper should pass the list to another function as cheaply as it forwards a slice.

The PowLog-Lite heartbeat is the baseline. Its existing M0 ELF shows that `TraceLn("hbcounter = {}", [g_hbcnt])` clears stack storage, calls `AnyValSetInt`, copies the record, calls `AnyValMove`, calls the trace function and finally calls `AnyValDestroy`. One `anyvalue` occupies 24 bytes on this target. The formatter only needs the integer and its kind. In contrast, `ODbRows.SetValue` deliberately retains data after its caller returns and copies incoming text into managed storage. Keep the existing `anyvalue` type, its operations and its storage ABI for uses such as database rows. [Current type layout](../compiler/types/otype_anyvalue.h) · [Database row implementation](../stdpkg/db/dbrows.dqi)

## Language specification

### Declaration, construction and forwarding

`varargs` is a built-in parameter type. A formal parameter consumes one argument position, can occur anywhere in a DQ signature and uses the ordinary value-parameter spelling. It has no default value or reference mode. The same rules apply to methods and to parameter types in DQ function references. Only DQ signatures may declare a `varargs` parameter. C interoperability continues to use the existing `...` syntax and target C ABI.

```dq
func TraceLn(fmt : rostr, args : varargs):
    conuart_formatter.Format(fmt, args)
endfunc

TraceLn("ready", [])
Print('value = {}', n)
Print('value = {}', [n])
Print('{} {}', [x, y])
Print('{} {}', x, y)  // Error: too many arguments for Print(fmt, args).
```

- In a `varargs` position, a single supported expression `e` is shorthand for `[e]`. Both spellings capture the same value and use the same descriptor and lowering. This includes a single text argument, even when it contains several characters. Use `[]` for an empty list and brackets for two or more elements. A comma outside brackets starts the next formal argument. The compiler never collects trailing arguments into a list.
- A bracketed list in a `varargs` parameter position may contain zero or more supported expressions of different types. Each expression is evaluated exactly once, left to right, before entering the callee. The format string may be computed at runtime. The compiler applies this rule to any DQ function with the parameter type.
- Passing a `varargs` parameter to another `varargs` parameter forwards the whole list, with its original lifetime. This is an exact parameter match. Writing `[args]` would attempt to nest a list and is invalid. Multiple readers can inspect it independently. Indexing or forwarding leaves the list intact.
- Ordinary overload resolution first considers applicable fixed-type parameters. For example, given overloads taking `int` and `varargs`, a bare `int` selects the integer overload. A bracketed literal likewise prefers a viable concrete array or slice overload. Creating a pack is a fallback for either spelling. Existing ambiguity rules apply to equally ranked candidates. Probe candidates without evaluating or rewriting elements, then lower only the selected call. Ordinary array literals elsewhere retain their usual typing.
- The list and its element views have no writable or addressable DQ storage. Reject declarations of variables, fields, array elements or results of type `varargs`, including through aliases or inference. Reject assignment, address-taking, casts to a storage type, escaping captures and passing the list through C `...`. A diagnostic for attempted storage should point the caller toward copying the required values.

### Accepted elements

| Expression | Value available to the reader |
| --- | --- |
| Signed or unsigned integer | Integer with its signedness and full value. Narrow integers may be widened to a native word. On a 32-bit target, preserve both halves of a 64-bit integer. |
| `float32`, `float64`, `float` | Floating-point value at its resolved source precision. The pack applies no C variadic promotion. |
| `bool`, `char`, `char16`, `wchar` | Boolean, byte character or wide character. Widen `char16` to `wchar` as in current formatting. |
| Text literal, `rostr`, `strslice`, sized or unsized `embstr`, `str` | Read-only text view. Preserve the byte length, including embedded zeros in a length-delimited source. Dynamic strings require a target with string management enabled. |
| `^char` | Zero-terminated text, with the existing DQ text conversion rules. |
| Other typed pointers, `pointer`, `nil` | Pointer value. A nil pointer retains pointer formatting. |

The table defines the complete first-release set for both bare and bracketed arguments. Enums can be passed via `Ord`. For stored `anyvalue` values, arrays (including `[N]T` and `[*]T`), slices, structs, unions, objects, function references and nested lists, the caller must select or convert to a supported element. For example, `Print('{}', samples[i])` can print an integer element. Passing `samples` or `[samples]` is invalid. The same rule applies to `[]anyvalue`. Future revisions can add kinds for arrays or `anyvalue` if a use case calls for them. Diagnostics identify the unsupported element and its type.

### Inspection

Expose `args.length` and zero-based `args[i]`. An indexed element is a transient, read-only receiver for the following operations. It cannot be assigned to an inferred local, returned, addressed or passed as an ordinary value. This keeps the physical representation private and avoids defining a second storable variant type.

| Operation on `args[i]` | Contract |
| --- | --- |
| `IsSInt()`, `IsUint()`, `IsFloat32()`, `IsFloat64()`, `IsBool()`, `IsChar()`, `IsWChar()`, `IsText()`, `IsPointer()` | Inspect the logical kind of the element. `IsText()` covers both text views and zero-terminated `^char`. |
| `AsInt64(def)`, `AsUInt64(def)` | Read a signed or unsigned integer respectively, at full width. |
| `AsFloat32(def)`, `AsFloat64(def)` | Read a float of the corresponding precision. |
| `AsBool(def)`, `AsChar(def)`, `AsWChar(def)`, `AsPointer(def)`, `AsText(def)` | Read the corresponding kind. Text is returned as a borrowed `strslice`. |

Each As-method takes a default of its result type and returns it for a different kind. Formatting conversions, such as rendering a character as hexadecimal, belong to the formatter. An index must satisfy `0 <= i < args.length`. Invalid indices follow the ordinary DQ indexing policy. A default handles a kind mismatch only. Scalar results are ordinary values. Borrowed text follows DQ's text-view lifetime rules. To retain text beyond the call, make an owned copy while the source is valid, for example with `var saved : str = args[i].AsText("")` when dynamic strings are available.

### Lifetime and aliasing

- Capture scalar values at element evaluation. For `[counter, IncrementCounter()]`, the first element holds the earlier value even if the second expression changes the variable.
- A text view borrows bytes. The caller must keep externally borrowed buffers and raw pointees valid through all nested readers. Views into mutable buffers can observe edits to the bytes. A managed `str` supplied directly must keep its evaluated backing storage alive even if a later argument or the callee reassigns its source variable. Keep the owner of a temporary or retain a managed reference where needed. Forwarding extends no lifetime and adds no retain.
- Release acquired owners exactly once after normal return or exception propagation. If a later element or later call argument raises, release only owners successfully acquired so far. Scalar-only lists require no initialization or destruction loop. Disabling dynamic strings must still allow integers, floats, pointers and borrowed text.

## Compiler and RTL representation

Pass the **payload pointer** first and the **descriptor pointer** second as the two components of one DQ argument. This follows the data-first order of DQ array slices. A target may pass the components in registers or on the stack according to its DQ calling convention. Both direct and indirect calls use the same order and representation. The following layout is a proposed private compiler/RTL ABI.

| Form | Descriptor | Payload |
| --- | --- | --- |
| Nonempty list (including a bare singleton) | Read-only descriptor containing the count and one tag byte per element. Intern identical tag sequences within a module. | Consecutive native-word slots. Values with a known constant representation may use read-only storage. |
| Empty list | Shared descriptor with count zero. | May be nil. Readers check the count before touching the payload. |

Encode the element count as one unsigned byte and reject lists longer than 255 elements. Each tag stores the kind in bits 1–7 and sets bit 0 for a two-word payload. The reader advances the payload by `1 + (tag & 1)` native words. A native or narrower scalar and a pointer occupy one word each. Keep wide scalars in their target byte order in consecutive words. `DQVA_STRVIEW` uses two words for the data pointer and byte length. Resolve unknown-length `rostr` when forming this view. `DQVA_PCHAR` uses one word for the zero-terminated pointer and resolves its length when read as text. The RTL module lists the kind codes and payload layouts. [RTL layout](../stdpkg/rtl/varargs.dq)

The payload is native-word aligned and has no padding between slots. On 32-bit targets, read a 64-bit scalar as two 32-bit words in address order. On 64-bit targets, load the scalar directly from its slot. A 64-bit value following one 32-bit word is four-byte aligned on Cortex-M0. Preserve address relocations for pointers in static payloads. Generate payload stores directly into their final slots. Static descriptors contain no per-call values and require no runtime initialization.

An indexed lookup may scan preceding tags to locate a slot. `OVarArgReader` advances a descriptor position and a payload position together. The formatter uses that reader for each successive placeholder. This gives linear total traversal with no offset table in flash. Two independent readers start from the beginning. Forwarding always passes the original two components.

| M0 call | Expected argument preparation |
| --- | --- |
| `Print("{} {}", [1, 2])` | Read-only descriptor and payload. No runtime payload stores. |
| `Print("{} {}", [x, y])` with two live native integers | Two payload words, one store per value, shared descriptor. |
| `TraceLn("hbcounter = {}", g_hbcnt)` or the same call with `[g_hbcnt]` | Identical preparation for both spellings. One payload word and one store for the observed counter. |
| `formatter.Format(fmt, args)` in a wrapper | Forward two components. No element loads, stores or management calls. |

These are code-generation targets for optimized builds. C `printf("%u %u", x, y)` can pass values in argument registers and obtains its type information from the format string. DQ pays for the descriptor and any payload stores so that `{}` works with runtime formats and wrappers. Compare the instruction bytes, peak stack and read-only bytes independently.

## Implementation plan

**Measurement baseline.** Retain disassembly and map measurements for the existing heartbeat before changing the compiler. Prepare small DQ and C comparison cases for constants, one and two live native integers, and one forwarding wrapper. Use matching Cortex-M0 target, optimization and LTO settings. Keep the calls observable so constant folding, inlining or a substituted C library call cannot erase the comparison. Measure caller preparation separately from UART, parsing and number rendering.

1. **Implement the RTL reader in `stdpkg/rtl/varargs.dq`.** Start from the existing placeholder. Use ordinary DQ types for the descriptor and payload pointers, kind codes and reader state so the current compiler can build the module. Provide helpers to read the count, locate an indexed element, advance to the next element, inspect its kind and read its typed value or the supplied default. Keep the word-width calculation and payload decoding in this module so indexed and sequential reads agree. Return text as a borrowed view. Caller-side packing and the lifetime of source expressions belong to the compiler. [RTL helper module](../stdpkg/rtl/varargs.dq)
2. **Test the helpers independently.** Add `autotest/tests/stdpkg/rtl/test_varargs.dq` using hand-built descriptors and word-aligned payloads. Call the module directly and report checks using the autotest runner and C `printf`. The test must run with the current compiler, before introducing the built-in type or changing any formatter signatures. Cover an empty descriptor with a nil payload, a singleton, and a mixed list with different slot widths. Check exact decoded values, defaults on kind mismatch, text with an embedded zero, the final cursor position and two readers traversing the same input independently. On a 32-bit target, include a 64-bit value starting at an odd word offset followed by a sentinel value. This catches both an alignment bug and a reader that advances by the wrong width. Run the focused host test with `build/dq-atrun --clean -c build/dq-comp autotest/tests/stdpkg/rtl/test_varargs.dq`.
3. **Introduce the parameter type and call rules.** Register the built-in alongside the existing types, add a distinct type kind, and implement parameter-position, escape and overload checks. Give the type ownership of contextual packing. Normalize a bare supported expression and a one-element `OArrayLit` to the same pack expression. Reuse parsed list elements before ordinary homogeneous-array conversion. Preserve ordinary call arity and recognize direct forwarding before attempting to pack a value. Provide interface printing, import/export, overload identity and function-reference signature support. Version the module interface/cache as needed so an old formatting signature cannot be linked to the new ABI. Verify an exported wrapper called from a separately compiled module. [Built-ins](../compiler/builtins/scope_builtins.cpp) · [Array conversion](../compiler/types/otype_array.cpp) · [Module interfaces](../compiler/ast/module_intf.cpp)
4. **Emit packs and verify them against the RTL reader.** Match the tested RTL kind codes and widths when interning descriptors, emitting constant payloads and storing live values. Teach all DQ call paths to carry the two components, including methods, indirect calls and inlined wrappers. Connect the language-level queries to the RTL helpers. Test compiler-produced packs in a small DQ consumer that reads their elements and forwards them to a second consumer. Use the same expected values as the raw RTL fixtures to catch ABI disagreements before formatter migration. Start with native integers and the empty list so the M0 cost can be checked before adding managed sources. Verify unaligned wide values, signedness and both 32- and 64-bit layouts. [Call expressions](../compiler/ast/expressions.cpp) · [Compiler type kinds](../compiler/src/rtlint.h) · [RTL definitions](../stdpkg/rtl/rtlint.dq)

   Add RTL-only, read-only properties on `varargs`. `args.data` yields the payload pointer (`^uint`) and `args.desc` yields the descriptor pointer (`^byte`). Lower each property directly to its existing ABI component. RTL code uses both pointers for read-only access within the original call lifetime. The built-in `varargs` representation carries the pair through forwarding, and the reader constructor delegates to the two-pointer overload in ordinary DQ code.

   ```dq
   func *Create(args : varargs) [[overload]]:
       Create(args.data, args.desc)
   endfunc
   ```

   Application code creates a borrowed reader with `var ar <- OVarArgReader(args)`.

5. **Add text views and call-lifetime cleanup.** Reuse DQ's text descriptor conversion and `OCallLifetimeCleanup` for source expressions that acquire owners. Register each acquired owner before evaluating the next expression. Test mutations through aliases, managed temporaries, nested forwarding, exceptions during list construction, and exceptions thrown by the callee. Verify that plain numeric calls emit no cleanup in both exception configurations. [Text lowering](../compiler/types/otype_string.cpp)
6. **Migrate formatting after the reader and compiler tests pass.** Change `OTextFormatter.Format` to consume the tested sequential reader from `rtl/varargs` and dispatch on the decoded kind. Reuse the existing format parser, field writers and sinks. Preserve supported format strings, flags, missing arguments rendered as `null`, ignored extras, character handling and pointer spelling. Exercise 64-bit values outside the native range on 32-bit targets and extend the numeric writer where native-width arithmetic would truncate the value. Migrate `EmbStrAddFmt`, `DynStrAddFmt`, both compiler-recognized `AddFmt` method signatures, `Format`, `Print`, `PrintLn` and formatted exception/shell constructors together. For the application validation build, change the formatted `Trace` and `TraceLn` overloads in both trace configurations. Change formatting wrappers that forward `[]anyvalue` to declare `varargs`. Update callers of migrated functions that supply boxed values to pass the original scalar or text expression, or explicitly extract a supported value. Callers that assemble arbitrary argument arrays at runtime need a separate migration decision. [Formatter](../stdpkg/rtl/textformat.dq) · [Built-in method signatures](../compiler/parser/dqc_parser_expr.cpp) · [Print package](../stdpkg/print/print.dq)
7. **Verify the migration and publish the language reference.** Update the function, formatting and `anyvalue` chapters. Add examples of DQ `varargs` for a call, `anyvalue` for stored values and C `...` for external variadic functions. Rebuild modules, run the clean suite, then rerun without cleaning to exercise interface reuse. Rebuild the firmware and compare like-for-like measurements with the original heartbeat and C baselines.

### Acceptance checks

- Run `make -j"$(nproc)"` and `make test`, followed by `build/dq-atrun -c build/dq-comp -r autotest/tests` for module reuse. Keep the existing `anyvalue` storage and database tests. Add targeted cases under the existing compiler, formatting and module test suites.
- Cover empty and mixed lists, runtime format strings, repeated reads, forwarding across modules, method/function-reference calls, concrete overload selection, invalid element diagnostics and attempts to store or mutate a list. Check that a bare scalar and a bracketed singleton both evaluate a side-effecting expression exactly once. Test single text inputs, fixed-type overload precedence, direct forwarding, and rejection of `Print('{} {}', x, y)` by the two-parameter overload. Require diagnostics for bare and bracketed `anyvalue`, stored arrays and slices, including `[]anyvalue`.
- Cover integer boundaries, both float widths, a missing placeholder argument rendered as `null` and a supplied nil pointer rendered as `^0`, `^char`, embedded zeros, mutable text views, temporary strings, a later argument replacing an earlier managed source, and an exception at each ownership boundary. Run applicable tests with dynamic strings disabled as well as enabled.
- Inspect optimized M0 output for the four cases in the table. Integer-only packing must have zero `AnyVal*` calls, heap operations, reference counting, tag stores and cleanup loops. Compare the bare and bracketed heartbeat disassembly for equivalent instructions and a shared descriptor. Verify one descriptor per distinct tag sequence within the module and no repacking in a separately compiled wrapper. Report caller instruction bytes, stack footprint, descriptor/payload flash bytes and whole-image size alongside the baseline and C comparator. Shared floating-point formatting code in the image is a separate measurement from argument preparation.
