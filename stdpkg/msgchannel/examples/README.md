Run the standalone Linux stress tester from the repository root:

```sh
build/dq-run stdpkg/msgchannel/examples/stress_test.dq
```

It queries the online logical processor count with `get_nprocs()` and uses that
many workers. Defaults are 50,000 messages per producer, 257 ring slots (256
usable), and a 15-second timeout per scenario. The tester uses `threads.OThread`
and requires `libatomic` for channel locking and its own start/stop coordination.

Optional positional arguments are worker count, messages per producer, ring
slots, and timeout in seconds:

```sh
build/dq-run stdpkg/msgchannel/examples/stress_test.dq -- 32 100000 257 30
build/dq-run -O3 stdpkg/msgchannel/examples/stress_test.dq -- 32 100000 2 30
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

The harness never locks around channel operations. Channel locking is enabled;
both blocking flags remain disabled until blocking is implemented. All scenarios
should pass with channel locking enabled. Two workers
reduce every scenario to one producer and one consumer, useful as a control run.

Exit status is 0 for success, 1 for test failure/timeout, or 2 for invalid
arguments/thread creation failure. On timeout, workers are asked to stop; a
two-second grace period catches deadlocks. Consumer bookkeeping uses roughly
`producers * consumers * messages_per_producer` bytes, so larger worker counts
and message counts can require substantial memory.
