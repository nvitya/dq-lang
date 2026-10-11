See the [threads package documentation](../docs/index.md) for the thread,
message-channel, atomic, spinlock, and event APIs.

Run the standalone stress tester from the repository root:

```sh
build/dq-run stdpkg/threads/examples/stress_test.dq
```

It queries the online logical processor count with `ProcessorCount()` and uses that many workers. Defaults are
50,000 messages per producer, a queue capacity of 256 messages, and a 15-second
timeout per scenario. The tester uses `threads.OThread` for workers and the
package's atomic operations for its own start/stop coordination.

Optional positional arguments are worker count, messages per producer, queue
capacity, timeout in seconds, and an optional operation wait timeout in milliseconds:

```sh
build/dq-run stdpkg/threads/examples/stress_test.dq -- 32 100000 256 30
build/dq-run -O3 --lto stdpkg/threads/examples/stress_test.dq -- 32 100000 1 30
build/dq-run -O3 --lto stdpkg/threads/examples/stress_test.dq -- 32 20000 1 30 100
```

The first checks exercise empty/full behavior and FIFO across repeated ring
wraparound, with both one-message capacity and the requested size. Threaded scenarios use:

- One producer and one consumer.
- All but one worker producing, with one consumer.
- One producer, with all but one worker consuming.
- Half the workers producing and the remainder consuming.

Every message has a unique ID and redundant fields to detect torn/corrupted
payloads. Each consumer tracks its own received IDs; after joining, the tester
checks for missing and duplicate messages and exact sent/received totals. With
one consumer it also checks each producer's FIFO order. Failed nonblocking
operations are retried with `SchedYield()`.
The optional fifth argument enables waiting operations instead of nonblocking
attempts; zero is the default. Waiting workers check cancellation on every attempt.

The harness never locks around channel operations. Channel locking is enabled.
All scenarios should pass with channel locking enabled. Two workers
reduce every scenario to one producer and one consumer, useful as a control run.

Exit status is 0 for success, 1 for test failure/timeout, or 2 for invalid
arguments/thread creation failure. On timeout, workers are asked to stop; a
two-second grace period catches deadlocks. Consumer bookkeeping uses roughly
`producers * consumers * messages_per_producer` bytes, so larger worker counts
and message counts can require substantial memory.

Run the simple wait-method tester with:

```sh
build/dq-run stdpkg/threads/examples/wait_test.dq
```

It checks immediate success, full/empty timeouts, unchanged messages on failure,
and success after a delayed worker enqueues or dequeues, with both finite and
indefinite waits. It also checks notifications preceding the kernel wait and
repeated Linux signal interruptions without restarting the deadline, and reports
notification-to-return latency. It uses a queue with capacity one and exits
with status 1 if a check fails.

`PutWaitMillis(message, timeout_ms)` and `GetWaitMillis(message, timeout_ms)`
return `true` on success and `false` on timeout. Zero makes one immediate attempt;
any negative timeout waits indefinitely. Positive timeouts use monotonic time.
See [Waiting and Timeouts](../docs/index.md#waiting-and-timeouts) for the exact
deadline, notification, error, and lifetime rules, and
[Thread Events](../docs/index.md#thread-events) for the reusable `OThreadEvent`
primitive exported by `use threads`.

The work-distribution example compares one worker with progressively larger
worker pools, up to the detected processor count:

```sh
build/dq-run -O3 --lto stdpkg/threads/examples/work_distribution.dq
build/dq-run -O3 --lto stdpkg/threads/examples/work_distribution.dq -- 32 256 1000000
```

Arguments are maximum workers, job count, and samples per job. Every run performs
the same work: deterministic Monte Carlo jobs estimating pi. The main thread is
the sole producer, feeding a bounded queue with capacity 64. Workers use
`GetWaitMillis()`; the producer uses `PutWaitMillis()` for backpressure and sends
one stop message per worker after all jobs. Neither side polls or sleeps.

Elapsed time includes thread creation, dispatch, computation, and joining; result
validation happens afterward. The table reports speedup over one worker and the
range of job counts handled by individual workers. Every job must appear exactly
once, and its result must match the one-worker reference. Use `-O3 --lto` for the timing
comparison; smaller workloads and heavily loaded machines can limit speedup.

All examples also compile for x86_64 Windows. From a Linux host with llvm-mingw:

```sh
DQ_MINGW_ROOT="$PWD/sysroots/llvm-mingw-x86_64" \
  build/dq-comp --target=x86_64-win stdpkg/threads/examples/stress_test.dq
```

Run the resulting `stress_test.exe` on Windows with the same positional arguments.
`make test-windows` runs the lifecycle, atomic, event, and channel checks plus
these wait, stress, and work-distribution examples under Wine at O0 and O3 with LTO.
