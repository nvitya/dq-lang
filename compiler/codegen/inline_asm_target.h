/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    inline_asm_target.h
 * authors: nvitya
 * created: 2026-08-10
 * brief:   target-specific inline assembly properties
 */

#pragma once

#include <string>
#include <string_view>

using namespace std;

enum EInlineAsmDialect
{
  INLINE_ASM_DIALECT_NATIVE,
  INLINE_ASM_DIALECT_INTEL
};

class OInlineAsmTarget
{
public:
  using TRegisterValidator = bool (*)(string_view name);

  string_view             integer_constraint;
  string_view             float_constraint;
  EInlineAsmDialect       dialect;
  bool                     has_flags;
  TRegisterValidator       register_validator;

  string_view RegisterConstraint(bool is_float) const;
  string ClobberConstraint(string_view name) const;
};

const OInlineAsmTarget * InlineAsmTargetForArch(string_view arch);

bool InlineAsmIndexedRegister(string_view name, string_view prefix, unsigned max_index,
                              string_view suffixes = {});

const OInlineAsmTarget & InlineAsmTargetX86();
const OInlineAsmTarget & InlineAsmTargetArm();
const OInlineAsmTarget & InlineAsmTargetAArch64();
const OInlineAsmTarget & InlineAsmTargetRiscV();
