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

LLVM cannot attach unwind edges to ordinary loads and stores. On Linux
with exceptions enabled, indirect memory accesses therefore use unwindable
inline assembly containing the corresponding memory instruction. This adds
exception-table coverage without pointer checks, checkpoints, or helper calls
on the normal path. Stack and global storage accesses retain ordinary LLVM
loads and stores. Aggregate accesses are lowered field by field. The unwind
edges constrain optimizations around potentially faulting accesses, as they do
for potentially throwing calls.

Known limitation on x86-64: LLVM 21 and 22 can shrink-wrap an optimized leaf
function's stack setup into its cleanup landing pad, placing the unwind entry
after that setup.
A SIGSEGV unwinding through such a function can therefore fail. Local signal
handling is verified at `-O0` (including the `dq-run` default); optimized signal
recovery is not yet reliable. No normal-path stack setup or checkpoints are
added to work around this backend issue.

## Verification Areas

The exception autotests cover typed matching, propagation and rethrow, finally
on all control transfers, local cleanup, runtime-error catching, top-level
reporting, backtraces, and supported signal handling. Any change to scope cleanup
or object lifetime should run these tests in addition to its local test group.
