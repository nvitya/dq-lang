/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    inline_asm_target_x86.cpp
 * authors: nvitya
 * created: 2026-08-10
 * brief:   x86-64 inline assembly properties
 */

#include <algorithm>
#include <array>

#include "inline_asm_target.h"

static bool X86InlineAsmRegister(string_view name)
{
  static constexpr array fixed_registers = {
    string_view("rax"), string_view("rbx"), string_view("rcx"), string_view("rdx"),
    string_view("rsi"), string_view("rdi"), string_view("rbp"), string_view("rsp"),
    string_view("eax"), string_view("ebx"), string_view("ecx"), string_view("edx"),
    string_view("esi"), string_view("edi"), string_view("ebp"), string_view("esp"),
    string_view("ax"), string_view("bx"), string_view("cx"), string_view("dx"),
    string_view("si"), string_view("di"), string_view("bp"), string_view("sp"),
    string_view("al"), string_view("bl"), string_view("cl"), string_view("dl"),
    string_view("ah"), string_view("bh"), string_view("ch"), string_view("dh"),
    string_view("sil"), string_view("dil"), string_view("bpl"), string_view("spl"),
    string_view("st")
  };
  if (find(fixed_registers.begin(), fixed_registers.end(), name) != fixed_registers.end())
  {
    return true;
  }

  if (name.starts_with("r"))
  {
    string_view index = name.substr(1);
    if (!index.empty() && (index.back() == 'd' || index.back() == 'w' || index.back() == 'b'))
    {
      index.remove_suffix(1);
    }
    if (InlineAsmIndexedRegister(index, "", 15))
    {
      unsigned value = 0;
      for (char c : index) value = value * 10 + unsigned(c - '0');
      if (value >= 8) return true;
    }
  }

  return InlineAsmIndexedRegister(name, "xmm", 31)
      || InlineAsmIndexedRegister(name, "ymm", 31)
      || InlineAsmIndexedRegister(name, "zmm", 31)
      || InlineAsmIndexedRegister(name, "mm", 7)
      || InlineAsmIndexedRegister(name, "k", 7);
}

const OInlineAsmTarget & InlineAsmTargetX86()
{
  static const OInlineAsmTarget result = {
    "r", "x", INLINE_ASM_DIALECT_INTEL, true, X86InlineAsmRegister
  };
  return result;
}
