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
  return resolved && TK_CHAR == resolved->kind;
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
    return value <= 255;
  }
  if (dsttype == g_builtins->type_char16)
  {
    return value <= 0xFFFF;
  }
  if (dsttype == g_builtins->type_wchar)
  {
    return IsValidWCharValue(value);
  }
  return false;
}

static bool CharConvertFromExpr(OType * dsttype, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (is_explicit_cast)
  {
    if (!resolved_src || resolved_src->kind != TK_INT)
    {
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src ? resolved_src->name : "?", dsttype->name);
      return false;
    }

    int64_t const_value = 0;
    if (g_compiler->TryCalculateIntConstant(src, const_value) && !CharConstantAccepted(dsttype, const_value))
    {
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, dsttype->name);
      return false;
    }

    *rexpr = new OExprTypeConv(dsttype, src);
    FoldExprTreeAfterTypeRewrite(rexpr);
    return true;
  }

  if (dsttype == g_builtins->type_char && resolved_src == g_builtins->type_cchar)
  {
    *rexpr = new OExprTypeConv(dsttype, src);
    FoldExprTreeAfterTypeRewrite(rexpr);
    return true;
  }

  int64_t literal_value = 0;
  if (TryGetDirectWCharLiteralValue(src, literal_value) && CharConstantAccepted(dsttype, literal_value))
  {
    *rexpr = new OExprTypeConv(dsttype, src);
    FoldExprTreeAfterTypeRewrite(rexpr);
    return true;
  }

  if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", dsttype->name, resolved_src ? resolved_src->name : "?");
  return false;
}

static int CharConversionCostFromExpr(OType * dsttype, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (is_explicit_cast)
  {
    if (!resolved_src || resolved_src->kind != TK_INT) return -1;

    int64_t const_value = 0;
    if (g_compiler->TryCalculateIntConstant(expr, const_value) && !CharConstantAccepted(dsttype, const_value)) return -1;
    return 1;
  }

  if (dsttype == g_builtins->type_char && resolved_src == g_builtins->type_cchar) return 1;

  int64_t literal_value = 0;
  return (TryGetDirectWCharLiteralValue(expr, literal_value) && CharConstantAccepted(dsttype, literal_value)) ? 1 : -1;
}

LlDiType * OTypeChar::CreateDiType()
{
  return di_builder->createBasicType(name, bitlength, llvm::dwarf::DW_ATE_UTF);
}

bool OTypeChar::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  return CharConvertFromExpr(this, rexpr, aflags);
}

int OTypeChar::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  return CharConversionCostFromExpr(this, expr, aflags);
}

LlDiType * OTypeChar16::CreateDiType()
{
  return di_builder->createBasicType(name, bitlength, llvm::dwarf::DW_ATE_UTF);
}

bool OTypeChar16::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  return CharConvertFromExpr(this, rexpr, aflags);
}

int OTypeChar16::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  return CharConversionCostFromExpr(this, expr, aflags);
}

LlDiType * OTypeWchar::CreateDiType()
{
  return di_builder->createBasicType(name, bitlength, llvm::dwarf::DW_ATE_UTF);
}

bool OTypeWchar::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  return CharConvertFromExpr(this, rexpr, aflags);
}

int OTypeWchar::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  return CharConversionCostFromExpr(this, expr, aflags);
}
