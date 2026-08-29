# DQ Autotest V2

## Uncovered cases

The current autotest runner handles two main kinds of tests:

- expected compiler diagnostics marked with `//?error`, `//?warning`, or
  `//?hint`;
- native executables whose standard output is checked with `//?check` and
  `//?ignore`.

The following compiler tests therefore require special commands in the CMake
`test` target instead of being ordinary autotests:

- successful compile-only and link-only tests;
- per-test compiler arguments, including target selection and command-line
  overrides of project properties;
- Cortex-M output, which cannot be executed natively;
- compiling one shared source module for several architectures;
- projects which link an existing object file;
- checks of compiler output, such as the LLVM attributes emitted for `-Os` and
  `-Oz`;
- checks of generated artifacts, such as ensuring that an object does not
  reference dynamic-string or unwind symbols.

The current external ARM tests cover successful compilation for the supported
Cortex-M targets, M7 inline assembly, pointer constants, target-dependent
`float`, native-width collections, exception and dynamic-string settings,
bare-metal linking, LTO, bundled runtime symbols, and ARM project files. Most
of these tests only require a successful compiler or linker exit code. A few
use CMake scripts and `nm` to inspect compiler output or generated symbols.

The current runtime path also does not verify the executed program's exit code.
V2 should verify it with an explicit expectation.

## Test inputs and discovery

The autotester should scan both `.dq` and `.dqproj` files. Each discovered file
is an independent test descriptor:

- a `.dq` descriptor is passed directly to the compiler;
- a `.dqproj` descriptor is passed to the compiler as a project;
- autotest markers are read from the descriptor itself;
- a project filename does not have to match its `main` source filename.

This allows several project files to compile the same source module with
different targets or settings. A shared `.dq` module under the scanned test
root can use `//?notest`, or it can be placed outside that root.

Project files already support comments, so autotest markers can be embedded in
them without changing the project-file syntax.

## Successful build-only tests

Introduce the marker:

```text
//?build_only
```

It means:

- invoke the compiler;
- require a zero compiler exit code;
- do not execute the resulting artifact.

The marker covers both compile-only and link-only tests. Whether the compiler
produces an object or a linked image is controlled by compiler arguments or
the `.dqproj` properties.

`//?build_only` should not be combined with runtime-output markers such as
`//?check` or `//?ignore`. Expected diagnostic markers may continue to select
the separate error-test variant.

## Runtime exit code

Introduce:

```text
//?exitcode(3)
```

The marker requires the executed program to terminate normally with exactly
the specified exit code. It also creates a runtime variant when there are no
`//?check` or `//?ignore` markers, so a silent program can be tested solely by
its result:

```dq
//?exitcode(2)

function *Main() -> int:
    return 2
endfunc
```

A descriptor may contain at most one `//?exitcode()` marker. It cannot be
combined with `//?build_only`. For portability, the expected value should be
an integer from 0 through 255.

Runtime tests without an explicit `//?exitcode()` should expect exit code zero.
A timeout, signal, or failure to start the executable is a runtime failure and
must not be treated as a matching exit code.

## Per-test compiler arguments

Introduce:

```text
//?compargs('--target=arm_m7f-bare', '--no-use-sys', '-c')
```

Each argument is passed as one literal compiler argument. The directive must
not be interpreted as a shell command. Quoted arguments make paths, spaces,
defines, and linker arguments unambiguous.

`//?compargs()` should be accepted in both `.dq` and `.dqproj` descriptors. A
descriptor may contain multiple `//?compargs()` directives. Their arguments
are appended into one compiler argument list in source order:

```text
//?compargs('--target=arm_m7f-bare', '--no-use-sys')
//?compargs('-O1', '--lto', '-c')
```

The example above is equivalent to one directive containing all five
arguments. The directives do not create separate test variants. Conflicting
arguments are passed to the compiler in their assembled order and are handled
according to normal compiler command-line rules.

Normal build configuration should generally be expressed as project
properties. The directive is especially useful for direct `.dq` tests and for
testing command-line overrides of project properties.

