/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_float.cpp
 * authors: nvitya
 * created: 2026-03-01
 * brief:   float types implementation
 */

#include "otype_float.h"
#include "expressions.h"
#include "dqc.h"

LlConst * OValueFloat::CreateLlConst()
{
  return llvm::ConstantFP::get(ptype->GetLlType(), value);
}

bool OValueFloat::CalculateConstant(OExpr * expr)
{
  value = 0;

  {
    auto * ex = dynamic_cast<OIntLit *>(expr);
    if (ex)
    {
      value = ex->value;
      return true;
    }
  }
  {
    auto * ex = dynamic_cast<OFloatLit *>(expr);
    if (ex)
    {
      value = ex->value;
      return true;
    }
  }
  {
    auto * ex = dynamic_cast<OExprTypeConv *>(expr);
    if (ex)
    {
      return CalculateConstant(ex->src);
    }
  }

  {
    auto * ex = dynamic_cast<ONegExpr *>(expr);
    if (ex)
    {
      OValueFloat v(this->ptype, 0);
      if (not v.CalculateConstant(ex->operand))
      {
        return false;
      }
      value = -v.value;
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<OVarRef *>(expr);
    if (ex)
    {
      OValSymConst * vsconst = dynamic_cast<OValSymConst *>(ex->pvalsym);
      if (not vsconst)
      {
        g_compiler->ExpressionError("Non-constant symbol in float constant expression");
        return false;
      }

      OValueFloat * v = dynamic_cast<OValueFloat *>(vsconst->pvalue);
      if (not v)
      {
        g_compiler->ExpressionError("Float constant expression type error");
        return false;
      }

      value = v->value;
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<OBinExpr *>(expr);
    if (ex)
    {
      OValueFloat vleft(this->ptype, 0);
      OValueFloat vright(this->ptype, 0);

      if (not vleft.CalculateConstant(ex->left)
          or not vright.CalculateConstant(ex->right))
      {
        return false;
      }

      if      (BINOP_ADD == ex->op)  value = vleft.value + vright.value;
      else if (BINOP_SUB == ex->op)  value = vleft.value - vright.value;
      else if (BINOP_MUL == ex->op)  value = vleft.value * vright.value;
      else if (BINOP_DIV == ex->op)  value = vleft.value / vright.value;
      else                           return false;
      return true;
    }
  }

  g_compiler->ExpressionError("Float constant expression error");
  return false;
}

LlValue * OTypeFloat::GenerateConversion(OScope * scope, OExpr * src)
{
  OTypeInt * tint = dynamic_cast<OTypeInt *>(src->ptype);
  if (tint)
  {
    if (tint->issigned)
    {
      return ll_builder.CreateSIToFP(src->Generate(scope), ptype->GetLlType());
    }
    else
    {
      return ll_builder.CreateUIToFP(src->Generate(scope), ptype->GetLlType());
    }
  }

  throw logic_error(format("Unsupported float conversion from \"{}\"", src->ptype->name));
}
