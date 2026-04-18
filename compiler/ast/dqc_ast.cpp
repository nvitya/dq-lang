/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_ast.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include <print>
#include <format>

#include "dqc_ast.h"
#include "otype_array.h"
#include "otype_cstring.h"
#include "otype_float.h"
#include "otype_int.h"

static bool IsPointerWidthIntegerType(OType * type)
{
  OTypeInt * inttype = dynamic_cast<OTypeInt *>(type ? type->ResolveAlias() : nullptr);
  return inttype && (inttype->bitlength == TARGET_PTRSIZE * 8);
}

static bool TryCalculateIntConstant(OExpr * expr, int64_t & rvalue)
{
  OTypeInt * exprtype = dynamic_cast<OTypeInt *>(expr ? expr->ResolvedType() : nullptr);
  if (!exprtype)
  {
    return false;
  }

  OValueInt value(exprtype, 0);
  if (!value.CalculateConstant(expr, false))
  {
    return false;
  }

  rvalue = value.value;
  return true;
}

static bool FitsPointerWidthConstant(OTypeInt * srctype, int64_t value)
{
  uint32_t ptrbits = TARGET_PTRSIZE * 8;
  if (!srctype)
  {
    return false;
  }

  if (ptrbits >= 64)
  {
    return true;
  }

  if (srctype->issigned)
  {
    int64_t minval = -(int64_t(1) << (ptrbits - 1));
    int64_t maxval =  (int64_t(1) << (ptrbits - 1)) - 1;
    return (value >= minval) && (value <= maxval);
  }

  uint64_t maxval = (uint64_t(1) << ptrbits) - 1;
  return uint64_t(value) <= maxval;
}

static bool CanAssignPointerImplicitly(OTypePointer * dst, OTypePointer * src)
{
  if (!dst || !src)
  {
    return false;
  }

  if (src->IsNullPointer())
  {
    return true;
  }

  if (dst->IsOpaquePointer())
  {
    return true;
  }

  if (src->IsOpaquePointer())
  {
    return dst->IsOpaquePointer();
  }

  if (!dst->IsTypedPointer() || !src->IsTypedPointer())
  {
    return false;
  }

  return dst->basetype->ResolveAlias() == src->basetype->ResolveAlias();
}

static void FoldExprTreeAfterTypeRewrite(OExpr ** rexpr)
{
  // ParseExpression() already folds the original parse tree. Re-fold only after type
  // resolution injects conversion nodes so constant casts collapse immediately.
  OExpr::FoldTree(rexpr);
}

ODqCompAst::ODqCompAst()
{
}

ODqCompAst::~ODqCompAst()
{
}

bool ODqCompAst::ResolveCompoundMemberBase(OLValueExpr * lval, OType * srctype, OLValueExpr *& memberbase, OCompoundType *& ctype)
{
  if (TK_COMPOUND == srctype->kind)
  {
    memberbase = lval;
    ctype = static_cast<OCompoundType *>(srctype);
    return true;
  }

  if (TK_POINTER == srctype->kind)
  {
    OTypePointer * ptype = static_cast<OTypePointer *>(srctype);
    if (ptype->basetype && TK_COMPOUND == ptype->basetype->kind)
    {
      memberbase = new OLValueDeref(lval);
      ctype = static_cast<OCompoundType *>(ptype->basetype);
      return true;
    }
  }

  return false;
}

ODecl * ODqCompAst::AddDeclVar(OScPosition & scpos, string aid, OType * atype)
{
  OValSym * pvalsym = new OValSym(scpos, aid, atype, VSK_VARIABLE);
  pvalsym->scpos.Assign(scpos);

  ODecl * result = g_module->DeclareValSym(section_public, pvalsym);

  if (g_opt.verblevel >= VERBLEVEL_DEBUG)
  {
    print("{}: ", scpos.Format());
    print("AddVarDecl(): var {} : {}", aid, atype->name);
    print("\n");
  }

  return result;
}

