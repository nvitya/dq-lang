/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_int.h
 * authors: nvitya
 * created: 2026-02-02
 * brief:
 */

#include "otype_int.h"
#include "otype_float.h"
#include "expressions.h"
#include "dqc.h"
#include <cmath>

static int64_t NormalizeIntConstant(OTypeInt * dsttype, uint64_t rawbits)
{
  if (dsttype->bitlength >= 64)
  {
    return int64_t(rawbits);
  }

  uint64_t mask = ((uint64_t(1) << dsttype->bitlength) - 1);
  rawbits &= mask;

  if (dsttype->issigned && (rawbits & (uint64_t(1) << (dsttype->bitlength - 1))))
  {
    rawbits |= ~mask;
  }

  return int64_t(rawbits);
}

static int64_t ConvertIntConstant(OTypeInt * dsttype, int64_t value)
{
  return NormalizeIntConstant(dsttype, uint64_t(value));
}

LlConst * OValueInt::CreateLlConst()
{
  return llvm::ConstantInt::get(ptype->GetLlType(), value);
}

bool OValueInt::CalculateConstant(OExpr * expr, bool emit_errors)
{
  value = 0;

  {
    auto * ex = dynamic_cast<OIntLit *>(expr);
    if (ex)
    {
      value = ConvertIntConstant(static_cast<OTypeInt *>(ResolvedType()), ex->value);
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<ONegExpr *>(expr);
    if (ex)
    {
      OValueInt v(this->ptype, 0);
      if (not v.CalculateConstant(ex->operand, emit_errors))
      {
        return false;
      }
      value = ConvertIntConstant(static_cast<OTypeInt *>(ResolvedType()), -v.value);
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<OBinNotExpr *>(expr);
    if (ex)
    {
      OValueInt v(this->ptype, 0);
      if (not v.CalculateConstant(ex->operand, emit_errors))
      {
        return false;
      }
      value = ConvertIntConstant(static_cast<OTypeInt *>(ResolvedType()), ~v.value);
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<OExprTypeConv *>(expr);
    if (ex)
    {
      OType * srctype = ex->src->ResolvedType();
      if (not srctype)
      {
        return false;
      }

      if (TK_INT == srctype->kind)
      {
        OValueInt srcvalue(srctype, 0);
        if (not srcvalue.CalculateConstant(ex->src, emit_errors))
        {
          return false;
        }

        value = ConvertIntConstant(static_cast<OTypeInt *>(ResolvedType()), srcvalue.value);
        return true;
      }

      if (TK_FLOAT == srctype->kind)
      {
        OValueFloat srcvalue(srctype, 0.0);
        if (not srcvalue.CalculateConstant(ex->src, emit_errors))
        {
          return false;
        }

        if (static_cast<OTypeInt *>(ResolvedType())->issigned)
        {
          value = ConvertIntConstant(static_cast<OTypeInt *>(ResolvedType()), int64_t(srcvalue.value));
        }
        else
        {
          value = NormalizeIntConstant(static_cast<OTypeInt *>(ResolvedType()), uint64_t(srcvalue.value));
        }
        return true;
      }
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
          g_compiler->Error(DQERR_CONSTEXPR_NONCONST_SYM, ex->pvalsym->name, "int");
        }
        return false;
      }

      OValueInt * vint = dynamic_cast<OValueInt *>(vsconst->pvalue);
      if (not vint)
      {
        if (emit_errors)
        {
          g_compiler->Error(DQERR_TYPE_EXPECTED, "int", ex->ResolvedType()->name);
        }
        return false;
      }

      value = ConvertIntConstant(static_cast<OTypeInt *>(ResolvedType()), vint->value);
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<OBinExpr *>(expr);
    if (ex)
    {
      OValueInt vleft(this->ptype, 0);
      OValueInt vright(this->ptype, 0);

      if (not vleft.CalculateConstant(ex->left, emit_errors)
          or not vright.CalculateConstant(ex->right, emit_errors))
      {
        return false;
      }

      if      (BINOP_ADD  == ex->op)  value = vleft.value + vright.value;
      else if (BINOP_SUB  == ex->op)  value = vleft.value - vright.value;
      else if (BINOP_MUL  == ex->op)  value = vleft.value * vright.value;

      else if (BINOP_IDIV == ex->op)  value = vleft.value / vright.value;
      else if (BINOP_IMOD == ex->op)  value = vleft.value % vright.value;

      else if (BINOP_IAND == ex->op)  value = (vleft.value & vright.value);
      else if (BINOP_IOR  == ex->op)  value = (vleft.value | vright.value);
      else if (BINOP_IXOR == ex->op)  value = (vleft.value ^ vright.value);

      else if (BINOP_ISHL == ex->op)  value = (vleft.value << vright.value);
      else if (BINOP_ISHR == ex->op)  value = (vleft.value >> vright.value);

      else
      {
        if (emit_errors)
        {
          g_compiler->Error(DQERR_OP_UNHANDLED_FOR, GetBinopSymbol(ex->op), "int const expression");
        }
        return false;
      }

      value = ConvertIntConstant(static_cast<OTypeInt *>(ResolvedType()), value);
      return true;
    }
  }

  {
    auto * ex = dynamic_cast<OFloatRoundExpr *>(expr);
    if (ex)
    {
      OValueFloat vf(g_builtins->type_float, 0);
      if (not vf.CalculateConstant(ex->src, emit_errors))
      {
        return false;
      }
      if      (RNDMODE_ROUND == ex->mode)  value = (int64_t)std::round(vf.value);
      else if (RNDMODE_CEIL  == ex->mode)  value = (int64_t)std::ceil(vf.value);
      else                                 value = (int64_t)std::floor(vf.value);
      value = ConvertIntConstant(static_cast<OTypeInt *>(ResolvedType()), value);
      return true;
    }
  }

  if (emit_errors)
  {
    g_compiler->Error(DQERR_INT_CONSTEXPR_ERROR);
  }
  return false;
}

LlValue * OTypeInt::GenerateConversion(OScope * scope, OExpr * src)
{
  OTypeInt * srcint = dynamic_cast<OTypeInt *>(src->ResolvedType());
  if (srcint)
  {
    LlValue * ll_value = src->Generate(scope);
    if (bitlength > srcint->bitlength)
    {
      // Widen: sext or zext
      return srcint->issigned
          ? ll_builder.CreateSExt(ll_value, GetLlType())
          : ll_builder.CreateZExt(ll_value, GetLlType());
    }
    else
    {
      // Narrow: trunc
      return ll_builder.CreateTrunc(ll_value, GetLlType());
    }
  }

  OTypeFloat * srcfloat = dynamic_cast<OTypeFloat *>(src->ResolvedType());
  if (srcfloat)
  {
    LlValue * ll_value = src->Generate(scope);
    return issigned
        ? ll_builder.CreateFPToSI(ll_value, GetLlType())
        : ll_builder.CreateFPToUI(ll_value, GetLlType());
  }

  throw logic_error(format("Unsupported int conversion from \"{}\"", src->ptype->name));
}
