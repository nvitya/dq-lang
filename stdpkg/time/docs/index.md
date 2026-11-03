# `time`

`time` provides wall-clock timestamps, monotonic clocks, date/time formatting,
and sleeps for hosted Linux and Windows programs.

```dq
use time
```

## Choosing a Clock

| Function | Return type | Unit | Use |
| --- | --- | --- | --- |
| `Time()` | `float64` | seconds since the Unix epoch<br>(1970-01-01 00:00:00 UTC) | dates, timestamps, and wall-clock deadlines |
| `MilliTime()` | `int64` | milliseconds | elapsed time and monotonic deadlines |
| `MicroTime()` | `int64` | microseconds | elapsed time and monotonic deadlines |
| `NanoTime()` | `int64` | nanoseconds | elapsed time and monotonic deadlines |

`Time()` includes a fractional second. It reads the system wall clock, so clock
corrections can change its value forwards or backwards. Use it when a value must
correspond to a calendar date, such as a log timestamp.

The monotonic clocks have a system-defined origin unrelated to the Unix epoch.
Subtract two readings from the same clock to measure a duration, or add a
duration to form a deadline. Their values are unaffected by wall-clock changes;
do not pass them to the date/time formatters or store them as calendar timestamps.
The unit names describe the returned scale, not a guarantee of clock resolution.

### Measuring Elapsed Time

```dq
use print
use time

func *Main():
    var started : int64 = MicroTime()
    SleepMillis(20) // Replace with the operation to measure.
    var elapsed : int64 = MicroTime() - started
    PrintLn('Elapsed: {} us ({:.3f} ms)', [elapsed, elapsed / 1000])
endfunc
```

The elapsed value can exceed the requested sleep because the operating system
decides when the thread runs again.

## Formatting Dates and Times

All three formatters take the format first and a `Time()`-style timestamp second:

```dq
func FormatTime1(fmt : rostr, atime : float64) -> str ..
func FormatTime2(fmt : rostr, atime : float64) -> str ..
func FormatTimeUtc(fmt : rostr, atime : float64) -> str ..
```

| Function | Time zone | Format language |
| --- | --- | --- |
| `FormatTime1` | system local time | Delphi-style tokens |
| `FormatTime2` | system local time | native `strftime` directives, plus `%f` for microseconds |
| `FormatTimeUtc` | UTC | native `strftime` directives; `%f` is not expanded |

The timestamp itself has no local-time offset. `FormatTime1` and `FormatTime2`
apply the system's local time-zone and daylight-saving rules during conversion.
`FormatTimeUtc` formats the same instant in UTC. None of the formatters accepts
an explicit time-zone argument.

```dq
use print
use time

func *Main():
    var now : float64 = Time()
    PrintLn('Local: {}', [FormatTime1('yyyy-mm-dd hh:nn:ss.zzz', now)])
    PrintLn('Local with microseconds: {}',
            [FormatTime2('%Y-%m-%d %H:%M:%S.%f', now)])
    PrintLn('UTC: {}', [FormatTimeUtc('%Y-%m-%dT%H:%M:%SZ', now)])

    // Fixed timestamp: 2024-01-01 00:00:00 UTC.
    PrintLn(FormatTimeUtc('%Y-%m-%d %H:%M:%S', 1704067200))
endfunc
```

The last line prints `2024-01-01 00:00:00` regardless of the local time zone.
Only append the literal `Z` when formatting UTC.

### Delphi-Style Tokens (`FormatTime1`)

Tokens are case-insensitive. Prefer `nn` for minutes to distinguish them from
the month token `mm`.

| Token | Meaning |
| --- | --- |
| `yy`, `yyyy` | two-digit year, full year |
| `m`, `mm` | month without padding, two-digit month |
| `mmm`, `mmmm` | abbreviated month name, full month name |
| `d`, `dd` | day of month without padding, two-digit day |
| `ddd`, `dddd` | abbreviated weekday name, full weekday name |
| `ddddd` | locale date representation |
| `h`, `hh` | hour without padding, two-digit hour; normally 24-hour time |
| `n`, `nn` | minute without padding, two-digit minute |
| `s`, `ss` | second without padding, two-digit second |
| `z`, `zz`, `zzz` | milliseconds padded to at least one, two, or three digits |
| `am/pm`, `ampm` | locale AM/PM marker; switches hours to 12-hour time |
| `a/p` | `a` or `p` (`A` or `P` for uppercase token); switches hours to 12-hour time |
| `c` | locale date and time representation |
| `t`, `tt` | locale time representation |

`m` and `mm` mean minutes immediately after an hour token, optionally separated
by a colon: `hh:mm:ss` works, but `hh:nn:ss` is clearer. Text inside matching
single or double quotes is copied literally; other characters are copied unless
they form a recognized token. For example:

```dq
PrintLn(FormatTime1('yyyy-mm-dd "at" hh:nn:ss', Time()))
PrintLn(FormatTime1('hh:nn am/pm', Time()))
```