ODecl * ODqCompAst::AddDeclConst(OScPosition & scpos, string aid, OType * atype, OValue * avalue)
{
  OValSym * pvalsym = new OValSymConst(scpos, aid, atype, avalue);
  pvalsym->scpos.Assign(scpos);

  ODecl * result = g_module->DeclareValSym(section_public, pvalsym);

  if (g_opt.verblevel >= VERBLEVEL_DEBUG)
  {
    print("{}: ", scpos.Format());
    print("AddConstDecl(): var {} : {}", aid, atype->name);
    print("\n");
  }

  return result;
}

ODecl * ODqCompAst::AddDeclFunc(OScPosition & scpos, OValSymFunc * avsfunc)
{
  ODecl * result = g_module->DeclareValSym(section_public, avsfunc);

  if (g_opt.verblevel >= VERBLEVEL_DEBUG)
  {
    print("{}: ", scpos.Format());
    print("AddDeclFunc(): {}", avsfunc->name);
  }

  avsfunc->scpos.Assign(scpos);

  OTypeFunc * tfunc = (OTypeFunc *)(avsfunc->ptype);

  // push the parameters into the scope
  for (OFuncParam * fp : tfunc->params)
  {
    OValSym * vsarg = new OValSym(scpos, fp->name, fp->ptype, VSK_PARAMETER);
    vsarg->initialized = true;
    avsfunc->args.push_back(vsarg);
    avsfunc->body->scope->DefineValSym(vsarg);
  }

  // add the implicit result variable
  if (tfunc->rettype)
  {
    avsfunc->vsresult = new OValSym(scpos, "result", tfunc->rettype, VSK_VARIABLE);
    avsfunc->body->scope->DefineValSym(avsfunc->vsresult);
  }

  if (g_opt.verblevel >= VERBLEVEL_DEBUG)
  {
    print("\n");
  }

  return result;
}

void ODqCompAst::CollectIgnoredPlainAssignVars(OLValueExpr * leftexpr, vector<OLValueVar *> & ignored)
{
  if (!leftexpr)
  {
    return;
  }

  if (auto * varref = dynamic_cast<OLValueVar *>(leftexpr))
  {
    ignored.push_back(varref);
    return;
  }

  if (auto * memberref = dynamic_cast<OLValueMember *>(leftexpr))
  {
    CollectIgnoredPlainAssignVars(memberref->base, ignored);
    return;
  }

  if (auto * indexref = dynamic_cast<OLValueIndex *>(leftexpr))
  {
    OType * containertype = indexref->containertype ? indexref->containertype->ResolveAlias() : nullptr;
    if (!containertype)
    {
      return;
    }

    if (TK_ARRAY == containertype->kind)
    {
      CollectIgnoredPlainAssignVars(indexref->base, ignored);
      return;
    }

    if (TK_STRING == containertype->kind)
    {
      OTypeCString * cstrtype = static_cast<OTypeCString *>(containertype);
      if (cstrtype->maxlen > 0)
      {
        CollectIgnoredPlainAssignVars(indexref->base, ignored);
      }
      return;
    }

    return;
  }
}

OValSym * ODqCompAst::GetAssignRootValSym(OLValueExpr * leftexpr)
{
  if (!leftexpr)
  {
    return nullptr;
  }

  if (auto * varref = dynamic_cast<OLValueVar *>(leftexpr))
  {
    return varref->pvalsym;
  }

  if (auto * memberref = dynamic_cast<OLValueMember *>(leftexpr))
  {
    return GetAssignRootValSym(memberref->base);
  }

  if (auto * indexref = dynamic_cast<OLValueIndex *>(leftexpr))
  {
    return GetAssignRootValSym(indexref->base);
  }

  return nullptr;
}

