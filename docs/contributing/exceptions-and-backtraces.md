# Exceptions and Backtraces

> Contributor implementation note: source-level guarantees are defined in
> [Statements and Control Flow](../reference/statements-and-control-flow.md).

## Runtime Model

Hosted DQ exceptions are object references derived from the runtime exception
base type. Polymorphic type information supports handler matching. Raising an
exception transfers control through compiler-generated cleanup and handler
regions.

A raise captures a backtrace before propagation. Re-raising preserves the active
exception context rather than constructing an unrelated exception.

## Generated Cleanup

The compiler tracks initialized locals that own cleanup behavior. Every exit edge
from a protected scope—normal fallthrough, return, loop transfer, or unwind—runs
the required cleanup exactly once in reverse construction order.

`finally` is represented on all exits from its protected region. Contributor
changes must test both normal control flow and exception replacement from within
cleanup/finally code.

## Matching and Top-Level Handling

Handlers are considered in source order. Runtime type-chain checks implement
base/derived matching. If none match, propagation continues.

The hosted top-level handler prints the exception message and captured frames,
then terminates with failure. Signal-to-exception behavior is platform/runtime
specific and must not be generalized to bare targets without an implemented
runtime contract.

On all supported Linux targets (x86-64, ARM64, armhf, and riscv64), SIGSEGV is
converted to `ESegFault` in the signal handler and propagates through the native
unwinder. The handler unblocks SIGSEGV before
throwing, so subsequent faults can also be caught.

On armhf, `sigaction` supplies the saved signal context. After capturing the
backtrace, the handler advances its saved PC by a halfword so ARM EHABI's
return-address lookup includes a fault at the start of an exception-table
range. This adjustment runs only on the signal path.

On Windows x86-64, a vectored exception handler converts access violations to
`ESegFault` and propagates them through the native MinGW SEH unwinder. Local
handlers and owned-scope cleanups run before control continues after `endtry`.
The recursion guard is reset before throwing, allowing subsequent faults to
be caught. A thin wrapper around the MinGW personality advances the faulting
frame's lookup PC by one byte so its return-address lookup includes the faulting
instruction at the start of an exception-table range. The original machine
context and captured backtrace are preserved. Uncaught faults reach the hosted
top-level exception reporter.

LLVM cannot attach unwind edges to ordinary loads and stores. On Linux and
Windows with exceptions enabled, indirect memory accesses therefore use unwindable
inline assembly containing the corresponding memory instruction. This adds
exception-table coverage without pointer checks, checkpoints, or helper calls
on the normal path. Stack and global storage accesses retain ordinary LLVM
loads and stores. Aggregate accesses are lowered field by field. The unwind
edges constrain optimizations around potentially faulting accesses, as they do
for potentially throwing calls.

Known limitation on Linux x86-64: LLVM 21 and 22 can shrink-wrap an optimized leaf
function's stack setup into its cleanup landing pad, placing the unwind entry
after that setup.
A SIGSEGV unwinding through such a function can therefore fail. Local signal
handling is verified at `-O0` (including the `dq-run` default); optimized signal
recovery is not yet reliable. No normal-path stack setup or checkpoints are
added to work around this backend issue.

Windows access-violation recovery is also verified at `-O0`, using Wine; native
Windows execution and optimized fault recovery have not been validated. The
opt-in `tools/test-windows.py` suite runs the shared local-fault and top-level
fault tests, including reads, writes, repeated faults, cleanups, finally,
non-nil protected memory, and normal scalar/aggregate accesses.

## Verification Areas

The exception autotests cover typed matching, propagation and rethrow, finally
on all control transfers, local cleanup, runtime-error catching, top-level
reporting, backtraces, and supported signal handling. Any change to scope cleanup
or object lifetime should run these tests in addition to its local test group.