This is the token set implemented by DQ, rather than the entire Delphi formatting
API. Name and locale tokens use the native C library. Unpadded numeric tokens
use native `strftime` `%-` directives, whose support varies by platform; prefer
the padded forms for portable output.

### `strftime` Directives (`FormatTime2` and `FormatTimeUtc`)

| Directive | Meaning |
| --- | --- |
| `%Y`, `%y` | full year, two-digit year |
| `%m`, `%d` | two-digit month, two-digit day of month |
| `%H`, `%I` | two-digit 24-hour hour, two-digit 12-hour hour |
| `%M`, `%S` | two-digit minute, two-digit second |
| `%a`, `%A` | abbreviated weekday name, full weekday name |
| `%b`, `%B` | abbreviated month name, full month name |
| `%p` | locale AM/PM marker |
| `%j` | day of year, padded to three digits |
| `%w` | weekday number, Sunday = 0 |
| `%x`, `%X`, `%c` | locale date, time, date and time |
| `%%` | literal percent sign |
| `%f` | six-digit microseconds; **`FormatTime2` only** |

Other directives and flags are passed to the native C library, so their support
and output can differ between platforms. In particular, `FormatTime2` does not
implement all of Python's date/time API; it adds `%f` to native `strftime`.

Calendar conversion rounds the fractional second to the nearest microsecond,
carrying into the next second when necessary. `FormatTime1` then takes the
millisecond part for `z` tokens. `FormatTimeUtc` outputs no fractional seconds
with the directives above.

An empty format returns an empty string. Conversion failures or a native
formatting result that cannot be produced within the internal buffer limit
also return an empty string; the functions do not return a separate error status.

## Sleeping for a Duration

| Function | Argument type | Unit |
| --- | --- | --- |
| `SleepSecs(asecs)` | `float64` | seconds; fractions are accepted |
| `SleepMillis(ams)` | `int64` | milliseconds |
| `SleepMicros(aus)` | `int64` | microseconds |
| `SleepNanos(ansecs)` | `int64` | nanoseconds |

These functions block the calling thread and return no value. Zero or negative
durations return immediately. `SleepSecs` rounds its duration to nanoseconds.
On Windows, positive durations are rounded up to whole milliseconds before
calling the native sleep API. On Linux, the package uses `nanosleep` and retries
after signal interruptions with the remaining duration.

```dq
SleepSecs(0.05)   // 50 milliseconds.
SleepMillis(50)
SleepMicros(500)
SleepNanos(500000)
```

Actual wake-up time depends on OS timer resolution and scheduling. A nanosecond
argument does not imply nanosecond scheduling accuracy. Keep durations within
the `int64` range after conversion to nanoseconds; conversion does not saturate
on overflow.

## Sleeping Until a Deadline

| Function | Deadline type | Matching clock |
| --- | --- | --- |
| `SleepToTime(asecs)` | `float64` | `Time()` |
| `SleepToMilliTime(ams)` | `int64` | `MilliTime()` |
| `SleepToMicroTime(aus)` | `int64` | `MicroTime()` |
| `SleepToNanoTime(ansecs)` | `int64` | `NanoTime()` |

These functions take an **absolute deadline**, not a duration, and return no
value. Use a reading from the matching clock plus the desired delay:

```dq
SleepToTime(Time() + 0.05)
SleepToMilliTime(MilliTime() + 50)
SleepToMicroTime(MicroTime() + 50000)
SleepToNanoTime(NanoTime() + 50000000)
```

A deadline already reached, or a nonpositive deadline, returns immediately.
Prefer a monotonic clock for timeouts and periodic work. `SleepToTime` follows
wall-clock time, so clock adjustments can affect the wait. Sleep functions do
not expose native errors to the caller; they are not a hard real-time guarantee.

### Periodic Work Without Accumulating Work-Time Drift

Advance the previous deadline by a fixed interval. Sleeping for an interval
after every iteration would add each iteration's work time to the period.

```dq
use print
use time

func *Main():
    const period_ms : int64 = 100
    var deadline : int64 = MilliTime()
    for tick : int = 0 count 5:
        deadline += period_ms
        SleepToMilliTime(deadline)
        PrintLn('Tick {} at {}',
                [tick + 1, FormatTime1('hh:nn:ss.zzz', Time())])
    endfor
endfunc
```

The first tick is scheduled one period after the initial clock reading. If work
takes longer than the period, past deadlines return immediately and this loop
catches up. Applications that should skip missed ticks need an explicit policy
for advancing the deadline past the current time.

## Running the Examples

The complete programs above are also provided in `stdpkg/time/examples/`.
Run them from the repository root:

```bash
build/dq-run stdpkg/time/examples/elapsed.dq
build/dq-run stdpkg/time/examples/formatting.dq
build/dq-run stdpkg/time/examples/periodic.dq
```

With an installed release, use `dq-run` in place of `build/dq-run`.