bool ODqCompAst::HarmonizeNumericOperands(OExpr ** rleft, OExpr ** rright)
{
  if (!rleft || !rright || !*rleft || !*rright)
  {
    return false;
  }

  OExpr * left = *rleft;
  OExpr * right = *rright;
  OType * lefttype = left->ResolvedType();
  OType * righttype = right->ResolvedType();
  if (!lefttype || !righttype)
  {
    return false;
  }

  ETypeKind tkl = lefttype->kind;
  ETypeKind tkr = righttype->kind;

  if ((TK_INT == tkl) and (TK_INT == tkr))
  {
    OTypeInt * intl = static_cast<OTypeInt *>(lefttype);
    OTypeInt * intr = static_cast<OTypeInt *>(righttype);
    if (intl->bitlength != intr->bitlength)
    {
      if (intl->bitlength > intr->bitlength)
        right = new OExprTypeConv(left->ptype, right);
      else
        left = new OExprTypeConv(right->ptype, left);
    }

    *rleft = left;
    *rright = right;
    return true;
  }

  if ((TK_INT == tkl) and (TK_FLOAT == tkr))
  {
    *rleft = new OExprTypeConv(right->ptype, left);
    return true;
  }

  if ((TK_FLOAT == tkl) and (TK_INT == tkr))
  {
    *rright = new OExprTypeConv(left->ptype, right);
    return true;
  }

  if ((TK_FLOAT == tkl) and (TK_FLOAT == tkr))
  {
    OTypeFloat * floatl = static_cast<OTypeFloat *>(lefttype);
    OTypeFloat * floatr = static_cast<OTypeFloat *>(righttype);
    if (floatl->bitlength != floatr->bitlength)
    {
      if (floatl->bitlength > floatr->bitlength)
        right = new OExprTypeConv(left->ptype, right);
      else
        left = new OExprTypeConv(right->ptype, left);
    }

    *rleft = left;
    *rright = right;
    return true;
  }

  return false;
}

bool ODqCompAst::ResolveCommonPointerType(OExpr * leftexpr, OExpr * rightexpr, OType ** rresulttype)
{
  if (!leftexpr || !rightexpr || !rresulttype)
  {
    return false;
  }

  OTypePointer * leftptr = dynamic_cast<OTypePointer *>(leftexpr->ResolvedType());
  OTypePointer * rightptr = dynamic_cast<OTypePointer *>(rightexpr->ResolvedType());
  if (!leftptr || !rightptr)
  {
    return false;
  }

  if (leftptr->IsNullPointer() and !rightptr->IsNullPointer())
  {
    *rresulttype = rightexpr->ptype;
    return true;
  }

  if (rightptr->IsNullPointer() and !leftptr->IsNullPointer())
  {
    *rresulttype = leftexpr->ptype;
    return true;
  }

  if (leftexpr->ptype == rightexpr->ptype
      || (leftptr->IsOpaquePointer() && rightptr->IsOpaquePointer())
      || (leftptr->IsTypedPointer() && rightptr->IsTypedPointer()
          && (leftptr->basetype->ResolveAlias() == rightptr->basetype->ResolveAlias())))
  {
    *rresulttype = leftexpr->ptype;
    return true;
  }

  return false;
}

OExpr * ODqCompAst::FreeLeftRight(OExpr * left, OExpr * right)
{
  if (left) delete left;
  if (right) delete right;
  return nullptr;
}

OExpr * ODqCompAst::CreateBinExpr(EBinOp op, OExpr * left, OExpr * right)
{
  OExpr * newleft  = left;
  OExpr * newright = right;

  if (not left  or  not right)
  {
    return nullptr;
  }

  ETypeKind tkl = left->ptype->kind;
  ETypeKind tkr = right->ptype->kind;

  if ((op >= BINOP_IAND) and (op <= BINOP_ISHR))
  {
    if ((tkl != TK_INT) or (tkr != TK_INT))
    {
      Error(DQERR_TYPEMISM_FOR_OP, left->ptype->name, GetBinopSymbol(op), right->ptype->name);
      return nullptr;
    }
  }
  else if (tkl != tkr)
  {
    if ((TK_POINTER == tkl) and (TK_INT == tkr)
             and (BINOP_ADD == op or BINOP_SUB == op))
    {
      OTypePointer * ptrtype = static_cast<OTypePointer *>(left->ResolvedType());
      if (!ptrtype->IsTypedPointer())
      {
        Error(DQERR_PTR_OPAQUE_USAGE, "pointer arithmetic");
        return nullptr;
      }
    }
    else if (!HarmonizeNumericOperands(&newleft, &newright))
    {
      Error(DQERR_TYPEMISM_FOR_OP, left->ptype->name, GetBinopSymbol(op), right->ptype->name);
      return nullptr;
    }
  }
  else
  {
    HarmonizeNumericOperands(&newleft, &newright);

    if ((TK_INT == tkl) and (TK_INT == tkr))
    {
      OTypeInt * intl = static_cast<OTypeInt *>(left->ResolvedType());
      OTypeInt * intr = static_cast<OTypeInt *>(right->ResolvedType());
      if ((intl->bitlength == intr->bitlength) and (BINOP_DIV == op))
      {
        newleft  = new OExprTypeConv(g_builtins->type_float, newleft);
        newright = new OExprTypeConv(g_builtins->type_float, newright);
      }
    }
  }

  return new OBinExpr(op, newleft, newright);
}

