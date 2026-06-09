# Project Instructions

## General

In general try to find the most elegant solution (least code), which is not always the easiest.
- Try to re-use existing functions, or with slight modification
- Avoid repeating code patterns
- Make human readable code
- Add comments when necessary to describe non-trivial solutions
- Put helper behavior on the objects that own the responsibility, avoiding new broad file-static helper clusters.

## Build Defaults

- Use `make -j"$(nproc)"` for build the compiler

## DQ Compiler Testing

- Run the DQ compiler autotests with `make test`.
- Or run them directly with `build/dqatrun -c build/dq-comp -r autotest/tests`.
- single tests can be run like `build/dqatrun -c build/dq-comp autotest/tests/basic/printf.dq` which provides more detailed output

## DQ Single File Run and Debugging

- You can compile and run single files with `build/dq-run file.dq`. This way no autotest markers are required in the file.
- You can compile single file with debugging info using `build/dq-comp -g -O0 file.dq`. And then you can try debugging with `gdb`.
