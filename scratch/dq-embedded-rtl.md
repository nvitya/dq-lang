# RTL support for embedded targets

## Recommendation

Treat the CPU target and the runtime feature set as two separate dimensions.
`arm_m7f-bare` should continue to describe the ABI, instruction set, floating
point ABI, and GCC multilib. It should not mean either "tiny RTL" or "full
RTL".

For the first usable target, make the default bare-metal runtime fully featured:

- newlib-nano allocation (`malloc`, `realloc`, `free`) for dynamic strings,
  dynamic arrays, objects, and exception objects;
- zero-cost DQ exceptions using ARM EHABI/libgcc plus the small C++ ABI support
  required by the compiler's current LLVM IR;
- bounded exception messages;
- address-only stack traces, symbolized off-target from the ELF file;
- platform hooks for diagnostic output and fatal termination.

Later add a minimal profile in which heap-backed types and exceptions are
disabled independently. Do not weaken their semantics silently: code which
uses a disabled language feature should fail at compile time with a precise
diagnostic.

## Current failure (2026-08-26)

Reproduced with:

```text
cd /workpr/vihal-dq-tests/uart
dq-comp uart_nucleo_f746.dqproj
```

The run takes about 13 seconds and emits many errors, but the first failure is
an LLVM assertion while regenerating `rtl/strfunc` for `arm_m7f-bare`:

```text
llvm::ICmpInst::AssertOK(): Both operands to ICmp instruction are not of the same type
```

GDB identifies the first invalid comparison as:

```text
stdpkg/rtl/strfunc.dq:297
while i < chars.length
```

Here `i` generates as `i32`, while `chars.length` generates as `i64`.
`OArrayMetaFieldExpr::Generate()` declares the expression as target `int`, but
loads an array-slice length with a hard-coded LLVM `i64`. The errors in
`exception.dq`, `textformat.dq`, and the missing `Mem*` symbols are cascades
after child module regeneration aborts.

This is a target-width compiler bug, not evidence that dynamic strings are too
large for the M7. There are several other hard-coded `i64` array/index values
in `expressions.cpp`, so the first milestone needs a target-width audit rather
than a one-line special case.

There are additional runtime issues which will appear after this compiler bug
is fixed:

- `rtl/backtrace.dq` selects Windows on `WINDOWS` and Linux for every other
  target, so bare metal currently pulls in `backtrace_linux`.
- `rtl/rtl_bare.dq` does not install `rterror_handler`. Consequently bounds,
  allocation, and other `RuntimeError()` calls return silently on bare metal,
  often leaving an operation partially completed.
- DQ exception IR currently references `__cxa_allocate_exception`,
  `__cxa_throw`, `__cxa_begin_catch`, `__cxa_end_catch`, `__cxa_rethrow`,
  `_ZTIPv`, and `__gxx_personality_v0`.
- The bare linker currently supplies newlib-nano and libgcc only. The bundled
  `libgcc.a` has the ARM unwinder and `_Unwind_Backtrace`, but the C++ ABI
  entry points and `__gxx_personality_v0` are supplied by `libsupc++.a` (or
  `libsupc++_nano.a`), which is not yet linked.
- The currently generated native exception wrapper passes no destructor to
  `__cxa_throw`. A newly raised DQ `Exception` can therefore leak after it is
  caught. Exception ownership must be tested and fixed as part of exception
  enablement.
- Exception printing uses hosted `stdio`. The VIHAL bare-metal `_write` stub
  returns success without output, so an uncaught exception would currently be
  invisible even if unwinding worked.

## Runtime architecture

Avoid growing `rtl_bare.dq` into a large platform-specific module. Keep the
language-facing modules stable and put the varying behavior behind narrow
providers owned by the relevant subsystem.

Suggested separation:

| Concern | Stable language-facing API | Bare full provider | Minimal/custom provider |
|---|---|---|---|
| Memory | `rtl/mem` | newlib-nano allocator | user allocator, fixed arena, or disabled heap |
| Exceptions | `rtl/exception` | ARM EHABI + C++ ABI support | disabled, or a future explicit alternative |
| Backtrace | `rtl/backtrace` | `_Unwind_Backtrace`, raw PCs | no-op (`count = 0`) |
| Diagnostics | small write/panic hooks | board/UART override, weak fallback | board-defined or silent panic loop |
| Startup | `DqRuntimeInit()`/module init contract | install runtime-error and panic hooks | initialize enabled providers only |

The provider choice must be made by the compiler/build configuration, not by
an `#else` which treats every non-Windows platform as Linux. The chosen runtime
capabilities are ABI/build inputs and must be included in both the DQM interface
freshness metadata and the artifact build tag/fingerprint. Otherwise full and
minimal package artifacts can be mixed accidentally.

### Proposed project-level configuration

Start with one simple profile property and retain room for expert overrides:

