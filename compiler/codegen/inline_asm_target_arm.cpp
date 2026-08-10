/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    inline_asm_target_arm.cpp
 * authors: nvitya
 * created: 2026-08-10
 * brief:   ARM32 inline assembly properties
 */

#include "inline_asm_target.h"

static bool ArmInlineAsmRegister(string_view name)
{
  return ("sp" == name) || ("lr" == name) || ("pc" == name)
      || InlineAsmIndexedRegister(name, "r", 15)
      || InlineAsmIndexedRegister(name, "s", 31)
      || InlineAsmIndexedRegister(name, "d", 31);
}

const OInlineAsmTarget & InlineAsmTargetArm()
{
  static const OInlineAsmTarget result = {
    "r", "w", INLINE_ASM_DIALECT_NATIVE, true, ArmInlineAsmRegister
  };
  return result;
}
