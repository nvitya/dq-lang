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

bool OValueFloat::CalculateConstant(OExpr * expr, bool emit_errors)
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
      OType * srctype = ex->src->ResolvedType();
      if (!srctype)
      {
        return false;
      }

      if (TK_FLOAT == srctype->kind)
      {
        return CalculateConstant(ex->src, emit_errors);
      }

      if (TK_INT == srctype->kind)
      {
        OValueInt srcvalue(srctype, 0);
        if (!srcvalue.CalculateConstant(ex->src, emit_errors))
        {
          return false;
        }
        value = srcvalue.value;
        return true;
      }

      return false;
    }
  }

  {
    auto * ex = dynamic_cast<ONegExpr *>(expr);
    if (ex)
    {
      OValueFloat v(this->ptype, 0);
      if (not v.CalculateConstant(ex->operand, emit_errors))
      {
        return false;
      }
      value = -v.value;
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
          g_compiler->Error(DQERR_CONSTEXPR_NONCONST_SYM, ex->pvalsym->name, "float");
        }
        return false;
      }

      OValueFloat * v = dynamic_cast<OValueFloat *>(vsconst->pvalue);
      if (not v)
      {
        if (emit_errors)
        {
          g_compiler->Error(DQERR_TYPE_EXPECTED, "float", ex->ResolvedType()->name);
        }
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

      if (not vleft.CalculateConstant(ex->left, emit_errors)
          or not vright.CalculateConstant(ex->right, emit_errors))
      {
        return false;
      }

      if      (BINOP_ADD == ex->op)  value = vleft.value + vright.value;
      else if (BINOP_SUB == ex->op)  value = vleft.value - vright.value;
      else if (BINOP_MUL == ex->op)  value = vleft.value * vright.value;
      else if (BINOP_DIV == ex->op)  value = vleft.value / vright.value;
      else
      {
        if (emit_errors)
        {
          g_compiler->Error(DQERR_OP_INVALID_FOR, GetBinopSymbol(ex->op), "float expression");
        }
        return false;
      }
      return true;
    }
  }

  if (emit_errors)
  {
    g_compiler->Error(DQERR_FLOAT_CONSTEXPR_ERROR);
  }
  return false;
}

LlValue * OTypeFloat::GenerateConversion(OScope * scope, OExpr * src)
{
  OTypeInt * tint = dynamic_cast<OTypeInt *>(src->ResolvedType());
  if (tint)
  {
    LlValue * ll_value = src->Generate(scope);
    if (tint->issigned)
    {
      return ll_builder.CreateSIToFP(ll_value, GetLlType());
    }
    else
    {
      return ll_builder.CreateUIToFP(ll_value, GetLlType());
    }
  }

  OTypeFloat * tfloat = dynamic_cast<OTypeFloat *>(src->ResolvedType());
  if (tfloat)
  {
    LlValue * ll_value = src->Generate(scope);
    if (bitlength > tfloat->bitlength)
    {
      return ll_builder.CreateFPExt(ll_value, GetLlType());
    }
    else if (bitlength < tfloat->bitlength)
    {
      return ll_builder.CreateFPTrunc(ll_value, GetLlType());
    }
    return ll_value;
  }

  throw logic_error(format("Unsupported float conversion from \"{}\"", src->ptype->name));
}