bool ODqCompAst::ConvertExprToType(OType * dsttype, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = (rexpr ? *rexpr : nullptr);
  if (!dsttype || !src || !src->ptype)
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      Error(DQERR_TYPEMISM, (!dsttype ? "?" : dsttype->name), (!src || !src->ptype ? "?" : src->ptype->name));
    }
    return false;
  }

  OType * resolved_dst = dsttype->ResolveAlias();
  OType * resolved_src = src->ResolvedType();
  if (!resolved_dst || !resolved_src)
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      Error(DQERR_TYPEMISM, (!resolved_dst ? "?" : resolved_dst->name), (!resolved_src ? "?" : resolved_src->name));
    }
    return false;
  }

  ETypeKind tkd = resolved_dst->kind;
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (tkd != tks)
  {
    if ((TK_FLOAT == tkd) and (TK_INT == tks))
    {
      *rexpr = new OExprTypeConv(dsttype, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if (is_explicit_cast && (TK_INT == tkd) && (TK_BOOL == tks))
    {
      *rexpr = new OExprTypeConv(dsttype, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if (is_explicit_cast && (TK_INT == tkd) && (TK_FLOAT == tks))
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        Error(DQERR_CAST_FLOAT_TO_INT, resolved_src->name, resolved_dst->name);
      }
      return false;
    }

    if (is_explicit_cast && (TK_POINTER == tkd) && (TK_INT == tks))
    {
      OTypeInt * intsrc = static_cast<OTypeInt *>(resolved_src);
      int64_t const_value = 0;
      bool is_const = TryCalculateIntConstant(src, const_value);
      if (!IsPointerWidthIntegerType(resolved_src))
      {
        if (!is_const)
        {
          if (aflags & EXPCF_GENERATE_ERRORS)
          {
            Error(DQERR_CAST_PTR_WIDTH_MISM, resolved_src->name);
          }
          return false;
        }

        if (!FitsPointerWidthConstant(intsrc, const_value))
        {
          if (aflags & EXPCF_GENERATE_ERRORS)
          {
            ErrorTxt(DQERR_CAST_PTR_CONST_RANGE, to_string(const_value));
          }
          return false;
        }
      }

      *rexpr = new OExprTypeConv(dsttype, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if (is_explicit_cast && (TK_INT == tkd) && (TK_POINTER == tks))
    {
      if (!IsPointerWidthIntegerType(resolved_dst))
      {
        if (aflags & EXPCF_GENERATE_ERRORS)
        {
          Error(DQERR_CAST_PTR_WIDTH_MISM, resolved_dst->name);
        }
        return false;
      }

      *rexpr = new OExprTypeConv(dsttype, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if ((TK_ARRAY_SLICE == tkd) and (TK_ARRAY == tks))
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS)
        {
          Error(DQERR_CAST_INVALID, resolved_src->name, resolved_dst->name);
        }
        return false;
      }

      OTypeArraySlice * slicedst = static_cast<OTypeArraySlice *>(resolved_dst);
      OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
      if (slicedst->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias())
      {
        if (aflags & EXPCF_GENERATE_ERRORS)
        {
          Error(DQERR_ARR_ELEM_TYPE_MISM, slicedst->elemtype->ResolveAlias()->name, arrsrc->elemtype->ResolveAlias()->name);
        }
        return false;
      }

      OLValueVar * varref = dynamic_cast<OLValueVar *>(src);
      if (!varref)
      {
        if (aflags & EXPCF_GENERATE_ERRORS)
        {
          Error(DQERR_ARR_SLICE_CONVERSION);
        }
        return false;
      }

      OExpr * result = new OArrayToSliceExpr(varref->pvalsym, dsttype);
      delete src;
      *rexpr = result;
      return true;
    }

    if ((TK_STRING == tkd) and (TK_POINTER == tks))
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS)
        {
          Error(DQERR_CAST_INVALID, resolved_src->name, resolved_dst->name);
        }
        return false;
      }

      OTypeCString * cstrdst = static_cast<OTypeCString *>(resolved_dst);
      OCStringLit * strlit = dynamic_cast<OCStringLit *>(src);
      if (cstrdst->maxlen != 0)
      {
        if (strlit && (aflags & EXPCF_ALLOW_LAZY_CSTRING))
        {
          return true;
        }

        if (aflags & EXPCF_GENERATE_ERRORS)
        {
          ErrorTxt(DQERR_CSTR_CONVERSION, "cannot convert pointer to cstring descriptor");
        }
        return false;
      }

      if (!strlit)
      {
        if (aflags & EXPCF_GENERATE_ERRORS)
        {
          ErrorTxt(DQERR_CSTR_CONVERSION, "cannot convert pointer to cstring descriptor");
        }
        return false;
      }

      *rexpr = new OCStringLitToDescExpr(src, strlit->value.size() + 1, dsttype);
      return true;
    }

    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      if (is_explicit_cast)
      {
        Error(DQERR_CAST_INVALID, resolved_src->name, resolved_dst->name);
      }
      else
      {
        Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", resolved_dst->name, resolved_src->name);
      }
    }
    return false;
  }

  if (TK_INT == tkd)
  {
    OTypeInt * intdst = static_cast<OTypeInt *>(resolved_dst);
    OTypeInt * intsrc = static_cast<OTypeInt *>(resolved_src);
    if ((intdst->bitlength != intsrc->bitlength) or (intdst->issigned != intsrc->issigned))
    {
      *rexpr = new OExprTypeConv(dsttype, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    return true;
  }

  if (TK_FLOAT == tkd)
  {
    OTypeFloat * floatdst = static_cast<OTypeFloat *>(resolved_dst);
    OTypeFloat * floatsrc = static_cast<OTypeFloat *>(resolved_src);
    if (floatdst->bitlength != floatsrc->bitlength)
    {
      *rexpr = new OExprTypeConv(dsttype, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    return true;
  }

  if (TK_POINTER == tkd)
  {
    OTypePointer * ptrdst = static_cast<OTypePointer *>(resolved_dst);
    OTypePointer * ptrsrc = static_cast<OTypePointer *>(resolved_src);

    if (is_explicit_cast)
    {
      *rexpr = new OExprTypeConv(dsttype, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if (!CanAssignPointerImplicitly(ptrdst, ptrsrc))
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        Error(DQERR_PTR_TYPEMISM, ptrdst->name, ptrsrc->name);
      }
      return false;
    }

    return true;
  }

  if (TK_ARRAY_SLICE == tkd)
  {
    if (is_explicit_cast)
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        Error(DQERR_CAST_INVALID, resolved_src->name, resolved_dst->name);
      }
      return false;
    }

    OTypeArraySlice * slicedst = static_cast<OTypeArraySlice *>(resolved_dst);
    OTypeArraySlice * slicesrc = static_cast<OTypeArraySlice *>(resolved_src);
    if (slicedst->elemtype->ResolveAlias() != slicesrc->elemtype->ResolveAlias())
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        Error(DQERR_ARR_ELEM_TYPE_MISM, slicedst->elemtype->ResolveAlias()->name, slicesrc->elemtype->ResolveAlias()->name);
      }
      return false;
    }

    return true;
  }

  if (TK_ARRAY == tkd)
  {
    if (is_explicit_cast)
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        Error(DQERR_CAST_INVALID, resolved_src->name, resolved_dst->name);
      }
      return false;
    }

    OTypeArray * arrdst = static_cast<OTypeArray *>(resolved_dst);
    OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
    if (arrdst->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias())
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        Error(DQERR_ARR_ELEM_TYPE_MISM, arrdst->elemtype->ResolveAlias()->name, arrsrc->elemtype->ResolveAlias()->name);
      }
      return false;
    }
    if (arrdst->arraylength != arrsrc->arraylength)
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        Error(DQERR_ARR_SIZE_MISM, to_string(arrdst->arraylength), to_string(arrsrc->arraylength));
      }
      return false;
    }

    return true;
  }

  if (TK_STRING == tkd)
  {
    if (is_explicit_cast)
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        Error(DQERR_CAST_INVALID, resolved_src->name, resolved_dst->name);
      }
      return false;
    }

    OTypeCString * cstrdst = static_cast<OTypeCString *>(resolved_dst);
    OTypeCString * cstrsrc = static_cast<OTypeCString *>(resolved_src);
    if ((cstrdst->maxlen == 0) and (cstrsrc->maxlen > 0))
    {
      OLValueVar * varref = dynamic_cast<OLValueVar *>(src);
      if (!varref)
      {
        if (aflags & EXPCF_GENERATE_ERRORS)
        {
          ErrorTxt(DQERR_CSTR_CONVERSION, "cannot convert non-variable cstring to descriptor");
        }
        return false;
      }

      OExpr * result = new OCStringToDescExpr(varref->pvalsym, dsttype);
      delete src;
      *rexpr = result;
      return true;
    }

    if ((cstrdst->maxlen > 0) && (aflags & EXPCF_ALLOW_LAZY_CSTRING))
    {
      return true;
    }

    if (cstrdst->maxlen != cstrsrc->maxlen)
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        ErrorTxt(DQERR_CSTR_CONVERSION, "cstring sizes do not match");
      }
      return false;
    }

    return true;
  }

  return true;
}

