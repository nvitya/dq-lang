# `threads`

`threads` provides native threads, atomic values, spinlocks, thread notifications,
and a bounded message channel for hosted Linux and x86_64 Windows programs.

```dq
use threads
```

All the types below are available through this import. The `.dqh` and `.dqi`
files are parts of the `threads` module, not separate modules to import.
On Linux, the package links `pthread` and `atomic`; their libraries must be
available to the linker. `OThread.IsRunning()` uses `pthread_tryjoin_np`, and
`OThreadEvent` and channel waits use futexes. On 32-bit Linux, waits require
`futex_time64` (Linux 5.1 or later).

On Windows, the package uses `_beginthreadex`, interlocked atomic operations,
and `WaitOnAddress`/`WakeByAddressSingle`. It links `kernel32` and
`synchronization` from the llvm-mingw toolchain, without requiring `pthread` or
`libatomic`. Address-based waits require Windows 8 / Windows Server 2012 or later.
The same thread, channel, spinlock, and event APIs work on both platforms.

| Type / function | Purpose |
| --- | --- |
| `OThread` | start and join a native thread |
| `OMsgChannel` | bounded FIFO shared by producers and consumers |
| `SChMsg` | fixed 16-byte channel message |
| `OAtomicUint32`, `OAtomicUint64`, `OAtomicPtr` | atomic value access |
| `OSpinLockVar`, `OSpinLockHolder` | shared lock and scope-based lock holder |
| `OThreadEvent` | notification used when waiting for a shared condition |
| `SchedYield()` | give other runnable threads an opportunity to run |

## Threads

`OThread` starts one thread at a time. Choose a free function, a bound object
method, or the thread object's virtual `Run` method:

| Method | Meaning |
| --- | --- |
| `StartFunc(afunc : FThreadFunc, aarg : pointer) -> bool` | run a free function |
| `StartMethod(amethod : FThreadMethod, aarg : pointer) -> bool` | run a bound object method |
| `StartSelf(aarg : pointer) -> bool` | run this object's `Run(aarg)` |
| `Run(aarg : pointer) -> pointer` | virtual entry point for `StartSelf`; default returns `nil` |
| `Join()` | wait for the thread to finish and collect its return value |
| `IsRunning() -> bool` | try to join without waiting; collect the return value if finished |

The callback types are:

```dq
type FThreadFunc   = func(arg : pointer) -> pointer
type FThreadMethod = func(arg : pointer) -> pointer of object
```

Each start method returns `false` if the wrapper still has an unjoined thread or
native thread creation fails. A successful start sets `thread_id` (a pthread ID
on Linux, a numeric thread ID on Windows); the callback's full pointer result
is available in `retval` after joining. `Join()` on an idle wrapper does nothing.
After a successful join, the same wrapper can start another thread.

```dq
use threads
use print

func ThreadMain(arg : pointer) -> pointer:
    return arg
endfunc

func *Main():
    var value : int = 42
    var thread <- OThread()
    if not thread.StartFunc(ThreadMain, %value):
        Exit(1)
    endif
    thread.Join()
    PrintLn('result = {}', [^int(thread.retval)^])
endfunc
```

