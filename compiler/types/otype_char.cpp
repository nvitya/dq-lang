/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_char.cpp
 * authors: nvitya
 * created: 2026-06-07
 * brief:   Character type implementation
 */

#include "otype_char.h"
#include "dqc_ast.h"
#include "dqc.h"
#include "expressions.h"
#include "scope_builtins.h"

bool IsCharacterType(OType * type)
{
  OType * resolved = type ? type->ResolveAlias() : nullptr;
  return resolved && (TK_CHAR == resolved->kind || TK_WCHAR == resolved->kind);
}

bool IsValidWCharValue(int64_t value)
{
  return value >= 0
      && value <= 0x10FFFF
      && !(value >= 0xD800 && value <= 0xDFFF);
}

bool TryGetDirectWCharLiteralValue(OExpr * expr, int64_t & rvalue)
{
  auto * lit = dynamic_cast<OIntLit *>(expr);
  if (!lit || lit->ResolvedType() != g_builtins->type_wchar)
  {
    return false;
  }

  rvalue = lit->value;
  return true;
}

static bool CharConstantAccepted(OType * dsttype, int64_t value)
{
  if (dsttype == g_builtins->type_char)
  {
    return value >= 0 && value <= 255;
  }
  if (dsttype == g_builtins->type_char16)
  {
    return value >= 0 && value <= 0xFFFF;
  }
  if (dsttype == g_builtins->type_wchar)
  {
    return IsValidWCharValue(value);
  }
  return false;
}

int OTypeCharBase::AnalyzeConversion(OExpr ** rexpr, uint32_t aflags, bool convert)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();

  int64_t const_value = 0;
  if (TryGetDirectWCharLiteralValue(src, const_value) && CharConstantAccepted(this, const_value))
  {
    return WrapTypeConv(rexpr, convert);
  }

  // Other sources need an explicit cast from an integer whose constant value (if known) fits the char type
  if (!(aflags & EXPCF_EXPLICIT_CAST) || !resolved_src || (TK_INT != resolved_src->kind)
      || (g_compiler->TryCalculateIntConstant(src, const_value) && !CharConstantAccepted(this, const_value)))
  {
    return ConversionError(resolved_src, aflags);
  }

  return WrapTypeConv(rexpr, convert);
}

LlDiType * OTypeCharBase::CreateDiType()
{
  return di_builder->createBasicType(name, bitlength, llvm::dwarf::DW_ATE_UTF);
}
