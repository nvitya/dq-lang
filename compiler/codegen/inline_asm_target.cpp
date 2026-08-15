/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    inline_asm_target.cpp
 * authors: nvitya
 * created: 2026-08-10
 * brief:   common inline assembly target selection
 */

#include "inline_asm_target.h"

string_view OInlineAsmTarget::RegisterConstraint(bool is_float) const
{
  return is_float ? float_constraint : integer_constraint;
}

string OInlineAsmTarget::ClobberConstraint(string_view name) const
{
  if ("memory" == name) return "~{memory}";
  if ("flags" == name) return has_flags ? "~{cc}" : "";
  if (register_validator && register_validator(name)) return "~{" + string(name) + "}";
  return "";
}

bool InlineAsmIndexedRegister(string_view name, string_view prefix, unsigned max_index,
                              string_view suffixes)
{
  if (!name.starts_with(prefix)) return false;

  size_t pos = prefix.size();
  size_t digits_end = pos;
  unsigned index = 0;
  while (digits_end < name.size() && name[digits_end] >= '0' && name[digits_end] <= '9')
  {
    index = index * 10 + unsigned(name[digits_end] - '0');
    if (index > max_index) return false;
    ++digits_end;
  }
  if (digits_end == pos) return false;
  if (digits_end == name.size()) return true;
  return (digits_end + 1 == name.size()) && suffixes.contains(name[digits_end]);
}

const OInlineAsmTarget * InlineAsmTargetForArch(string_view arch)
{
  if ("x86_64" == arch) return &InlineAsmTargetX86();

  if (("arm_m0" == arch) || ("arm_m3" == arch) || ("arm_m4" == arch)
      || ("arm_m4f" == arch) || ("arm_m33" == arch) || ("arm_m33f" == arch)
      || ("arm_m7f" == arch)
      || ("arm" == arch) || arch.starts_with("arm_a"))
  {
    return &InlineAsmTargetArm();
  }

  if (("arm64" == arch) || ("aarch64" == arch)) return &InlineAsmTargetAArch64();
  if (("rv32i" == arch) || ("riscv32" == arch)
      || ("rv64g" == arch) || ("riscv64" == arch))
  {
    return &InlineAsmTargetRiscV();
  }
  return nullptr;
}
