Run the standalone Linux stress tester from the repository root:

```sh
build/dq-run stdpkg/msgchannel/examples/stress_test.dq
```

It queries the online logical processor count with `get_nprocs()` and uses that
many workers. Defaults are 50,000 messages per producer, 257 ring slots (256
usable), and a 15-second timeout per scenario. The tester uses `threads.OThread`
and requires `libatomic` for channel locking and its own start/stop coordination.

Optional positional arguments are worker count, messages per producer, ring
slots, timeout in seconds, and an optional operation wait timeout in milliseconds:

```sh
build/dq-run stdpkg/msgchannel/examples/stress_test.dq -- 32 100000 257 30
build/dq-run -O3 --lto stdpkg/msgchannel/examples/stress_test.dq -- 32 100000 2 30
build/dq-run -O3 --lto stdpkg/msgchannel/examples/stress_test.dq -- 32 20000 2 30 100
```

The first checks exercise empty/full behavior and FIFO across repeated ring
wraparound, with both two slots and the requested size. Threaded scenarios use:

- One producer and one consumer.
- All but one worker producing, with one consumer.
- One producer, with all but one worker consuming.
- Half the workers producing and the remainder consuming.

Every message has a unique ID and redundant fields to detect torn/corrupted
payloads. Each consumer tracks its own received IDs; after joining, the tester
checks for missing and duplicate messages and exact sent/received totals. With
one consumer it also checks each producer's FIFO order. Failed nonblocking
operations are retried with `sched_yield()`.
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
build/dq-run stdpkg/msgchannel/examples/wait_test.dq
```

It checks immediate success, full/empty timeouts, unchanged messages on failure,
and success after a delayed worker enqueues or dequeues, with both finite and
indefinite waits. It also checks notifications preceding the kernel wait and
repeated signal interruptions without restarting the deadline, and reports
notification-to-return latency. It uses a queue with one usable slot and exits
with status 1 if a check fails.

`PutWaitMillis(message, timeout_ms)` and `GetWaitMillis(message, timeout_ms)`
return `true` on success and `false` on timeout. Zero makes one immediate attempt;
any negative timeout waits indefinitely. Positive timeouts use monotonic time.
Waiting uses Linux `FUTEX_WAIT_BITSET_PRIVATE` with an absolute monotonic deadline,
and successful `Put`/`Get` operations wake one waiter on the opposite operation's
event. No periodic polling or sleeps are used. Sequence counters prevent missed
notifications between checking the queue and entering the kernel wait. The
reusable primitive is `threads/waitevent.OThreadEvent`; its futex words are
process-private, so the channel is for threads sharing one process. Do not destroy
a channel while operations are still running. Scheduling and channel-lock
contention can delay return past the deadline. On 32-bit Linux, waiting requires
the `futex_time64` syscall (Linux 5.1 or later).

The work-distribution example compares one worker with progressively larger
worker pools, up to the detected processor count:

```sh
build/dq-run -O3 --lto stdpkg/msgchannel/examples/work_distribution.dq
build/dq-run -O3 --lto stdpkg/msgchannel/examples/work_distribution.dq -- 32 256 1000000
```

Arguments are maximum workers, job count, and samples per job. Every run performs
the same work: deterministic Monte Carlo jobs estimating pi. The main thread is
the sole producer, feeding a bounded queue with 64 usable slots. Workers use
`GetWaitMillis()`; the producer uses `PutWaitMillis()` for backpressure and sends
one stop message per worker after all jobs. Neither side polls or sleeps.

Elapsed time includes thread creation, dispatch, computation, and joining; result
validation happens afterward. The table reports speedup over one worker and the
range of job counts handled by individual workers. Every job must appear exactly
once, and its result must match the one-worker reference. Use `-O3 --lto` for the timing
comparison; smaller workloads and heavily loaded machines can limit speedup.