```text
runtime = 'full'       // recommended default for arm_m7f-bare initially
```

Conceptually it expands to:

```text
dynamic_memory = true
exceptions     = true
backtrace      = 'addresses'
diagnostics    = 'platform'
```

Later:

```text
runtime = 'minimal'
```

would mean no heap, no exceptions, no backtrace, and only a fatal runtime-error
hook. If individual overrides are added, validate dependencies; for example,
the current exception object allocation requires dynamic memory, while address
backtraces require the unwind runtime and unwind tables.

The exact property names can wait. The important point is that profiles expand
to explicit capabilities rather than becoming a second set of CPU target names
such as `arm_m7f-full-bare`.

## Dynamic memory and strings

The current `rtl/mem` boundary is already close to the correct abstraction.
Dynamic strings should continue to depend on `MemAlloc`, `MemRealloc`, and
`MemFree`, not directly on board symbols.

For the F746 test, newlib-nano is a reasonable first allocator. VIHAL already
provides `_sbrk`, and the linker script defines the heap from `__end` to
`_Heap_Limit`. In `STM32F750x8_ram.ld`, normal data/heap/stack use the 64 KiB
DTC RAM region, with a reserved 1 KiB main stack. Code is placed in a separate
192 KiB code RAM region. This is enough for functional testing, although heap
and stack collision still needs telemetry or a guard for production use.

Required behavior:

- define a single, documented out-of-memory path;
- make allocation failure non-returning when exceptions are enabled (raise an
  allocation/runtime exception), or invoke the fatal hook when they are not;
- never continue a string/array mutation after a failed allocation;
- allow the application to replace the allocator without editing the RTL;
- document interrupt/thread safety. The initial allocator and exception state
  should be main-context only unless protected by the platform.

Disabling dynamic strings later must not disable `strview`, string literals, or
bounded `cstring`; those are useful heap-free types. It should disable owning
`str` operations and other heap-owning facilities with compile-time errors.

## Exceptions on Cortex-M

The shortest path to correct DQ exception semantics is to retain LLVM's
zero-cost `invoke`/`landingpad` model and use the ARM Exception Handling ABI.
The linker script already preserves `.ARM.exidx`, and bundled `libgcc.a`
contains the language-independent ARM unwinder. For the initial full profile,
bundle the matching multilib `libsupc++_nano.a` (or an intentionally
extracted, tested subset) and link it together with libgcc/newlib-nano.

This is preferable for the first implementation to writing a DQ personality
routine or an unwinder. The compiler already emits C++ ABI calls, and DQ does
its own exception type matching after catching the pointer payload. Once it is
working, measure what `--gc-sections` actually retains. Only then decide whether
a DQ-specific ABI shim would save enough code/RAM to justify its maintenance
and correctness cost.

Exception completion criteria include more than successful linking:

- caught and uncaught exceptions;
- derived-to-base matching and catch-all;
- rethrow;
- nested try/except/finally;
- cleanup of owned locals on every unwind edge;
- exception object release exactly once after catch/rethrow;
- exception during module initialization;
- out-of-memory while creating or formatting an exception;
- uncaught-exception platform action;
- exception from interrupt context is either explicitly unsupported or has a
  separately designed policy.

For the low-resource profile, initially reject `raise`, `try`, and `except`
when exceptions are disabled. `finally` without exception handling can in
principle remain as structured cleanup, but that should be decided based on
the language grammar and lowering. Do not implement exceptions with plain
`setjmp`/`longjmp`: it would bypass the owned-local cleanup semantics unless
the compiler gained a different, explicit cleanup mechanism.

## Stack backtrace on Cortex-M

A useful backtrace is possible, but it should mean something different from
the hosted implementation:

1. Capture a bounded list of program counters using `_Unwind_Backtrace` and the
   `.ARM.exidx` unwind tables already needed by ARM EHABI.
2. Store the addresses in the exception object (prefer a smaller configurable
   default than 32 frames for embedded).
3. Emit raw addresses through the platform diagnostic hook.
4. Symbolize them off-target using the exact unstripped ELF, for example with
   `llvm-addr2line`/`arm-none-eabi-addr2line` or a future DQ helper command.

Do not attempt to carry DWARF parsing, filenames, and symbol names in the
firmware initially. That consumes code and RAM for little benefit. Frame-pointer
walking is a possible tiny diagnostic mode, but it is less reliable across
optimized code, hand-written assembly, interrupt frames, and code which does
not preserve a uniform frame chain. The unwind-table implementation is the
right full-profile baseline.

Backtrace capture is diagnostic, not necessary for exception propagation. It
must therefore be independently switchable and failure to capture must leave a
valid exception with `backtrace_count = 0`.

