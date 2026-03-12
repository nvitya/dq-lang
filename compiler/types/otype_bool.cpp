/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_bool.h
 * authors: nvitya
 * created: 2026-02-02
 * brief:   "bool" (boolean) type implementation
 */

#include "otype_bool.h"
#include "expressions.h"
#include "dqc.h"

LlConst * OValueBool::CreateLlConst()
{
  return llvm::ConstantInt::get(ptype->GetLlType(), (value ? 1 : 0));
}

bool OValueBool::CalculateConstant(OExpr * expr)
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
      if (not v.CalculateConstant(ex->operand))
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
        g_compiler->ExpressionError("Non-constant symbol in bool constant expression");
        return false;
      }

      OValueBool * vint = dynamic_cast<OValueBool *>(vsconst->pvalue);
      if (not vint)
      {
        g_compiler->ExpressionError("Bool constant expression type error");
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
      OValueBool vleft(this->ptype, 0);
      OValueBool vright(this->ptype, 0);

      if (not vleft.CalculateConstant(ex->left)
          or not vright.CalculateConstant(ex->right))
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
      ETypeKind exprkind = ex->left->ResolvedType()->kind;

      if (TK_INT == exprkind)
      {
        OValueInt vleft(g_builtins->type_int, 0);
        OValueInt vright(g_builtins->type_int, 0);
        if (!vleft.CalculateConstant(ex->left) || !vright.CalculateConstant(ex->right))
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

      if (TK_FLOAT == exprkind)
      {
        OValueFloat vleft(g_builtins->type_float, 0);
        OValueFloat vright(g_builtins->type_float, 0);
        if (!vleft.CalculateConstant(ex->left) || !vright.CalculateConstant(ex->right))
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

  g_compiler->ExpressionError("Bool constant expression error");
  return false;
}
