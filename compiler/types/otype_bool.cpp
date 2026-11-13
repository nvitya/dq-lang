/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_bool.h
 * authors: nvitya
 * created: 2026-02-02
 * brief:   "bool" (boolean) type implementation
 */

#include "otype_bool.h"
#include "otype_char.h"
#include "dqm_if.h"
#include "expressions.h"
#include "dqc.h"

LlConst * OValueBool::CreateLlConst()
{
  return llvm::ConstantInt::get(ptype->GetLlType(), (value ? 1 : 0));
}

bool OValueBool::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecU8(DQMIF_VALUE_INLINE, value ? 1 : 0);
}

bool OValueBool::CalculateConstant(OExpr * expr, bool emit_errors)
{
  value = 0;

  {
    auto * ex = dynamic_cast<OBoolLit *>(expr);
    if (ex)
    {
      value = ex->value;
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<ONotExpr *>(expr);
    if (ex)
    {
      OValueBool v(this->ptype, false);
      if (not v.CalculateConstant(ex->operand, emit_errors))
      {
        return false;
      }
      value = not v.value;
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<OLValueVar *>(expr);
    if (ex)
    {
      OValSymConst * vsconst = dynamic_cast<OValSymConst *>(ex->pvalsym);
      if (not vsconst)
      {
        if (emit_errors)
        {
          g_compiler->Error(DQERR_CONSTEXPR_NONCONST_SYM, ex->pvalsym->name, "bool");
        }
        return false;
      }

      OValueBool * vint = dynamic_cast<OValueBool *>(vsconst->pvalue);
      if (not vint)
      {
        if (emit_errors)
        {
          g_compiler->Error(DQERR_TYPE_EXPECTED, "bool", ex->ResolvedType()->name);
        }
        return false;
      }

      value = vint->value;
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<OLogicalExpr *>(expr);
    if (ex)
    {
      OValueBool vleft(this->ptype, false);
      if (not vleft.CalculateConstant(ex->left, emit_errors))
      {
        return false;
      }

      if (LOGIOP_AND == ex->op && !vleft.value)
      {
        value = false;
        return true;
      }
      if (LOGIOP_OR == ex->op && vleft.value)
      {
        value = true;
        return true;
      }

      OValueBool vright(this->ptype, false);
      if (not vright.CalculateConstant(ex->right, emit_errors))
      {
        return false;
      }

      if      (LOGIOP_AND == ex->op)  value = (vleft.value and vright.value);
      else if (LOGIOP_OR  == ex->op)  value = (vleft.value or  vright.value);
      else if (LOGIOP_XOR == ex->op)  value = (vleft.value xor vright.value);
      else                            return false;
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<OCompareExpr *>(expr);
    if (ex)
    {
      OType * lefttype = ex->left->ResolvedType();
      OType * righttype = ex->right->ResolvedType();
      if (!lefttype || !righttype)
      {
        return false;
      }
      ETypeKind exprkind = lefttype->kind;

      if (TK_BOOL == exprkind && (COMPOP_EQ == ex->op || COMPOP_NE == ex->op))
      {
        OValueBool vleft(g_builtins->type_bool, false);
        OValueBool vright(g_builtins->type_bool, false);
        if (!vleft.CalculateConstant(ex->left, emit_errors) || !vright.CalculateConstant(ex->right, emit_errors))
        {
          return false;
        }

        value = (COMPOP_EQ == ex->op ? vleft.value == vright.value : vleft.value != vright.value);
        return true;
      }

      if (TK_INT == exprkind || IsCharacterType(lefttype))
      {
        auto * lint = dynamic_cast<OTypeInt *>(lefttype);
        auto * rint = dynamic_cast<OTypeInt *>(righttype);
        if (!lint || !rint)
        {
          return false;
        }

        OValueInt vleft(lint, 0);
        OValueInt vright(rint, 0);
        if (!vleft.CalculateConstant(ex->left, emit_errors) || !vright.CalculateConstant(ex->right, emit_errors))
        {
          return false;
        }

        if (lint->issigned || rint->issigned)
        {
          if      (COMPOP_EQ == ex->op)  value = (vleft.value == vright.value);
          else if (COMPOP_NE == ex->op)  value = (vleft.value != vright.value);
          else if (COMPOP_LT == ex->op)  value = (vleft.value <  vright.value);
          else if (COMPOP_LE == ex->op)  value = (vleft.value <= vright.value);
          else if (COMPOP_GT == ex->op)  value = (vleft.value >  vright.value);
          else if (COMPOP_GE == ex->op)  value = (vleft.value >= vright.value);
          else                           return false;
        }
        else
        {
          uint64_t ul = uint64_t(vleft.value);
          uint64_t ur = uint64_t(vright.value);
          if      (COMPOP_EQ == ex->op)  value = (ul == ur);
          else if (COMPOP_NE == ex->op)  value = (ul != ur);
          else if (COMPOP_LT == ex->op)  value = (ul <  ur);
          else if (COMPOP_LE == ex->op)  value = (ul <= ur);
          else if (COMPOP_GT == ex->op)  value = (ul >  ur);
          else if (COMPOP_GE == ex->op)  value = (ul >= ur);
          else                           return false;
        }
        return true;
      }

      if (TK_FLOAT == exprkind)
      {
        OValueFloat vleft(lefttype, 0);
        OValueFloat vright(righttype, 0);
        if (!vleft.CalculateConstant(ex->left, emit_errors) || !vright.CalculateConstant(ex->right, emit_errors))
        {
          return false;
        }

        if      (COMPOP_EQ == ex->op)  value = (vleft.value == vright.value);
        else if (COMPOP_NE == ex->op)  value = (vleft.value != vright.value);
        else if (COMPOP_LT == ex->op)  value = (vleft.value <  vright.value);
        else if (COMPOP_LE == ex->op)  value = (vleft.value <= vright.value);
        else if (COMPOP_GT == ex->op)  value = (vleft.value >  vright.value);
        else if (COMPOP_GE == ex->op)  value = (vleft.value >= vright.value);
        else                           return false;
        return true;
      }
    }
  }

  if (emit_errors)
  {
    g_compiler->Error(DQERR_BOOL_CONSTEXPR_ERROR);
  }
  return false;
}