Hardware faults are separate from DQ exceptions. Converting HardFault,
MemManage, or BusFault into a catchable language exception is unsafe in the
general case. Initially, capture the stacked Cortex-M fault frame plus fault
status registers, print/store a crash record, and terminate/reset. Do not unwind
and continue from arbitrary hardware faults.

## Proposed implementation sequence

### Phase 0: make 32-bit RTL compilation trustworthy

Status (2026-08-26): implemented. Slice descriptors and collection indexes now
use the target-native integer width, including adjacent string/cstring index
paths. The ARM compile-only regression covers fixed arrays, slices, dynamic
array conversions, string/cstring indexing, and text formatting. A failed
child-module regeneration now aborts the current parse after its root error.
The UART graph reaches linking without an LLVM assertion; its remaining
`-lbacktrace` failure belongs to the provider/link work below.

- Fix the slice-length `i64`/target-`int` mismatch.
- Audit array/slice/index/size code generation for hard-coded `i64` and use the
  target-native integer type consistently.
- Add small ARM compile-only tests for each affected expression before trying
  the full UART project.
- Stop child-module failure cascades after the first regeneration failure so
  the compiler reports one useful root error quickly.

Exit criterion: `rtl/strfunc`, `rtl/textformat`, `rtl/exception`, and the UART
module graph all compile for `arm_m7f-bare` without LLVM assertions.

### Phase 1: full heap-backed bare-metal RTL

- Select a real bare provider for memory/runtime initialization.
- Keep newlib-nano as the default allocator for the M7 full profile.
- Install a bare runtime-error handler before application/module code can use
  managed facilities.
- Make OOM behavior deterministic.
- Run dynamic string/array tests on the host and compile them for every 32-bit
  bare target; run a focused suite on the F746 hardware.

Exit criterion: allocation, grow/reallocate, copy-on-write, destruction, OOM,
and repeated allocation/free work on the F746 without heap corruption.

### Phase 2: exceptions without backtrace

- Bundle/link the matching `libsupc++_nano` multilib for the enabled exception
  profile.
- Verify LLVM's generated ARM EHABI sections and all required C++ ABI symbols.
- Correct exception object ownership/destruction.
- Add a top-level bare exception boundary or an explicit application wrapper
  contract so uncaught exceptions always reach `DqPanic`.
- Keep exception message printing behind the diagnostic sink.

Exit criterion: the exception semantic test matrix passes on an emulator if
available and on the F746, with no leaks across a repeated throw/catch loop.

### Phase 3: address backtraces and fault records

- Implement bounded `_Unwind_Backtrace` capture for ARM bare metal.
- Add host-side address symbolization from the unstripped ELF.
- Add a platform crash/fault hook and a Cortex-M stacked-register record.
- Measure code size, static RAM, maximum observed stack use, and per-throw heap
  use with backtrace off and on.

Exit criterion: a deliberately nested exception reports addresses that resolve
to the expected DQ call chain; a deliberate HardFault produces a useful crash
record without pretending it is recoverable.

### Phase 4: optional low-resource profiles

- Add the explicit capability/profile model to project parsing, artifact tags,
  DQM compatibility checks, and diagnostics.
- Add compile-time feature-use rejection.
- Provide heap-free runtime error/panic behavior and no-op backtrace provider.
- Use link-time garbage collection in all profiles and publish size budgets
  based on measured minimal programs.

Exit criterion: full and minimal builds cannot share stale incompatible module
artifacts; minimal programs contain no allocator, C++ EH ABI, or unwind-table
payload unless requested.

## Test and measurement plan

Use three levels of validation:

- Compiler regression: compile-only tests across M0/M3/M4/M4F/M33F/M7F for
  target-width expressions and feature diagnostics.
- Link inspection: no unresolved ABI calls, correct `.ARM.exidx` retention,
  archive/provider provenance, and map-file/`size` budgets.
- Hardware tests on F746: dynamic strings, OOM, nested exception cleanup,
  repeated throw/catch ownership, uncaught panic output, address backtrace, and
  fault record.

Record at least text size, read-only unwind table size, `.data`, `.bss`, heap
high-water mark, and stack high-water mark. “64 KiB configured RAM” is likely
enough, but the decision should be based on these numbers, especially because
the exception object currently reserves 32 pointers plus a 127-character
message before allocator and unwinder overhead.

## Decisions to make after the first measurements

- Whether full should remain the default for all bare CPUs or only for selected
  boards/projects.
- Whether to bundle `libsupc++_nano.a`, package a verified subset, or eventually
  implement a DQ-specific personality/C++ ABI shim.
- Default embedded backtrace depth (8 or 16 is likely more useful than 32).
- Whether the default diagnostic hook should be weak no-op, semihosting, or a
  mandatory board implementation.
- Whether exception/allocator state must support an RTOS/TLS model. The first
  implementation should state clearly that it is single-threaded/main-context
  if this is not yet implemented.