Runner-owned arguments, such as a unique output path and build suffix, must be
kept separate from the test-supplied arguments. The precedence of a global
runner optimization override, such as `dqatrun -O0`, must be defined
explicitly.

## Cortex-M and multiple architectures

A Cortex-M source can be tested by compiling it through a build-only project:

```text
// pointer_m0.dqproj
//?build_only

main = 'pointer_const.dq'
target = 'arm_m0-bare'
link = false
```

```text
// pointer_m7f.dqproj
//?build_only

main = 'pointer_const.dq'
target = 'arm_m7f-bare'
link = false
```

Both projects reuse the same DQ module. This is simpler and clearer than
adding a multi-architecture variant syntax to one test file.

Successful bare-metal linking can be tested the same way with `link = true`.
Executing Cortex-M output is a separate problem: a compiler target specifies
the CPU and ABI, but not a board, memory map, or observable runtime
environment. Runtime execution would require a board-specific project and a
future emulator or hardware-runner facility.

## Host platform and target architecture

The supported developer operating system is Linux. V2 therefore does not need
a general operating-system skip marker, but it should support a narrow host
architecture filter:

```text
//?hostarch('x64')
```

The marker describes the architecture of the machine running `dqatrun`, not
the target selected for `dq-comp`. The runner detects its host architecture at
startup. Initially, the only recognized identifiers are:

- `x64` for x86-64;
- `arm64` for AArch64.

If the detected host architecture does not match the marker, the descriptor is
skipped before any compiler invocation or executable is started. An unknown
architecture identifier makes the descriptor invalid. A descriptor may
contain at most one `//?hostarch()` marker.

Tests without `//?hostarch()` run on every supported host architecture. The
marker should be used only when the test genuinely depends on native host
behavior; target-specific compile-only coverage should continue to select its
compiler target explicitly instead.

Tests should distinguish native execution from target-specific compiler
coverage:

- portable runtime tests omit an explicit target and use the compiler's native
  Linux target, whether the host architecture is x86-64, AArch64, ARM, or
  RISC-V;
- architecture-specific compiler and linker tests explicitly select their
  target in `//?compargs()` or a `.dqproj` and normally use `//?build_only`;
- native DQ driver tests may invoke the compiler themselves to build and
  inspect explicitly targeted output;
- host-native architecture tests which cannot be made portable use
  `//?hostarch()`.

Requesting an unsupported or misspelled target is a test failure, not a reason
to skip the test. Tests must only request targets which the tested compiler is
expected to support. This avoids silently hiding target-support regressions.

At present, `dq-comp` accepts the configured native hosted target and the
explicit Cortex-M bare presets; it does not generally provide cross-target
hosted Linux presets. A hosted architecture-specific test may therefore pair
its explicit compiler target with the matching `//?hostarch()` marker. For
example, an AArch64 compiler build will skip an x86-64-host test instead of
being required to accept `--target=x86_64-linux`.

An architecture-specific runtime test can run only when its output is native
to the current developer machine. Such tests should normally be written as
portable native tests and exercise the appropriate implementation selected by
the compiler. Running a deliberately non-native hosted target would require a
future emulator facility.

## Output isolation

Generated artifact names for `.dqproj` tests should be based on the project
descriptor, not on its `main` source file. Several projects may share the same
main module.

Every test and variant should also receive a unique build suffix or build root.
Different target names already separate many Cortex-M artifacts, but projects
for the same target with different defines or runtime settings must not race
when the autotester runs in parallel.

Driver tests which launch further processes must give their child artifacts
unique paths and clean them up. The `dqautotest` package should provide helpers
for this, but the runner does not coordinate child artifacts created by
independently scheduled tests.

## DQ driver tests

Specialized compiler tests should be ordinary native `.dq` programs. The DQ
program launches the compiler or another developer tool, captures its result,
performs the required analysis, and returns a nonzero exit code on failure.

This replaces both built-in runner analyzers and special shell-test files. No
`//?analyser()`, `//?shell()`, `.sht`, or general prerequisite marker is
required.

A driver test is run by `dqatrun` like any other runtime test:

