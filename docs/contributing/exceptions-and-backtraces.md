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

## Verification Areas

The exception autotests cover typed matching, propagation and rethrow, finally
on all control transfers, local cleanup, runtime-error catching, top-level
reporting, backtraces, and supported signal handling. Any change to scope cleanup
or object lifetime should run these tests in addition to its local test group.
