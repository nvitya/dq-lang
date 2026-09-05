# Bundled RISC-V bare-metal runtime libraries

These archives were copied unchanged from xPack GNU RISC-V Embedded GCC
14.3.0-1 for Linux x64. The source toolchain was extracted at:

`/lindata/dev_riscv/xpack-riscv-none-elf-gcc-14.3.0-1/`

The compiler's `rv32imac-bare` target uses the `rv32imac_zicsr` ISA and the
`ilp32` ABI. In this xPack release that combination resolves to the default
multilib directory (`.`); DQ stages the selected libraries in
`lib/rv32imac_zicsr`.

The bundled archives are:

- `libgcc.a`
- `libc_nano.a`
- `libnosys.a`
- `libsupc++_nano.a`
- `libm.a`

`licenses/` contains the upstream GCC and Newlib licensing material that
applies to these archives.
