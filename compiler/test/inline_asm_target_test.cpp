/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    inline_asm_target_test.cpp
 * authors: nvitya
 * created: 2026-08-10
 * brief:   inline assembly target descriptor tests
 */

#include <print>
#include <string_view>

#include "inline_asm_target.h"

static int error_count = 0;

static void Check(bool condition, string_view description)
{
  if (condition) return;
  print("Inline ASM target test failed: {}\n", description);
  ++error_count;
}

static void CheckTarget(string_view arch, EInlineAsmDialect dialect,
                        string_view integer_constraint, string_view float_constraint)
{
  const OInlineAsmTarget * target = InlineAsmTargetForArch(arch);
  Check(target, string("target lookup: ") + string(arch));
  if (!target) return;
  Check(target->dialect == dialect, string("dialect: ") + string(arch));
  Check(target->RegisterConstraint(false) == integer_constraint,
        string("integer constraint: ") + string(arch));
  Check(target->RegisterConstraint(true) == float_constraint,
        string("float constraint: ") + string(arch));
}

static void CheckClobber(const OInlineAsmTarget & target, string_view name, bool valid)
{
  Check(!target.ClobberConstraint(name).empty() == valid,
        string("clobber ") + string(name));
}

int main()
{
  CheckTarget("x86_64", INLINE_ASM_DIALECT_INTEL, "r", "x");
  for (string_view arch : {"arm_m0", "arm_m3", "arm_m4", "arm_m4f", "arm_m33",
                           "arm_m33f", "arm_m7f", "arm_m7fd", "arm", "arm_a7", "arm_a9"})
  {
    CheckTarget(arch, INLINE_ASM_DIALECT_NATIVE, "r", "w");
  }
  for (string_view arch : {"arm64", "aarch64"})
  {
    CheckTarget(arch, INLINE_ASM_DIALECT_NATIVE, "r", "w");
  }
  for (string_view arch : {"rv32i", "rv32imac", "riscv32", "rv64g", "riscv64"})
  {
    CheckTarget(arch, INLINE_ASM_DIALECT_NATIVE, "r", "f");
  }
  Check(!InlineAsmTargetForArch("x86"), "unsupported x86");
  Check(!InlineAsmTargetForArch("unknown"), "unsupported unknown target");

  const OInlineAsmTarget & x86 = InlineAsmTargetX86();
  for (string_view name : {"memory", "flags", "rax", "spl", "r8", "r15b", "xmm31", "k7"})
  {
    CheckClobber(x86, name, true);
  }
  for (string_view name : {"r7", "r16", "r8q", "xmm32", "k8", "RAX"})
  {
    CheckClobber(x86, name, false);
  }

  const OInlineAsmTarget & arm = InlineAsmTargetArm();
  for (string_view name : {"memory", "flags", "r0", "r15", "sp", "lr", "pc", "s31", "d31"})
  {
    CheckClobber(arm, name, true);
  }
  for (string_view name : {"r16", "s32", "d32", "q0", "R0"})
  {
    CheckClobber(arm, name, false);
  }

  const OInlineAsmTarget & aarch64 = InlineAsmTargetAArch64();
  for (string_view name : {"memory", "flags", "x0", "x31", "w31", "sp", "wsp",
                           "xzr", "wzr", "fp", "lr", "nzcv", "b31", "h31", "s31", "d31"})
  {
    CheckClobber(aarch64, name, true);
  }
  for (string_view name : {"x32", "w32", "d32", "q0", "X0"})
  {
    CheckClobber(aarch64, name, false);
  }

  const OInlineAsmTarget & riscv = InlineAsmTargetRiscV();
  for (string_view name : {"memory", "x0", "x31", "f0", "f31", "zero", "ra", "sp",
                           "gp", "tp", "fp", "t6", "s11", "a7", "ft11", "fs11", "fa7"})
  {
    CheckClobber(riscv, name, true);
  }
  for (string_view name : {"flags", "x32", "f32", "t7", "s12", "a8", "ft12", "X0"})
  {
    CheckClobber(riscv, name, false);
  }

  return error_count ? 1 : 0;
}