```dq
//?exitcode(0)

use dqautotest

function *Main() -> int:
    var run = RunCompiler([
        '--target=arm_m7f-bare',
        '--no-use-sys',
        '-Os',
        '-ir',
        '-c',
        SourcePath('arm_m7f_pointer_const.dq')
    ])

    ExpectExitCode(run, 0)
    ExpectContains(run.output, 'optsize')
    return TestResult()
endfunc
```

The example API is illustrative; the helper implementation may adapt it to
the available DQ collection and string types.

The same mechanism covers the existing special cases:

- invoke `dq-comp` with `-Os` or `-Oz` and inspect captured IR text;
- build an object, invoke `nm`, and check that forbidden symbols are absent;
- invoke the compiler several times and check expected diagnostic identifiers;
- build a `.obj` fixture before compiling a project which uses `linkobject`;
- launch the C++ inline-assembly and project-file unit-test executables and
  require exit code zero.

The Python conversion check was a one-shot migration aid and is not part of
the compiler test suite. It should be removed from `make test` rather than
wrapped in a DQ driver.

## The `dqautotest` package

The test helper package is located at:

```text
autotest/packages/dqautotest/dqautotest.dq
```

`dqatrun` should add `autotest/packages` to the compiler package paths so that
driver tests can simply write:

```dq
use dqautotest
```

The package should own reusable test-driver behavior rather than adding
test-specific cases to the runner. A minimal interface will likely include:

- launching the compiler with an argument vector;
- launching another developer tool with an argument vector;
- optionally launching a shell command when shell behavior is genuinely
  required;
- capturing exit code, stdout, and stderr;
- checking exit codes and text presence or absence;
- locating source files and creating unique temporary artifact paths;
- accumulating failures and returning the final test exit code.

An argument-vector process API should be preferred over a shell command. It
avoids quoting problems and allows stdout and stderr to be captured reliably.
The initial Linux implementation may use libc process facilities. Generic
process-running behavior may later move into the standard DQ library, while
compiler-test expectations and paths remain in `dqautotest`.

The helper should print detailed diagnostics only when an expectation fails.
The DQ driver then returns nonzero. `dqatrun` reports the exit-code mismatch
and includes the driver's captured stdout and stderr; it does not need to
understand the inner compiler diagnostic format.

## Runner environment

DQ driver tests must know which compiler and developer tools belong to the
current test run. `dqatrun` should provide this through child-process
environment variables:

```text
DQ_AUTOTEST_COMPILER
DQ_AUTOTEST_BINDIR
DQ_AUTOTEST_TEST_ROOT
DQ_AUTOTEST_TEST_FILE
DQ_AUTOTEST_TEST_DIR
DQ_AUTOTEST_HOSTARCH
```

The paths should be absolute. `DQ_AUTOTEST_COMPILER` is the compiler selected
with the runner's `-c` option. `DQ_AUTOTEST_BINDIR` is its containing directory
and allows developer tests to locate helper executables built beside it. The
test file and directory identify the current descriptor, while the test root
and host architecture describe the complete run.

Environment overrides must be attached to each child process. The parallel
runner must not implement them by modifying the process-wide environment.
Ordinary runtime tests inherit these variables but do not need to use them.
Using environment variables instead of additional program arguments preserves
the existing `ArgCount()` behavior.

Driver tests which launch compiler processes may require longer than the
current runtime timeout. The runner must use a suitable timeout for these tests
and always report captured output on timeout.

## Still Un-Resolved

The V2 plan above does not yet cover the following cases:

- Executing non-native output. A hosted target would need an emulator, while a
  Cortex-M runtime test additionally needs a board model, memory map, startup
  behavior, and an emulator or hardware runner.
- Developer-only DQ drivers which invoke C++ unit-test executables cannot run
  from a packaged autotest tree unless those executables are packaged too, or
  the developer-only drivers are kept outside the packaged test root.
- The availability and location of optional external tools used by a driver
  test must be part of that test's environment contract. Missing required
  tools should fail clearly rather than silently skip the analysis.

These should remain explicit open design questions rather than being hidden in
individual CMake commands.
