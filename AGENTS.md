# Project Instructions

## General

In general try to find the most elegant solution (least code), which is not always the easiest.
- Try to re-use existing functions, or with slight modification
- Avoid repeating code patterns
- Make human readable code
- Add comments when necessary to describe non-trivial solutions

## Build Defaults

- Use `cmake --build . -j` for normal builds unless there is a specific reason to limit parallelism.

## DQ Compiler Testing

- Run the DQ compiler autotests with `make test`.
- Or run them directly with `build/dqatrun -c build/dq-comp -r autotest/tests`.
- single tests can be run like `build/dqatrun -c build/dq-comp autotest/tests/basic/printf.dq` which provides more detailed output
