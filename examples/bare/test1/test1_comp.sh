#!/bin/bash
set -e

VIHAL=/workvc/vihal-dq

../../../build/dq-comp --target=arm_m7f-bare -O0 test1.dq

clang++ --target=thumbv7em-none-eabihf -fuse-ld=lld -nostdlib \
  -Xlinker "--library-path=$VIHAL/core/ld" \
  -Xlinker "--script=$VIHAL/armm/stm32/f7/STM32F750x8_ram.ld" \
  .dqbuild/arm_m7f-bare/local/test1.o \
  -o test1.elf

llvm-size --format=sysv --radix=16 test1.elf