bool ODqCompAst::ResolveIifType(OExpr ** rtrueexpr, OExpr ** rfalseexpr, OType ** rresulttype)
{
  OType * truetype = (*rtrueexpr)->ResolvedType();
  OType * falsetype = (*rfalseexpr)->ResolvedType();

  if (!truetype || !falsetype)
  {
    ErrorTxt(DQERR_TYPE_EXPECTED, "iif() expects non-void value expressions");
    return false;
  }

  if (truetype == falsetype)
  {
    *rresulttype = truetype;
    return true;
  }

  if (HarmonizeNumericOperands(rtrueexpr, rfalseexpr))
  {
    FoldExprTreeAfterTypeRewrite(rtrueexpr);
    FoldExprTreeAfterTypeRewrite(rfalseexpr);
    *rresulttype = (*rtrueexpr)->ptype;
    return true;
  }

  OType * resulttype = nullptr;
  if (ResolveCommonPointerType(*rtrueexpr, *rfalseexpr, &resulttype))
  {
    *rresulttype = resulttype;
    return true;
  }

  if ((TK_POINTER == truetype->kind) and (TK_POINTER == falsetype->kind))
  {
    Error(DQERR_PTR_TYPEMISM, (*rtrueexpr)->ptype->name, (*rfalseexpr)->ptype->name);
    return false;
  }

  if (ConvertExprToType(truetype, rfalseexpr, EXPCF_ALLOW_LAZY_CSTRING))
  {
    *rresulttype = truetype;
    return true;
  }

  if (ConvertExprToType(falsetype, rtrueexpr, EXPCF_ALLOW_LAZY_CSTRING))
  {
    *rresulttype = falsetype;
    return true;
  }

  Error(DQERR_TYPEMISM_STMT_ASSIGN, "iif()", (*rtrueexpr)->ptype->name, (*rfalseexpr)->ptype->name);
  return false;
}

bool ODqCompAst::CheckAssignType(OType * dsttype, OExpr ** rexpr, const string astmt)
{
  (void)astmt;
  return ConvertExprToType(dsttype, rexpr, EXPCF_GENERATE_ERRORS | EXPCF_ALLOW_LAZY_CSTRING);
}
