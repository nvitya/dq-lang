# Adding ARM Cortex-M0 target to the DQ compiler

## DQ Project

An embedded C++ project has a complicated setup:

* Target CPU, CPU capabilities (floating point support)
* Defines
* Include directories
* Source code directories for compile
* Compiler arguments

Most of these setups can be expressed with compiler options.

### Single source, Multi-Target

In VIHAL I have example projects that can be targeted to multiple boards. These boards can have different microcontrollers:

blinkled -> STM32F103, STM32F030, STM32H743, RP2040, RP3540, ATSAME70 etc.

In Eclipse I've created multiple build configurations named after the target boards, like:

BOARD_MIN_F103, BOARD_NUCLEO_F746, BOARD_RPI_PICO, BOARD_DISCOVERY_F746

## Compiler Project Support

The DQ compiler could support multi-target configurations through special files, like `.dqprj`.

### .dqprj format

A `.dqprj` file describes one concrete build configuration. This keeps the DQ
source independent from the target board, so the same main source can be built
through several project files:

```
dq-comp blinkled_nucleo_f746.dqprj
dq-comp blinkled_rpi_pico.dqprj
```

The format uses DQ-like lexical rules and a small declarative grammar. Regular
properties use `property = value`; defines use `define NAME` or
`define NAME = value`:

```
// Nucleo F746 firmware configuration

main       = 'blinkled.dq'
output     = 'build/blinkled_nucleo_f746.elf'
target     = 'arm_m7f-bare'
optlevel   = 2
debuginfo  = true

define BOARD_NUCLEO_F746
define CPU_CLOCK_HZ = 216000000

/*
 * Board-specific linking
 */
linkscript = 'ld/stm32f746.ld'
linkoption = '--gc-sections'
```

Supported value forms:

* bare identifiers, for example `true` and `false`
* integer constants
* single- or double-quoted strings
* a define without a value, which is equivalent to defining it as `true`

Target names must be quoted strings:

```
target = 'arm_m0-bare'
```

This keeps hyphenated target names compatible with DQ-like lexical rules. The
project parser validates the complete string against the supported target list.

The file supports the same `//` and `/* ... */` comments as DQ. Whitespace
around `=` is optional. Statements are normally terminated by a line break;
semicolons may be accepted as optional terminators.

Properties such as `main`, `output`, `target`, `optlevel`, `debuginfo`, and
`linkscript` may occur only once. Repeatable entries include `define`,
`packagepath`, `linkobject`, and `linkoption`. Unknown properties and duplicate
single-value properties are errors. Relative paths are resolved from the
directory containing the `.dqprj` file.

The project file is parsed before target-dependent built-in types are
initialized. Its resolved target and build options are inherited by all child
module compilations; imported modules do not parse the project file again.
Explicit command-line options may override project settings.

## Compiler targets

The first embedded target should be called `arm_m0-bare`:

```
dq-comp --target arm_m0-bare embedded_code.dq
```

Target names are user-facing presets. Internally, each preset resolves to an
LLVM target triple, CPU, feature set, ABI, and data layout. The fully resolved
configuration must be stored in module compatibility metadata.

### Existing targets

The existing processor targets become the following full hosted targets:

* `x64-linux`
* `x64-windows`
* `arm64-linux`

`x64` is the user-facing DQ name for LLVM's `x86_64` architecture, and `arm64`
maps to LLVM's `aarch64` architecture. A future `x32` processor name may be
used for 32-bit x86.

### ARM targets

ARM target names use the `arm_<profile><number>[f]-<platform>` convention:

* `arm_m0-bare`
* `arm_m3-bare`
* `arm_m4-bare`
* `arm_m4f-bare`
* `arm_m33-bare`
* `arm_m33f-bare`
* `arm_m7-bare`
* `arm_m7f-bare`
* `arm_a7-bare`
* `arm_a7-linux`

The `f` suffix selects the hardware floating-point preset. For example,
`arm_m4-bare` means Cortex-M4 without FPU using the soft-float ABI, while
`arm_m4f-bare` enables FPv4-SP-D16 and uses the hard-float ABI by default. A
project may explicitly select `floatabi = softfp` when soft-float
calling-convention compatibility is required.

The final component identifies the platform and runtime. `bare` means a
bare-metal target, while `linux` and `windows` select the corresponding hosted
runtime. `bare` is preferred over LLVM's less descriptive `none` terminology.

### RISC-V targets

RISC-V uses the established ISA shortcuts directly:

| Target | Base ISA and extensions | Default ABI |
| --- | --- | --- |
| `rv32i-bare` | RV32I | `ilp32` |
| `rv32im-bare` | RV32I + M | `ilp32` |
| `rv32imac-bare` | RV32I + M + A + C | `ilp32` |
| `rv32imafc-bare` | RV32I + M + A + F + C | `ilp32f` |
| `rv64gc-bare` | RV64 general-purpose extensions + C | `lp64d` |
| `rv64gc-linux` | RV64 general-purpose extensions + C | `lp64d` |

A concrete CPU may be supplied separately for tuning, and an exceptional ABI
may be selected explicitly:

```
target = 'rv32imac-bare'
cpu    = sifive_e31
abi    = ilp32
```

## Embedded assembly support
```
function _start()  [[export('_start')]]:
    asm:
        ldr    r0, =__stack
        mov    sp, r0

        mov    r0, #0
        b      Main
    endasm
endfunc

function *Main():
    var i : int = 0
    while true:
        i += 1
    endwhile
endfunc

```

Inline assembly with register usage hinting will be added later.