For a bound method, use `thread.StartMethod(worker.ThreadMain, arg)`. For a
subclass of `OThread`, override `Run` and call `worker.StartSelf(arg)` as shown
in [the channel worker example](#distributing-work).

`IsRunning()` has a side effect: it reaps a completed thread just as `Join()`
does, including closing the native thread handle on Windows. On Linux it also
returns `false` on try-join errors other than “busy”; on Windows a native wait
error retains the running state. It is not an error-reporting API. `Join()`
returns no status; if the native join fails, the wrapper retains its running state.

Threads share the process address space. The wrapper, bound-method receiver,
and argument storage must remain alive until the thread finishes. `OThread`
does not automatically stop or join from a destructor. Manage each wrapper from
one controlling thread; its lifecycle fields are not synchronized. Join before
destroying a worker or reading results that only that worker writes.

## Message Channels

`OMsgChannel` is a ring-buffer FIFO with a fixed size chosen at construction.
It supports multiple producers and multiple consumers with its default
`locking = true` setting. Each successful `Get` removes one message for one
consumer; it does not broadcast a message to all workers.

### Capacity and Nonblocking Operations

```dq
use threads
use print

func *Main():
    var channel <- OMsgChannel(64) // capacity: 64 messages
    var message : SChMsg = {msg: 1, par.u32: 7, data.u64: 12345}
    if not channel.Put(message):
        Exit(1)
    endif

    var received : SChMsg = {}
    if channel.Get(received):
        PrintLn('message {}, job {}, value {}',
                [received.msg, received.par.u32, received.data.u64])
    endif
endfunc
```

The constructor argument is the **usable message capacity**: `OMsgChannel(n)`
holds at most `n` messages. The channel allocates one additional ring slot
internally to distinguish a full queue from an empty queue. `OMsgChannel(1)`
holds one message; `OMsgChannel(0)` always rejects puts and gets. The argument is
`uint32`; `0xFFFFFFFF` raises `Exception` because the additional slot would
overflow the internal slot count. Allocation must also fit available memory.

| Method | Success | Failure |
| --- | --- | --- |
| `Put(amsg : SChMsg) -> bool` | copy the message into the queue; return `true` | full queue; return `false` without changing queued messages |
| `Get(rmsg : refout SChMsg) -> bool` | remove the oldest message into `rmsg`; return `true` | empty queue; return `false` and leave `rmsg` unchanged |
| `PutWaitMillis(amsg : SChMsg, atimeoutms : int) -> bool` | enqueue, waiting for space if needed | return `false` if waiting ends without space |
| `GetWaitMillis(rmsg : refout SChMsg, atimeoutms : int) -> bool` | dequeue, waiting for a message if needed | return `false` if waiting ends without a message; leave `rmsg` unchanged |

`Put` and `Get` do not wait for queue space or data. They can still contend for
the internal spinlock. FIFO describes the order of successful enqueues and
dequeues; concurrent producers have no predetermined relative order, and
workers can finish processing messages in a different order.

Set `locking` before sharing the channel. Keep it `true` for concurrent use.
Disabling it removes synchronization of the buffer and indices: it requires
single-threaded access or external synchronization of every buffer operation.
The volatile indices alone do not establish a safe lock-free channel, even with
one producer and one consumer.

### Message Layout and Ownership

`SChMsg` has a message identifier, a 4-byte parameter union, and an 8-byte data
union. Choose the union view that matches your own message protocol:

| Field | Available views |
| --- | --- |
| `msg` | `uint32` identifier |
| `par` | `bytes : [4]byte`, `i16 : [2]int16`, `u16 : [2]uint16`, `i32 : int32`, `u32 : uint32`, `f32 : float32` |
| `data` | `bytes : [8]byte`, `i16 : [4]int16`, `u16 : [4]uint16`, `i32 : [2]int32`, `u32 : [2]uint32`, `f32 : [2]float32`, `i64 : int64`, `u64 : uint64`, `f64 : float64`, `ptr : pointer` |

The views of each union overlap; writing `data.u64` replaces the same storage
as `data.f64` or `data.ptr`. There is no automatic payload type tag. Initialize
the structure, for example with `var message : SChMsg = {}`, and agree on which
fields each `msg` identifier uses.

Messages are copied by value. A successful `Put` lets the sender reuse its
local `SChMsg`, but `data.ptr` copies only an address. The channel neither copies
the pointed-to data nor frees it. Keep payload storage alive until the consumer
is done, and define who releases it. If enqueueing fails, no message or payload
has been handed to a consumer. Destroying a channel also does not free payloads
referenced by messages still in the queue.

### Waiting and Timeouts

The timeout argument has the same meaning for both waiting methods:

| `atimeoutms` | Behavior |
| --- | --- |
| `0` | one immediate `Put` or `Get` attempt |
| negative | wait indefinitely until the operation succeeds or the wait fails |
| positive | attempt immediately, then wait using one monotonic deadline |

An available operation succeeds immediately even with a positive timeout.
Failed attempts and signal interruptions reuse the original deadline; they do
not restart the timeout. Very large timeouts saturate the deadline rather than
overflowing it. Scheduling and internal lock contention can delay the return
beyond the requested timeout, so it is not a strict bound on execution time.

Successful enqueues signal a waiting consumer; successful dequeues signal a
waiting producer. Waiters sleep using process-private Linux futexes or Windows
address-based waits. A sequence snapshot taken before checking the queue prevents
a notification between that check and the kernel wait from being missed. A wake
only causes another queue attempt; a different worker may already have taken
the available message or space. Native wait errors other than an interruption
or changed sequence also produce `false`, so the result does not distinguish a
timeout from such an error.

### Distributing Work

This complete example sends jobs to one worker and then sends an explicit stop
message. The worker's result is read after joining:

```dq
use threads
use print

const MSG_WORK : uint32 = 1
const MSG_STOP : uint32 = 2

object OWorker(OThread):
    channel : OMsgChannel
    total : uint64 = 0

    func *Create(achannel : OMsgChannel):
        channel = achannel
    endfunc

    func Run(arg : pointer) -> pointer [[override]]:
        use *
        var job : SChMsg
        while channel.GetWaitMillis(job, -1):
            if job.msg == MSG_STOP:
                break
            endif
            if job.msg == MSG_WORK:
                total += job.data.u64
            endif
        endwhile
        return nil
    endfunc
endobject

func *Main():
    var channel <- OMsgChannel(4) // capacity: four messages
    var worker <- OWorker(channel)
    if not worker.StartSelf(nil):
        Exit(1)
    endif
    var job : SChMsg = {msg: MSG_WORK, par.u32: 0, data.u64: 0}
    for i : int = 1 to 10:
        job.data.u64 = i
        if not channel.PutWaitMillis(job, 1000):
            Exit(1)
        endif
    endfor
    job.msg = MSG_STOP
    if not channel.PutWaitMillis(job, 1000):
        Exit(1)
    endif
    worker.Join()
    PrintLn('total = {}', [worker.total]) // 55
endfunc
```

Waiting on a full queue gives the producer backpressure. To use several workers,
give each the same channel and enqueue one stop message per worker after all
producers have finished enqueueing jobs. FIFO places those stops after the jobs;
join every worker before inspecting its results or releasing the channel.
Processing completion order and distribution among workers are not guaranteed.

There is no channel close, cancellation, or broadcast API. An indefinite wait
needs a cooperating producer or consumer to make progress. Use finite waits if
workers need to check a separate shutdown condition. Do not destroy a channel
while operations or waits are still running. Channels and events operate among
threads in one process, not between processes.

## Atomic Values

Construct an atomic wrapper with its initial value, for example
`var flag <- OAtomicUint32(0)` or `var current <- OAtomicPtr(nil)`.
All three types expose `Load()`, `Store(value)`, and `Exchange(value)`.
`Exchange` replaces the value and returns its previous value. The integer
wrappers also expose `Add(value)`, which returns the value **before** addition;
unsigned overflow wraps.

`Load()`, `Store(value)`, `Exchange(value)`, and `Add(value)` are sequentially
consistent and take no memory-order argument. Loads acquire published data,
stores release earlier writes, and exchanges and additions do both. These
operations also participate in a single ordering across atomic variables.

All three types provide `LoadRelaxed()`. The integer types also provide
`AddRelaxed(value)`, which returns the value before addition and wraps on unsigned
overflow, just like `Add(value)`. For example, use `counter.AddRelaxed(1)` and
`counter.LoadRelaxed()` for independent statistics. Relaxed operations remain
atomic but do not synchronize other shared storage; do not use a relaxed load
of a ready flag to acquire data published by another thread.

On Windows, all these operations use interlocked functions with full memory
barriers, providing stronger ordering for the relaxed methods. Linux uses
the method's specified ordering. Do not rely on the stronger Windows behavior
in portable code.

Each method call is atomic; a sequence of calls is not a single atomic operation.
Use a lock when several accesses must happen together. Atomic pointer publication
still requires separate management of the pointed-to allocation's lifetime.

`OAtomicUint32.Address()` exposes the value's address for native address-based
waits. Keep the wrapper alive while using this address, and access its value
through the atomic methods.

`SchedYield() -> int32`
wraps `sched_yield` on Linux and `Sleep(0)` on Windows; it returns zero on Windows.
Yielding does not synchronize shared data.

## Spinlocks

`OSpinLockVar` inherits the atomic integer constructor. Initialize an unlocked
lock with `OSpinLockVar(0)`. `Lock()` acquires it, yielding while contended;
`Unlock()` releases it. The lock is not recursive and has no timeout or fairness
guarantee. Keep protected sections short.

`OSpinLockHolder(lock)` acquires in its constructor and releases in its
destructor. An embedded local holder provides scope-based release, including
early return and exception unwinding:

```dq
use threads

var lock <- OSpinLockVar(0)
var counter : int = 0

func IncrementCounter():
    var holder <- OSpinLockHolder(lock)
    ++counter
endfunc
```

Every concurrent access to the protected data must use the same lock. The lock
must outlive all holders and users. A holder does not own the lock; explicitly
delete a heap-allocated holder to release it, or use an embedded local as above.

## Thread Events

`OThreadEvent` is a notification primitive used by the channel. It does not
store a job or represent a persistent boolean condition.

| Method | Meaning |
| --- | --- |
| `Snapshot() -> uint32` | read the notification sequence |
| `Signal()` | advance the sequence and wake at most one registered waiter |
| `WaitUntilNanos(asequence : uint32, adeadline : int64) -> bool` | sleep only while the sequence still matches; negative deadline means indefinite wait |

For a finite deadline, pass an absolute monotonic time such as
`NanoTime() + 100000000` for 100 ms, with `use time`.
`true` means recheck the shared condition: the thread woke, the sequence changed,
or a signal interrupted the wait. `false` means the deadline expired or another
native wait error occurred. An already-expired deadline returns `false` before
checking for a changed sequence.

Use the event with a separately synchronized condition:

1. Take a sequence snapshot.
2. Check the condition under its lock or with the appropriate atomic operations.
3. If satisfied, continue with the work.
4. Otherwise, wait using that snapshot and the original absolute deadline.
5. After a `true` result, repeat from the snapshot step.

The thread changing the condition updates it first, then calls `Signal()`.
Taking the snapshot before checking ensures a notification in between checking
and sleeping is observed. A signal before the snapshot does not count as a
pending notification; the shared condition is the source of truth. Keep the
event alive until every waiter and signaler has finished.

## Examples and Checks

The source examples are in
[`stdpkg/threads/examples/`](https://github.com/nvitya/dq-lang/tree/main/stdpkg/threads/examples).
Run these commands from the repository root:

| Example | Shows |
| --- | --- |
| `thread_test.dq` | free-function threads, a `Run` override, completion polling; also includes a bound-method example |
| `wait_test.dq` | immediate, finite, indefinite, and very large waits; unchanged output on failure; notification races and signal interruptions |
| `stress_test.dq` | full/empty queues, FIFO and ring wraparound, one/many producers and consumers, corruption/duplicate/missing-message checks |
| `work_distribution.dq` | a bounded queue feeding a worker pool, stop messages, and deterministic Monte Carlo jobs compared against one worker |

```sh
build/dq-run stdpkg/threads/examples/thread_test.dq
build/dq-run stdpkg/threads/examples/wait_test.dq
build/dq-run stdpkg/threads/examples/stress_test.dq -- 4 50000 256 15
build/dq-run stdpkg/threads/examples/stress_test.dq -- 4 20000 1 15 100
build/dq-run -O3 --lto stdpkg/threads/examples/work_distribution.dq -- 4 256 1000000
```

The stress tester's arguments are worker count, messages per producer, queue
capacity, scenario timeout in seconds, and operation wait timeout in milliseconds.
Defaults are the detected online processor count, 50,000 messages, capacity 256,
15 seconds, and zero (nonblocking operations). Its consumer bookkeeping uses
roughly `producers * consumers * messages_per_producer` bytes. Exit status is
0 for success, 1 for failed checks or timeout, and 2 for invalid arguments or
thread creation failure.

The work-distribution arguments are maximum workers, job count, and samples per
job; defaults are the detected processor count, 256, and 1,000,000. It compares
one worker with progressively larger pools, using the same work each time.
Timing includes creation, dispatch, computation, and joining, with validation
afterward. Use `-O3 --lto` for speed comparisons.

On a Linux host with llvm-mingw and Wine installed, `make test-windows`
cross-compiles and runs thread lifecycle, atomic, event, channel capacity,
wait, stress, and work-distribution checks at `-O0` and `-O3 --lto=full`.
See [Windows application cross-compilation](../../compiler/using-dq-comp.md#windows-cross-compilation-from-linux).
