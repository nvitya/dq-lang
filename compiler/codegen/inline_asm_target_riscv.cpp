/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    inline_asm_target_riscv.cpp
 * authors: nvitya
 * created: 2026-08-10
 * brief:   RISC-V inline assembly properties
 */

#include "inline_asm_target.h"

static bool RiscVInlineAsmRegister(string_view name)
{
  if (("zero" == name) || ("ra" == name) || ("sp" == name) || ("gp" == name)
      || ("tp" == name) || ("fp" == name))
  {
    return true;
  }

  return InlineAsmIndexedRegister(name, "x", 31)
      || InlineAsmIndexedRegister(name, "f", 31)
      || InlineAsmIndexedRegister(name, "t", 6)
      || InlineAsmIndexedRegister(name, "s", 11)
      || InlineAsmIndexedRegister(name, "a", 7)
      || InlineAsmIndexedRegister(name, "ft", 11)
      || InlineAsmIndexedRegister(name, "fs", 11)
      || InlineAsmIndexedRegister(name, "fa", 7);
}

const OInlineAsmTarget & InlineAsmTargetRiscV()
{
  static const OInlineAsmTarget result = {
    "r", "f", INLINE_ASM_DIALECT_NATIVE, false, RiscVInlineAsmRegister
  };
  return result;
}
