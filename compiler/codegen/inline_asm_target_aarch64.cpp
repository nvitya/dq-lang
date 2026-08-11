/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    inline_asm_target_aarch64.cpp
 * authors: nvitya
 * created: 2026-08-10
 * brief:   AArch64 inline assembly properties
 */

#include "inline_asm_target.h"

static bool AArch64InlineAsmRegister(string_view name)
{
  return ("sp" == name) || ("wsp" == name) || ("xzr" == name) || ("wzr" == name)
      || ("fp" == name) || ("lr" == name) || ("nzcv" == name)
      || InlineAsmIndexedRegister(name, "x", 31)
      || InlineAsmIndexedRegister(name, "w", 31)
      || InlineAsmIndexedRegister(name, "b", 31)
      || InlineAsmIndexedRegister(name, "h", 31)
      || InlineAsmIndexedRegister(name, "s", 31)
      || InlineAsmIndexedRegister(name, "d", 31);
}

const OInlineAsmTarget & InlineAsmTargetAArch64()
{
  static const OInlineAsmTarget result = {
    "r", "w", INLINE_ASM_DIALECT_NATIVE, true, AArch64InlineAsmRegister
  };
  return result;
}
