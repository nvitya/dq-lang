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
#include "otype_func.h"
#include "otype_int.h"
#include "otype_object.h"
#include "otype_string.h"
#include "otype_anyvalue.h"

bool ODqCompAst::IsPointerWidthIntegerType(OType * type)
{
  OTypeInt * inttype = dynamic_cast<OTypeInt *>(type ? type->ResolveAlias() : nullptr);
  return inttype && (inttype->bitlength == TARGET_PTRSIZE * 8);
}

bool ODqCompAst::TryCalculateIntConstant(OExpr * expr, int64_t & rvalue)
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

bool ODqCompAst::FitsPointerWidthConstant(OTypeInt * srctype, int64_t value)
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

bool ODqCompAst::CanAssignPointerImplicitly(OTypePointer * dst, OTypePointer * src)
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

  auto * dst_object = dynamic_cast<OTypeObject *>(dst->basetype ? dst->basetype->ResolveAlias() : nullptr);
  auto * src_object = dynamic_cast<OTypeObject *>(src->basetype ? src->basetype->ResolveAlias() : nullptr);
  if (dst_object && src_object)
  {
    return src_object->IsSameOrDerivedFrom(dst_object);
  }

  return dst->basetype->ResolveAlias() == src->basetype->ResolveAlias();
}

void ODqCompAst::FoldExprTreeAfterTypeRewrite(OExpr ** rexpr)
{
  // ParseExpression() already folds the original parse tree. Re-fold only after type
  // resolution injects conversion nodes so constant casts collapse immediately.
  OExpr::FoldTree(rexpr);
}

static bool IsCCharPointerType(OType * type)
{
  auto * ptrtype = dynamic_cast<OTypePointer *>(type ? type->ResolveAlias() : nullptr);
  return ptrtype
      && ptrtype->IsTypedPointer()
      && ptrtype->basetype
      && (ptrtype->basetype->ResolveAlias() == g_builtins->type_cchar);
}

static bool IsCharLiteralExpr(OExpr * expr, uint8_t & rvalue)
{
  auto * lit = dynamic_cast<OIntLit *>(expr);
  OType * lit_type = lit ? lit->ResolvedType() : nullptr;
  if (!lit || !lit_type || (lit_type != g_builtins->type_char && lit_type != g_builtins->type_cchar))
  {
    return false;
  }
  rvalue = uint8_t(lit->value);
  return true;
}

ODqCompAst::ODqCompAst()
{
}

ODqCompAst::~ODqCompAst()
{
}

bool ODqCompAst::ResolveCompoundMemberBase(OLValueExpr * lval, OType * srctype, OLValueExpr *& memberbase, OCompoundType *& ctype)
{
  if (srctype->IsCompound())
  {
    memberbase = lval;
    ctype = static_cast<OCompoundType *>(srctype);
    return true;
  }

  if (TK_POINTER == srctype->kind)
  {
    OTypePointer * ptype = static_cast<OTypePointer *>(srctype);
    OType * basetype = (ptype->basetype ? ptype->basetype->ResolveAlias() : nullptr);
    if (basetype && basetype->IsCompound())
    {
      memberbase = new OLValueDeref(lval);
      ctype = static_cast<OCompoundType *>(basetype);
      return true;
    }
  }

  return false;
}

ODecl * ODqCompAst::AddDeclVar(OScPosition & scpos, string aid, OType * atype)
{
  OValSym * pvalsym = atype->CreateValSym(scpos, aid);
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

void ODqCompAst::PrepareFuncDecl(OScPosition & scpos, OValSymFunc * avsfunc)
{
  avsfunc->scpos.Assign(scpos);

  OTypeFunc * tfunc = (OTypeFunc *)(avsfunc->ptype);

  // push the parameters into the scope
  for (OFuncParam * fp : tfunc->params)
  {
    OValSym * vsarg = fp->ptype->CreateValSym(scpos, fp->name);
    vsarg->kind = VSK_PARAMETER;
    vsarg->param_mode = fp->mode;
    vsarg->is_ref_alias = fp->IsRefLike();
    vsarg->ref_nullable = (FPM_REFNULL == fp->mode);
    vsarg->initialized = (FPM_REFOUT != fp->mode);
    if (auto * objarg = dynamic_cast<OVsObject *>(vsarg); objarg && objarg->IsRefLike())
    {
      objarg->SetObjectStorage(OSK_PLAIN);
    }
    avsfunc->args.push_back(vsarg);
    avsfunc->body->scope->DefineValSym(vsarg);
    if (avsfunc->owner_compound_type && ("__this" == fp->name))
    {
      avsfunc->receiver_arg = vsarg;
      if (!avsfunc->body->scope->FindValSym("self", nullptr, false))
      {
        avsfunc->body->scope->valsyms["self"] = vsarg;
      }
    }
  }

  // add the implicit result variable
  if (tfunc->rettype)
  {
    avsfunc->vsresult = tfunc->rettype->CreateValSym(scpos, "result");
    avsfunc->body->scope->DefineValSym(avsfunc->vsresult);
  }
}

ODecl * ODqCompAst::AddDeclFunc(OScPosition & scpos, OValSymFunc * avsfunc)
{
  ODecl * result = g_module->DeclareValSym(section_public, avsfunc);

  if (g_opt.verblevel >= VERBLEVEL_DEBUG)
  {
    print("{}: ", scpos.Format());
    print("AddDeclFunc(): {}", avsfunc->name);
  }

  PrepareFuncDecl(scpos, avsfunc);
  g_module->RegisterSpecialFunction(avsfunc);

  if (g_opt.verblevel >= VERBLEVEL_DEBUG)
  {
    print("\n");
  }

  return result;
}

ODecl * ODqCompAst::AddDeclOverloadSet(OScPosition & scpos, OValSymOverloadSet * avsoverload)
{
  ODecl * result = g_module->DeclareValSym(section_public, avsoverload);

  if (g_opt.verblevel >= VERBLEVEL_DEBUG)
  {
    print("{}: ", scpos.Format());
    print("AddDeclOverloadSet(): {}", avsoverload->name);
    print("\n");
  }

  avsoverload->scpos.Assign(scpos);
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

    if (TK_CSTRING == containertype->kind)
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

bool ODqCompAst::ResolveCommonFuncRefType(OExpr * leftexpr, OExpr * rightexpr, OType ** rresulttype)
{
  if (!leftexpr || !rightexpr || !rresulttype)
  {
    return false;
  }

  OTypeFuncRef * leftcb = dynamic_cast<OTypeFuncRef *>(leftexpr->ResolvedType());
  OTypeFuncRef * rightcb = dynamic_cast<OTypeFuncRef *>(rightexpr->ResolvedType());
  OTypePointer * leftptr = dynamic_cast<OTypePointer *>(leftexpr->ResolvedType());
  OTypePointer * rightptr = dynamic_cast<OTypePointer *>(rightexpr->ResolvedType());

  if (leftcb && rightcb && leftcb->object_ref == rightcb->object_ref
      && leftcb->functype && rightcb->functype
      && leftcb->functype->MatchesSignature(rightcb->functype))
  {
    *rresulttype = leftexpr->ptype;
    return true;
  }

  if (leftcb && rightptr && rightptr->IsNullPointer())
  {
    *rresulttype = leftexpr->ptype;
    return true;
  }

  if (rightcb && leftptr && leftptr->IsNullPointer())
  {
    *rresulttype = rightexpr->ptype;
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
  else if ((TK_POINTER == tkl) and (TK_POINTER == tkr))
  {
    OTypePointer * lptr = static_cast<OTypePointer *>(left->ResolvedType());
    OTypePointer * rptr = static_cast<OTypePointer *>(right->ResolvedType());
    if ((BINOP_SUB != op) or !lptr->IsTypedPointer() or !rptr->IsTypedPointer()
        or (lptr->basetype->ResolveAlias() != rptr->basetype->ResolveAlias()))
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


#include "otype_anyvalue.h"


#include "../types/otype_anyvalue.h"

bool ODqCompAst::ConvertExprToType(OType * dsttype, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  if (!dsttype || !src || !src->ptype)
  {
    return false;
  }

  OType * resolved_dst = dsttype->ResolveAlias();
  OType * resolved_src = src->ResolvedType();
  if (!resolved_dst || !resolved_src)
  {
    return false;
  }

  if (resolved_dst == resolved_src)
  {
    return true;
  }

  return resolved_dst->ConvertFromExpr(this, rexpr, aflags);
}

int ODqCompAst::GetAssignTypeConversionCost(OType * dsttype, OExpr * expr, uint32_t aflags)
{
  if (!dsttype || !expr || !expr->ptype)
  {
    return -1;
  }

  OType * resolved_dst = dsttype->ResolveAlias();
  OType * resolved_src = expr->ResolvedType();
  if (!resolved_dst || !resolved_src)
  {
    return -1;
  }

  if (resolved_dst == resolved_src)
  {
    return 0;
  }

  return resolved_dst->GetConversionCostFromExpr(this, expr, aflags);
}

// ---- Virtual Implementations of ConvertFromExpr ----




bool OTypeInt::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_INT != tks)
  {
    if (is_explicit_cast && (TK_BOOL == tks))
    {
      *rexpr = new OExprTypeConv(this, src);
      ast->FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }
    if (is_explicit_cast && (TK_FLOAT == tks))
    {
      if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_FLOAT_TO_INT, resolved_src->name, this->name);
      return false;
    }
    if (is_explicit_cast && (TK_POINTER == tks))
    {
      if (!ast->IsPointerWidthIntegerType(this))
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_PTR_WIDTH_MISM, this->name);
        return false;
      }
      *rexpr = new OExprTypeConv(this, src);
      ast->FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }
    return OType::ConvertFromExpr(ast, rexpr, aflags);
  }

  OTypeInt * intsrc = static_cast<OTypeInt *>(resolved_src);
  if (!is_explicit_cast && (resolved_src == g_builtins->type_char) && (this == g_builtins->type_cchar))
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
    return false;
  }
  if ((this->bitlength != intsrc->bitlength) or (this->issigned != intsrc->issigned))
  {
    *rexpr = new OExprTypeConv(this, src);
    ast->FoldExprTreeAfterTypeRewrite(rexpr);
    return true;
  }
  return true;
}

int OTypeInt::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_INT != tks)
  {
    if (is_explicit_cast && (TK_BOOL == tks)) return 1;
    if (is_explicit_cast && (TK_FLOAT == tks)) return -1;
    if (is_explicit_cast && (TK_POINTER == tks)) return (ast->IsPointerWidthIntegerType(this) ? 1 : -1);
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  OTypeInt * intsrc = static_cast<OTypeInt *>(resolved_src);
  if (!is_explicit_cast && (resolved_src == g_builtins->type_char) && (this == g_builtins->type_cchar)) return -1;
  return (((this->bitlength == intsrc->bitlength) && (this->issigned == intsrc->issigned)) ? 0 : 1);
}



bool OTypeFloat::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (TK_FLOAT != tks)
  {
    if (TK_INT == tks)
    {
      *rexpr = new OExprTypeConv(this, src);
      ast->FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }
    return OType::ConvertFromExpr(ast, rexpr, aflags);
  }

  OTypeFloat * floatsrc = static_cast<OTypeFloat *>(resolved_src);
  if (this->bitlength != floatsrc->bitlength)
  {
    *rexpr = new OExprTypeConv(this, src);
    ast->FoldExprTreeAfterTypeRewrite(rexpr);
    return true;
  }
  return true;
}

int OTypeFloat::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (TK_FLOAT != tks)
  {
    if (TK_INT == tks) return 1;
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  OTypeFloat * floatsrc = static_cast<OTypeFloat *>(resolved_src);
  return ((this->bitlength == floatsrc->bitlength) ? 0 : 1);
}



bool OTypeObject::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_OBJECT != tks)
  {
    if (TK_POINTER == tks)
    {
      OTypePointer * ptrsrc = static_cast<OTypePointer *>(resolved_src);
      OTypeObject * src_object = dynamic_cast<OTypeObject *>(ptrsrc->basetype ? ptrsrc->basetype->ResolveAlias() : nullptr);
      if (ptrsrc->IsNullPointer() || (src_object && src_object->IsSameOrDerivedFrom(this)))
      {
        if (src_object && src_object != this)
        {
          *rexpr = new OObjectUpcastExpr(this, src);
        }
        else if (is_explicit_cast)
        {
          *rexpr = new OExprTypeConv(this, src);
          ast->FoldExprTreeAfterTypeRewrite(rexpr);
        }
        return true;
      }
      if (is_explicit_cast)
      {
        *rexpr = new OExprTypeConv(this, src);
        ast->FoldExprTreeAfterTypeRewrite(rexpr);
        return true;
      }
      if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
      return false;
    }
    return OType::ConvertFromExpr(ast, rexpr, aflags);
  }

  return true;
}

int OTypeObject::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_OBJECT != tks)
  {
    if (TK_POINTER == tks)
    {
      OTypePointer * ptrsrc = static_cast<OTypePointer *>(resolved_src);
      OTypeObject * src_object = dynamic_cast<OTypeObject *>(ptrsrc->basetype ? ptrsrc->basetype->ResolveAlias() : nullptr);
      if (ptrsrc->IsNullPointer() || (src_object && src_object->IsSameOrDerivedFrom(this))) return 0;
      return is_explicit_cast ? 1 : -1;
    }
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  return 0;
}



bool OTypeFuncRef::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (TK_FUNCREF != tks)
  {
    OValSymFunc * matched_func = nullptr;
    EOverloadFuncRefMatch ovmatch = FindAcceptingOverload(src, matched_func);
    if (OFRM_UNIQUE_MATCH == ovmatch)
    {
      if (auto * bound_ov = dynamic_cast<OBoundMethodOverloadExpr *>(src))
      {
        bound_ov->matched_func = matched_func;
        *rexpr = new OExprTypeConv(this, src);
      }
      else
      {
        delete src;
        *rexpr = new OExprTypeConv(this, new OLValueVar(matched_func));
      }
      ast->FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if ((OFRM_NO_MATCH == ovmatch) || (OFRM_AMBIGUOUS == ovmatch))
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        auto * srcvar = dynamic_cast<OLValueVar *>(src);
        string srcname = (srcvar && srcvar->pvalsym ? srcvar->pvalsym->name : string("?"));
        if (OFRM_AMBIGUOUS == ovmatch)
        {
          ast->ErrorTxt(DQERR_OVERLOAD_AMBIGUOUS, format("Overloaded function \"{}\" is ambiguous for callback type \"{}\"", srcname, this->name));
        }
        else
        {
          ast->ErrorTxt(DQERR_OVERLOAD_NO_MATCH, format("No overload of function \"{}\" matches callback type \"{}\"", srcname, this->name));
        }
      }
      return false;
    }

    if (CanAccept(resolved_src))
    {
      *rexpr = new OExprTypeConv(this, src);
      ast->FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_FUNCSIG_TYPEMISM, this->name, resolved_src->name);
    return false;
  }

  if (!CanAccept(resolved_src))
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_FUNCSIG_TYPEMISM, this->name, resolved_src->name);
    return false;
  }

  if (this != resolved_src)
  {
    *rexpr = new OExprTypeConv(this, src);
    ast->FoldExprTreeAfterTypeRewrite(rexpr);
  }

  return true;
}

int OTypeFuncRef::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (TK_FUNCREF != tks)
  {
    OValSymFunc * matched_func = nullptr;
    EOverloadFuncRefMatch ovmatch = FindAcceptingOverload(expr, matched_func);
    if (OFRM_UNIQUE_MATCH == ovmatch) return 1;
    if ((OFRM_NO_MATCH == ovmatch) || (OFRM_AMBIGUOUS == ovmatch)) return -1;
    return (CanAccept(resolved_src) ? 1 : -1);
  }

  if (!CanAccept(resolved_src)) return -1;
  return ((this == resolved_src) ? 0 : 1);
}



bool OTypeAnyValue::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (TK_ANYVALUE != tks)
  {
    if (!IsAnyValueSourceType(resolved_src))
    {
      if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
      return false;
    }
    if (!EnsureAnyValueRtlUse()) return false;
    *rexpr = new OAnyValueBoxExpr(src, this);
    return true;
  }
  return true;
}

int OTypeAnyValue::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  if (TK_ANYVALUE != tks)
  {
    return IsAnyValueSourceType(resolved_src) ? 1 : -1;
  }
  return 0;
}



bool OTypePointer::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_POINTER != tks)
  {
    if (is_explicit_cast && (TK_INT == tks))
    {
      OTypeInt * intsrc = static_cast<OTypeInt *>(resolved_src);
      int64_t const_value = 0;
      bool is_const = ast->TryCalculateIntConstant(src, const_value);
      if (!ast->IsPointerWidthIntegerType(resolved_src))
      {
        if (!is_const)
        {
          if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_PTR_WIDTH_MISM, resolved_src->name);
          return false;
        }

        if (!ast->FitsPointerWidthConstant(intsrc, const_value))
        {
          if (aflags & EXPCF_GENERATE_ERRORS) ast->ErrorTxt(DQERR_CAST_PTR_CONST_RANGE, to_string(const_value));
          return false;
        }
      }

      *rexpr = new OExprTypeConv(this, src);
      ast->FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if (TK_INT == tks)
    {
      uint8_t charlit = 0;
      if (IsCCharPointerType(this) && IsCharLiteralExpr(src, charlit))
      {
        *rexpr = new OCharLitToCStringPtrExpr(charlit);
        return true;
      }
    }

    if (is_explicit_cast && (TK_OBJECT == tks))
    {
      *rexpr = new OExprTypeConv(this, src);
      ast->FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    return OType::ConvertFromExpr(ast, rexpr, aflags);
  }

  OTypePointer * ptrsrc = dynamic_cast<OTypePointer *>(resolved_src);

  if (is_explicit_cast)
  {
    *rexpr = new OExprTypeConv(this, src);
    ast->FoldExprTreeAfterTypeRewrite(rexpr);
    return true;
  }

  uint8_t charlit = 0;
  if (IsCCharPointerType(this) && IsCharLiteralExpr(src, charlit))
  {
    *rexpr = new OCharLitToCStringPtrExpr(charlit);
    return true;
  }

  if (!ptrsrc)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_PTR_TYPEMISM, this->name, resolved_src->name);
    return false;
  }

  if (!ast->CanAssignPointerImplicitly(this, ptrsrc))
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_PTR_TYPEMISM, this->name, ptrsrc->name);
    return false;
  }

  return true;
}

int OTypePointer::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_POINTER != tks)
  {
    if (is_explicit_cast && (TK_INT == tks))
    {
      OTypeInt * intsrc = static_cast<OTypeInt *>(resolved_src);
      int64_t const_value = 0;
      bool is_const = ast->TryCalculateIntConstant(expr, const_value);
      if (!ast->IsPointerWidthIntegerType(resolved_src))
      {
        if (!is_const || !ast->FitsPointerWidthConstant(intsrc, const_value)) return -1;
      }
      return 1;
    }
    if (TK_INT == tks)
    {
      uint8_t charlit = 0;
      return (IsCCharPointerType(this) && IsCharLiteralExpr(expr, charlit)) ? 1 : -1;
    }
    if (is_explicit_cast && (TK_OBJECT == tks)) return 1;
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  OTypePointer * ptrsrc = dynamic_cast<OTypePointer *>(resolved_src);
  if (is_explicit_cast) return 1;

  uint8_t charlit = 0;
  if (IsCCharPointerType(this) && IsCharLiteralExpr(expr, charlit)) return 1;
  if (!ptrsrc) return -1;
  return (ast->CanAssignPointerImplicitly(this, ptrsrc) ? 0 : -1);
}



bool OTypeArray::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ARRAY != tks)
  {
    return OType::ConvertFromExpr(ast, rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }

  OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
  if ((TK_ANYVALUE == this->elemtype->ResolveAlias()->kind) && dynamic_cast<OArrayLit *>(src))
  {
    auto * arrlit = static_cast<OArrayLit *>(src);
    if (this->arraylength != arrlit->elements.size())
    {
      if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_ARR_SIZE_MISM, to_string(this->arraylength), to_string(arrlit->elements.size()));
      return false;
    }
    if (!EnsureAnyValueRtlUse()) return false;
    for (OExpr *& elem : arrlit->elements)
    {
      if (!ast->ConvertExprToType(this->elemtype, &elem, EXPCF_GENERATE_ERRORS | EXPCF_ALLOW_LAZY_CSTRING)) return false;
    }
    arrlit->ptype = this;
    return true;
  }
  if (this->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias())
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, arrsrc->elemtype->ResolveAlias()->name);
    return false;
  }
  if (this->arraylength != arrsrc->arraylength)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_ARR_SIZE_MISM, to_string(this->arraylength), to_string(arrsrc->arraylength));
    return false;
  }

  return true;
}

int OTypeArray::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ARRAY != tks)
  {
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  if (is_explicit_cast) return -1;

  OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
  if ((TK_ANYVALUE == this->elemtype->ResolveAlias()->kind) && dynamic_cast<OArrayLit *>(expr))
  {
    return (this->arraylength == arrsrc->arraylength) ? 1 : -1;
  }
  if ((this->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias()) || (this->arraylength != arrsrc->arraylength))
  {
    return -1;
  }

  return 0;
}

bool OTypeArraySlice::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ARRAY_SLICE != tks)
  {
    if (TK_ARRAY == tks)
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
      if ((TK_ANYVALUE == this->elemtype->ResolveAlias()->kind) && dynamic_cast<OArrayLit *>(src))
      {
        auto * arrlit = static_cast<OArrayLit *>(src);
        if (!EnsureAnyValueRtlUse()) return false;
        for (OExpr *& elem : arrlit->elements)
        {
          if (!ast->ConvertExprToType(this->elemtype, &elem, EXPCF_GENERATE_ERRORS | EXPCF_ALLOW_LAZY_CSTRING)) return false;
        }
        arrlit->ptype = this->elemtype->GetArrayType(uint32_t(arrlit->elements.size()));
        *rexpr = new OArrayLitToSliceExpr(arrlit, this);
        return true;
      }
      if (this->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias())
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, arrsrc->elemtype->ResolveAlias()->name);
        return false;
      }
      if (auto * arrlit = dynamic_cast<OArrayLit *>(src))
      {
        if (aflags & EXPCF_ALLOW_ARRAY_LITERAL_SLICE)
        {
          *rexpr = new OArrayLitToSliceExpr(arrlit, this);
          return true;
        }
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
        return false;
      }
      OLValueExpr * lval = dynamic_cast<OLValueExpr *>(src);
      if (!lval)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_ARR_SLICE_CONVERSION);
        return false;
      }
      *rexpr = new OArrayToSliceExpr(lval, this);
      return true;
    }
    if (TK_DYN_ARRAY == tks)
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      OTypeDynArray * dynsrc = static_cast<OTypeDynArray *>(resolved_src);
      if (this->elemtype->ResolveAlias() != dynsrc->elemtype->ResolveAlias())
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, dynsrc->elemtype->ResolveAlias()->name);
        return false;
      }
      OLValueExpr * lval = dynamic_cast<OLValueExpr *>(src);
      if (!lval)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_ARR_SLICE_CONVERSION);
        return false;
      }
      *rexpr = new ODynArrayToSliceExpr(lval, this);
      return true;
    }
    return OType::ConvertFromExpr(ast, rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }
  OTypeArraySlice * slicesrc = static_cast<OTypeArraySlice *>(resolved_src);
  if (this->elemtype->ResolveAlias() != slicesrc->elemtype->ResolveAlias())
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, slicesrc->elemtype->ResolveAlias()->name);
    return false;
  }
  return true;
}

int OTypeArraySlice::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ARRAY_SLICE != tks)
  {
    if (TK_ARRAY == tks)
    {
      OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
      if ((TK_ANYVALUE == this->elemtype->ResolveAlias()->kind) && (aflags & EXPCF_ALLOW_ARRAY_LITERAL_SLICE) && dynamic_cast<OArrayLit *>(expr)) return 1;
      if (is_explicit_cast || (this->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias())) return -1;
      if (dynamic_cast<OLValueExpr *>(expr)) return 1;
      if ((aflags & EXPCF_ALLOW_ARRAY_LITERAL_SLICE) && dynamic_cast<OArrayLit *>(expr)) return 1;
      return -1;
    }
    if (TK_DYN_ARRAY == tks)
    {
      OTypeDynArray * dynsrc = static_cast<OTypeDynArray *>(resolved_src);
      if (is_explicit_cast || (this->elemtype->ResolveAlias() != dynsrc->elemtype->ResolveAlias())) return -1;
      return (dynamic_cast<OLValueExpr *>(expr) ? 1 : -1);
    }
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  if (is_explicit_cast) return -1;
  OTypeArraySlice * slicesrc = static_cast<OTypeArraySlice *>(resolved_src);
  return ((this->elemtype->ResolveAlias() == slicesrc->elemtype->ResolveAlias()) ? 0 : -1);
}

bool OTypeDynArray::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_DYN_ARRAY != tks)
  {
    if (is_explicit_cast)
    {
      if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
      return false;
    }
    bool ok = false;
    if (TK_ARRAY == tks)
    {
      OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
      if ((TK_ANYVALUE == this->elemtype->ResolveAlias()->kind) && dynamic_cast<OArrayLit *>(src))
      {
        auto * arrlit = static_cast<OArrayLit *>(src);
        if (!EnsureAnyValueRtlUse()) return false;
        for (OExpr *& elem : arrlit->elements)
        {
          if (!ast->ConvertExprToType(this->elemtype, &elem, EXPCF_GENERATE_ERRORS | EXPCF_ALLOW_LAZY_CSTRING)) return false;
        }
        arrlit->ptype = this->elemtype->GetArrayType(uint32_t(arrlit->elements.size()));
        return true;
      }
      ok = (arrsrc->arraylength == 0) || (this->elemtype->ResolveAlias() == arrsrc->elemtype->ResolveAlias());
    }
    else if (TK_ARRAY_SLICE == tks)
    {
      ok = (this->elemtype->ResolveAlias() == static_cast<OTypeArraySlice *>(resolved_src)->elemtype->ResolveAlias());
    }
    if (!ok && (aflags & EXPCF_GENERATE_ERRORS)) ast->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
    return ok;
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }
  OTypeDynArray * dynsrc = static_cast<OTypeDynArray *>(resolved_src);
  bool ok = (this->elemtype->ResolveAlias() == dynsrc->elemtype->ResolveAlias());
  if (!ok && (aflags & EXPCF_GENERATE_ERRORS)) ast->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
  return ok;
}

int OTypeDynArray::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_DYN_ARRAY != tks)
  {
    if (is_explicit_cast) return -1;
    if (TK_ARRAY == tks)
    {
      OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
      if ((TK_ANYVALUE == this->elemtype->ResolveAlias()->kind) && dynamic_cast<OArrayLit *>(expr)) return 1;
      return ((arrsrc->arraylength == 0) || (this->elemtype->ResolveAlias() == arrsrc->elemtype->ResolveAlias())) ? 1 : -1;
    }
    if (TK_ARRAY_SLICE == tks)
    {
      return (this->elemtype->ResolveAlias() == static_cast<OTypeArraySlice *>(resolved_src)->elemtype->ResolveAlias()) ? 1 : -1;
    }
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  if (is_explicit_cast) return -1;
  OTypeDynArray * dynsrc = static_cast<OTypeDynArray *>(resolved_src);
  return ((this->elemtype->ResolveAlias() == dynsrc->elemtype->ResolveAlias()) ? 0 : -1);
}



bool OTypeStrView::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_STRVIEW != tks)
  {
    if (IsTextSourceType(resolved_src))
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      *rexpr = new OTextSourceToViewExpr(src, this);
      return true;
    }
    return OType::ConvertFromExpr(ast, rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }
  return true;
}

int OTypeStrView::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_STRVIEW != tks)
  {
    if (IsTextSourceType(resolved_src)) return is_explicit_cast ? -1 : 1;
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  return is_explicit_cast ? -1 : 0;
}

bool OTypeDynString::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_DYNSTR != tks)
  {
    if (IsTextSourceType(resolved_src))
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      *rexpr = new OTextSourceToStringExpr(src, this);
      return true;
    }
    return OType::ConvertFromExpr(ast, rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }
  return true;
}

int OTypeDynString::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_DYNSTR != tks)
  {
    if (IsTextSourceType(resolved_src)) return is_explicit_cast ? -1 : 1;
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  return is_explicit_cast ? -1 : 0;
}



bool OTypeCString::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_CSTRING != tks)
  {
    if (TK_POINTER == tks)
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      if (this->maxlen != 0)
      {
        if ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && this->CanStoreFrom(src)) return true;
        if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
        return false;
      }
      if ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && IsCCharPointerType(resolved_src))
      {
        auto * strlit = dynamic_cast<OCStringLit *>(src);
        uint32_t known_len = (strlit ? uint32_t(strlit->value.size() + 1) : 0);
        *rexpr = new OCStringLitToDescExpr(src, known_len, this);
        return true;
      }
      if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
      return false;
    }
    return OType::ConvertFromExpr(ast, rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }

  OTypeCString * cstrsrc = static_cast<OTypeCString *>(resolved_src);
  if ((this->maxlen == 0) and (cstrsrc->maxlen > 0))
  {
    OLValueExpr * lval = dynamic_cast<OLValueExpr *>(src);
    if (!lval)
    {
      if (aflags & EXPCF_GENERATE_ERRORS) ast->ErrorTxt(DQERR_CSTR_CONVERSION, "cannot convert non-lvalue cstring to descriptor");
      return false;
    }
    *rexpr = new OCStringLValueToDescExpr(lval, this);
    return true;
  }

  if ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && this->CanStoreFrom(src)) return true;

  if (this->maxlen != cstrsrc->maxlen)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) ast->ErrorTxt(DQERR_CSTR_CONVERSION, "cstring sizes do not match");
    return false;
  }

  return true;
}

int OTypeCString::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_CSTRING != tks)
  {
    if (TK_POINTER == tks)
    {
      if (is_explicit_cast) return -1;
      if (this->maxlen != 0) return ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && this->CanStoreFrom(expr)) ? 0 : -1;
      return ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && IsCCharPointerType(resolved_src)) ? 1 : -1;
    }
    return OType::GetConversionCostFromExpr(ast, expr, aflags);
  }

  if (is_explicit_cast) return -1;

  OTypeCString * cstrsrc = static_cast<OTypeCString *>(resolved_src);
  if ((this->maxlen == 0) && (cstrsrc->maxlen > 0)) return (dynamic_cast<OLValueExpr *>(expr) ? 1 : -1);
  if ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && this->CanStoreFrom(expr)) return 0;
  return (this->maxlen == cstrsrc->maxlen) ? 0 : -1;
}





bool OType::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);
  
  if (this->kind == resolved_src->kind) return true;
  
  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      ast->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    }
  }
  else
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      ast->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
    }
  }
  return false;
}

int OType::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  if (this->kind == resolved_src->kind) return 0;
  return -1;
}

bool OCompoundType::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  return OType::ConvertFromExpr(ast, rexpr, aflags);
}

int OCompoundType::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  return OType::GetConversionCostFromExpr(ast, expr, aflags);
}



bool OTypeAlias::ConvertFromExpr(ODqCompAst * ast, OExpr ** rexpr, uint32_t aflags)
{
  return (ptype ? ptype->ConvertFromExpr(ast, rexpr, aflags) : false);
}

int OTypeAlias::GetConversionCostFromExpr(ODqCompAst * ast, OExpr * expr, uint32_t aflags)
{
  return (ptype ? ptype->GetConversionCostFromExpr(ast, expr, aflags) : -1);
}

// --------------------------------------------------

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
  if (ResolveCommonFuncRefType(*rtrueexpr, *rfalseexpr, &resulttype))
  {
    *rresulttype = resulttype;
    return true;
  }

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


// ---- Functions moved from ODqCompParser ----

bool ODqCompAst::SupportsFuncParamDefaultType(OType * ptype)
{
  OType * resolved = (ptype ? ptype->ResolveAlias() : nullptr);
  if (!resolved)
  {
    return false;
  }

  if ((TK_INT == resolved->kind) || (TK_FLOAT == resolved->kind)
      || (TK_BOOL == resolved->kind) || (TK_ARRAY == resolved->kind)
      || (TK_POINTER == resolved->kind))
  {
    return true;
  }

  if (TK_CSTRING == resolved->kind)
  {
    OTypeCString * cstrtype = dynamic_cast<OTypeCString *>(resolved);
    return cstrtype && (cstrtype->maxlen > 0);
  }

  return false;
}

void ODqCompAst::InjectObjectReceiver(OValSymFunc * vsfunc, OCompoundType * ctype)
{
  if (!vsfunc || !ctype)
  {
    return;
  }

  OTypeFunc * tfunc = dynamic_cast<OTypeFunc *>(vsfunc->ptype);
  if (!tfunc)
  {
    return;
  }

  if (!tfunc->ParNameValid("__this"))
  {
    Error(DQERR_FUNCPAR_NAME_INVALID, "__this");
  }

  tfunc->params.insert(tfunc->params.begin(), new OFuncParam("__this", ctype, FPM_REF));
}

bool ODqCompAst::SpecialFunctionSignatureIsValid(OValSymFunc * vsfunc)
{
  OTypeFunc * tfunc = dynamic_cast<OTypeFunc *>(vsfunc ? vsfunc->ptype : nullptr);
  if (!vsfunc || !tfunc || !tfunc->params.empty() || tfunc->has_varargs)
  {
    return false;
  }

  if (SFK_MAIN == vsfunc->special_kind)
  {
    return tfunc->rettype && (tfunc->rettype->ResolveAlias() == g_builtins->native_int);
  }

  if (SFK_MODULE_INIT == vsfunc->special_kind)
  {
    OType * rettype = tfunc->rettype ? tfunc->rettype->ResolveAlias() : nullptr;
    return !rettype || (TK_VOID == rettype->kind);
  }

  return false;
}

void ODqCompAst::ValidateConstructorEmbeddedObjects(OValSymFunc * vsfunc)
{
  if (!vsfunc || !vsfunc->owner_compound_type || !vsfunc->body)
  {
    return;
  }

  OTypeObject * object_type = dynamic_cast<OTypeObject *>(vsfunc->owner_compound_type);
  if (!object_type)
  {
    return;
  }
  if (object_type->base_type)
  {
    int inherited_lifecycle_count = 0;
    size_t inherited_lifecycle_index = size_t(-1);
    for (size_t i = 0; i < vsfunc->body->stlist.size(); ++i)
    {
      auto * inherited = dynamic_cast<OStmtInheritedCall *>(vsfunc->body->stlist[i]);
      if (inherited && inherited->method
          && inherited->method->object_specfunc_kind == vsfunc->object_specfunc_kind)
      {
        ++inherited_lifecycle_count;
        inherited_lifecycle_index = i;
      }
    }

    if (OSF_CREATE == vsfunc->object_specfunc_kind)
    {
      if (inherited_lifecycle_count != 1 || inherited_lifecycle_index != 0)
      {
        ErrorTxt(DQERR_OBJ_SPEC_FUNC_INVALID,
                 "derived constructors must call inherited Create exactly once as the first statement",
                 &vsfunc->scpos_endfunc);
      }
    }
    else if (OSF_DESTROY == vsfunc->object_specfunc_kind)
    {
      bool requires_inherited_destroy = object_type->GetBaseObject()->FindSpecialMethod(OSF_DESTROY) != nullptr;
      if (requires_inherited_destroy
          && (inherited_lifecycle_count != 1 || inherited_lifecycle_index + 1 != vsfunc->body->stlist.size()))
      {
        ErrorTxt(DQERR_OBJ_SPEC_FUNC_INVALID,
                 "derived destructors must call inherited Destroy exactly once as the last statement",
                 &vsfunc->scpos_endfunc);
      }
    }
  }

  if (OSF_CREATE != vsfunc->object_specfunc_kind)
  {
    return;
  }

  vector<bool> constructed(object_type->member_order.size(), false);

  for (size_t i = 0; i < object_type->member_order.size(); ++i)
  {
    OValSym * member = object_type->member_order[i];
    auto * objmember = dynamic_cast<OVsObject *>(member);
    if (objmember && objmember->IsFixedObjectStorage() && objmember->ObjectCtorCallAtDecl())
    {
      constructed[i] = true;
    }
  }

  for (OStmt * stmt : vsfunc->body->stlist)
  {
    auto * voidcall = dynamic_cast<OStmtVoidCall *>(stmt);
    auto * callexpr = dynamic_cast<OCallExpr *>(voidcall ? voidcall->callexpr : nullptr);
    if (!callexpr || !callexpr->vsfunc || (OSF_CREATE != callexpr->vsfunc->object_specfunc_kind) || callexpr->args.empty())
    {
      continue;
    }

    auto * objaddr = dynamic_cast<OObjectAddrExpr *>(callexpr->args[0]);
    auto * memberref = dynamic_cast<OLValueMember *>(objaddr ? objaddr->target : nullptr);
    if (!memberref || (memberref->structtype != object_type) || (memberref->memberindex >= object_type->member_order.size()))
    {
      continue;
    }

    OValSym * member = object_type->member_order[memberref->memberindex];
    auto * objmember = dynamic_cast<OVsObject *>(member);
    if (!objmember || !objmember->IsFixedObjectStorage())
    {
      continue;
    }

    if (constructed[memberref->memberindex])
    {
      ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, format("embedded object \"{}\" is constructed twice", member->name), &stmt->scpos);
    }
    constructed[memberref->memberindex] = true;
  }

  for (size_t i = 0; i < object_type->member_order.size(); ++i)
  {
    OValSym * member = object_type->member_order[i];
    auto * objmember = dynamic_cast<OVsObject *>(member);
    if (objmember && objmember->IsFixedObjectStorage() && !constructed[i])
    {
      OTypeObject * member_object = objmember->ObjectType();
      if (member_object && member_object->HasTrivialDefaultConstructor())
      {
        continue;
      }
      ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, format("embedded object \"{}\" is not constructed", member->name), &vsfunc->scpos_endfunc);
    }
  }
}

OValSymFunc * ODqCompAst::AddGeneratedObjectConstructor(OTypeObject * object_type, OValSymFunc * inherited_ctor,
                                                           OScPosition & scpos, size_t overload_count)
{
  if (!object_type)
  {
    return nullptr;
  }

  OTypeFunc * inherited_sig = dynamic_cast<OTypeFunc *>(inherited_ctor ? inherited_ctor->ptype : nullptr);
  if (inherited_ctor && !inherited_sig)
  {
    return nullptr;
  }

  OTypeFunc * tfunc = new OTypeFunc("Create");
  if (inherited_sig)
  {
    tfunc->has_varargs = inherited_sig->has_varargs;
    for (size_t i = 1; i < inherited_sig->params.size(); ++i)
    {
      OFuncParam * src = inherited_sig->params[i];
      if (!src)
      {
        continue;
      }
      tfunc->AddParam(src->name, src->ptype, src->mode);
    }
  }

  OValSymFunc * ctor = new OValSymFunc(scpos, "Create", tfunc, object_type->Members());
  ctor->owner_compound_type = object_type;
  ctor->generated_linkage_name = object_type->name + ".Create";
  ctor->object_specfunc_kind = OSF_CREATE;
  ctor->member_visibility = inherited_ctor ? inherited_ctor->member_visibility : MV_PUBLIC;
  ctor->has_body = true;
  ctor->scpos_endfunc.Assign(scpos);
  ctor->attr_is_overload = (overload_count > 1);

  InjectObjectReceiver(ctor, object_type);
  PrepareFuncDecl(scpos, ctor);

  if (inherited_ctor)
  {
    vector<OExpr *> inherited_args;
    for (size_t i = 1; i < ctor->args.size(); ++i)
    {
      inherited_args.push_back(new OLValueVar(ctor->args[i]));
    }
    auto * stmt = new OStmtInheritedCall(scpos, ctor, inherited_ctor, inherited_args);
    stmt->emit_derived_field_init = true;
    ctor->body->stlist.push_back(stmt);
  }

  object_type->constructors.push_back(ctor);

  if (overload_count > 1)
  {
    OValSymOverloadSet * ovset = nullptr;
    OValSym * existing = object_type->Members()->FindValSym("Create", nullptr, false);
    if (!existing)
    {
      ovset = new OValSymOverloadSet(scpos, "Create", g_builtins->type_func);
      ovset->owner_compound_type = object_type;
      ovset->generated_linkage_prefix = object_type->name + ".Create";
      ovset->member_visibility = ctor->member_visibility;
      object_type->Members()->DefineValSym(ovset);
      g_module->DeclareHiddenValSym(true, ovset);
    }
    else
    {
      ovset = dynamic_cast<OValSymOverloadSet *>(existing);
    }

    if (ovset)
    {
      ovset->AddFunc(ctor);
    }
    else
    {
      Error(DQERR_OVERLOAD_MIXED_DECL, "Create");
    }
  }
  else
  {
    object_type->Members()->DefineValSym(ctor);
    g_module->DeclareHiddenValSym(true, ctor);
  }

  return ctor;
}

OValSymFunc * ODqCompAst::AddGeneratedObjectDestructor(OTypeObject * object_type, OValSymFunc * inherited_dtor,
                                                          OScPosition & scpos)
{
  if (!object_type)
  {
    return nullptr;
  }

  OTypeFunc * tfunc = new OTypeFunc("Destroy");
  OValSymFunc * dtor = new OValSymFunc(scpos, "Destroy", tfunc, object_type->Members());
  dtor->owner_compound_type = object_type;
  dtor->generated_linkage_name = object_type->name + ".Destroy";
  dtor->object_specfunc_kind = OSF_DESTROY;
  dtor->member_visibility = inherited_dtor ? inherited_dtor->member_visibility : MV_PUBLIC;
  dtor->has_body = true;
  dtor->scpos_endfunc.Assign(scpos);

  InjectObjectReceiver(dtor, object_type);
  PrepareFuncDecl(scpos, dtor);

  if (inherited_dtor)
  {
    auto * stmt = new OStmtInheritedCall(scpos, dtor, inherited_dtor);
    stmt->emit_derived_field_destroy = true;
    dtor->body->stlist.push_back(stmt);
  }

  object_type->destructor = dtor;
  object_type->Members()->DefineValSym(dtor);
  g_module->DeclareHiddenValSym(true, dtor);

  return dtor;
}

bool ODqCompAst::CheckObjectCtorArgs(OTypeObject * object_type, vector<OExpr *> & rargs, OValSymFunc *& rctor)
{
  rctor = nullptr;
  if (!object_type)
  {
    return false;
  }

  if (object_type->HasTrivialDefaultConstructor() && rargs.empty())
  {
    return true;
  }

  bool ambiguous = false;
  rctor = object_type->FindConstructorForArgs(rargs, &ambiguous);
  if (!rctor)
  {
    Error(ambiguous ? DQERR_OVERLOAD_AMBIGUOUS : DQERR_OVERLOAD_NO_MATCH, "Create");
    return false;
  }

  OTypeFunc * sigtype = dynamic_cast<OTypeFunc *>(rctor->ptype);
  if (!sigtype || (sigtype->params.size() != rargs.size() + 1))
  {
    Error(DQERR_OVERLOAD_NO_MATCH, "Create");
    return false;
  }

  for (size_t i = 0; i < rargs.size(); ++i)
  {
    if (!CheckAssignType(sigtype->params[i + 1]->ptype, &rargs[i], "constructor argument"))
    {
      return false;
    }
  }

  return true;
}

OValSymFunc * ODqCompAst::FindInheritedMethod(const string & method_name, const vector<OExpr *> & args)
{
  auto * owner_object = dynamic_cast<OTypeObject *>(curvsfunc ? curvsfunc->owner_compound_type : nullptr);
  if (!owner_object || !owner_object->base_type)
  {
    return nullptr;
  }

  for (OTypeObject * cur = owner_object->GetBaseObject(); cur; cur = cur->GetBaseObject())
  {
    if ("Create" == method_name)
    {
      OValSymFunc * fn = cur->FindSpecialMethod(OSF_CREATE, args.size());
      return (ObjectMemberAccessAllowed(cur, fn) ? fn : nullptr);
    }
    if ("Destroy" == method_name)
    {
      OValSymFunc * fn = (args.empty() ? cur->FindSpecialMethod(OSF_DESTROY) : nullptr);
      return (ObjectMemberAccessAllowed(cur, fn) ? fn : nullptr);
    }

    OValSym * vs = cur->Members()->FindValSym(method_name, nullptr, false);
    if (auto * fn = dynamic_cast<OValSymFunc *>(vs))
    {
      if (!ObjectMemberAccessAllowed(cur, fn))
      {
        return nullptr;
      }
      OTypeFunc * sig = dynamic_cast<OTypeFunc *>(fn->ptype);
      if (sig && sig->params.size() == args.size() + 1)
      {
        return fn;
      }
    }
    if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
    {
      if (!ObjectMemberAccessAllowed(cur, ovset))
      {
        return nullptr;
      }
      for (OValSymFunc * fn : ovset->funcs)
      {
        OTypeFunc * sig = dynamic_cast<OTypeFunc *>(fn ? fn->ptype : nullptr);
        if (sig && sig->params.size() == args.size() + 1)
        {
          return fn;
        }
      }
    }
  }
  return nullptr;
}

void ODqCompAst::FreeRawCallArguments(vector<TRawCallArg> & rawargs)
{
  for (TRawCallArg & rawarg : rawargs)
  {
    OExpr::DeleteTree(rawarg.expr);
    rawarg.expr = nullptr;
    rawarg.init_diags.clear();
  }

  rawargs.clear();
}

void ODqCompAst::EmitStoredVarInitDiags(const vector<TSuppressedVarInitDiag> & diags)
{
  for (const auto & diag : diags)
  {
    OScPosition scpos = diag.scpos;
    if (diag.valsym->IsRefLike() && (FPM_REFOUT == diag.valsym->param_mode))
    {
      Error(DQERR_REFOUT_READ_BEFORE_WRITE, diag.valsym->name, &scpos);
    }
    else
    {
      Error(DQERR_VAR_NOT_INITIALIZED, diag.valsym->name, &scpos);
    }
  }
}

OExpr * ODqCompAst::CreateImplicitObjectMemberExpr(const string & sid, OValSym * vs, OScope * found_scope)
{
  if (!curvsfunc || !curvsfunc->owner_compound_type || !curvsfunc->receiver_arg || !vs || !found_scope)
  {
    return nullptr;
  }

  if (VSK_FUNCTION == vs->kind)
  {
    return nullptr;
  }

  OCompoundType * decl_type = nullptr;
  if (auto * object_type = dynamic_cast<OTypeObject *>(curvsfunc->owner_compound_type))
  {
    for (OTypeObject * cur = object_type; cur; cur = cur->GetBaseObject())
    {
      if (found_scope == cur->Members())
      {
        decl_type = cur;
        break;
      }
    }
  }
  else
  {
    for (OCompoundType * cur = curvsfunc->owner_compound_type; cur; cur = cur->base_type)
    {
      if (found_scope == cur->Members())
      {
        decl_type = cur;
        break;
      }
    }
  }
  if (!decl_type)
  {
    return nullptr;
  }

  int midx = decl_type->FindMemberIndex(sid);
  if (midx < 0)
  {
    return nullptr;
  }
  if (!ObjectMemberAccessAllowed(decl_type, vs))
  {
    Error(DQERR_MEMBER_UNKNOWN, sid, curvsfunc->owner_compound_type->name);
    return nullptr;
  }

  return new OLValueMember(new OLValueVar(curvsfunc->receiver_arg), decl_type, midx, vs->ptype);
}

bool ODqCompAst::ObjectMemberAccessAllowed(OCompoundType * decl_type, OValSym * member) const
{
  if (!member || MV_PUBLIC == member->member_visibility)
  {
    return true;
  }

  OCompoundType * curtype = (curvsfunc ? curvsfunc->owner_compound_type : nullptr);
  if (!curtype || !decl_type)
  {
    return false;
  }

  if (MV_PRIVATE == member->member_visibility)
  {
    return curtype == decl_type;
  }

  return curtype->IsSameOrDerivedFrom(decl_type);
}

bool ODqCompAst::BindCallArguments(const string & callname, OTypeFunc * tfunc, vector<TRawCallArg> & rawargs, vector<OExpr *> & rargs)
{

  if (!tfunc)
  {
    Error(DQERR_EXPR_NOT_CALLABLE, callname);
    return false;
  }

  bool        bok = true;
  size_t      required_param_count = tfunc->RequiredParamCount();

  for (size_t pcnt = 0; pcnt < rawargs.size(); ++pcnt)
  {
    if (pcnt >= tfunc->params.size() && !tfunc->has_varargs)
    {
      Error(DQERR_FUNC_ARGS_TOO_MANY, callname, to_string(tfunc->params.size()));
      bok = false;
      break;
    }

    TRawCallArg & rawarg = rawargs[pcnt];
    OExpr * argexpr = rawarg.expr;
    rawarg.expr = nullptr;

    OFuncParam * fparam = ((pcnt < tfunc->params.size()) ? tfunc->params[pcnt] : nullptr);
    bool is_ref_arg = (fparam && fparam->IsRefLike());

    if (!argexpr)
    {
      bok = false;
      break;
    }

    if (!is_ref_arg)
    {
      if (!rawarg.init_diags.empty())
      {
        EmitStoredVarInitDiags(rawarg.init_diags);
        OExpr::DeleteTree(argexpr);
        bok = false;
        break;
      }

      rargs.push_back(argexpr);
      if (pcnt < tfunc->params.size())
      {
        OType * argtype = tfunc->params[pcnt]->ptype;
        if (!ConvertExprToType(argtype, &argexpr,
                               EXPCF_GENERATE_ERRORS | EXPCF_ALLOW_LAZY_CSTRING | EXPCF_ALLOW_ARRAY_LITERAL_SLICE))
        {
          bok = false;
          break;
        }
        // CheckAssignType may have replaced argexpr (e.g. array->slice conversion)
        rargs[pcnt] = argexpr;
      }
    }
    else
    {
      bool is_null_arg = dynamic_cast<ONullLit *>(argexpr);
      if (is_null_arg)
      {
        if (FPM_REFNULL != fparam->mode)
        {
          Error(DQERR_FUNC_ARG_REF_NULL, to_string(pcnt + 1), callname);
          OExpr::DeleteTree(argexpr);
          bok = false;
          break;
        }

        rargs.push_back(argexpr);
      }
      else
      {
        OLValueExpr * arglval = dynamic_cast<OLValueExpr *>(argexpr);
        OValSym * rootvalsym = (arglval ? GetAssignRootValSym(arglval) : nullptr);
        bool bind_ok = (arglval != nullptr);
        if (bind_ok && rootvalsym)
        {
          if ((VSK_CONST == rootvalsym->kind) || !rootvalsym->IsRefWriteable())
          {
            bind_ok = false;
          }
        }

        if (!bind_ok)
        {
          Error(DQERR_FUNC_ARG_REF_BIND, to_string(pcnt + 1), callname);
          OExpr::DeleteTree(argexpr);
          bok = false;
          break;
        }

        if (!OTypeFunc::SameRefBindingType(fparam->ptype, argexpr->ptype))
        {
          string type_text = format("{} = {}", fparam->ptype->name, argexpr->ptype->name);
          ErrorTxt(DQERR_FUNC_ARG_REF_TYPE,
                   format("Reference argument {} type mismatch for function \"{}\": {}", pcnt + 1, callname, type_text));
          OExpr::DeleteTree(argexpr);
          bok = false;
          break;
        }

        if (!rawarg.init_diags.empty() && ((FPM_REF == fparam->mode) || (FPM_REFIN == fparam->mode) || (FPM_REFNULL == fparam->mode)))
        {
          OValSym * uninitvs = rawarg.init_diags[0].valsym;
          Error(DQERR_FUNC_ARG_REF_UNINIT, uninitvs->name);
          OExpr::DeleteTree(argexpr);
          bok = false;
          break;
        }

        OTypeObject * ref_object_type = dynamic_cast<OTypeObject *>(fparam->ptype ? fparam->ptype->ResolveAlias() : nullptr);
        if (ref_object_type)
        {
          bool hidden_receiver_arg = (0 == pcnt) && ("__this" == fparam->name);
          auto * root_obj = dynamic_cast<OVsObject *>(rootvalsym);
          if (!hidden_receiver_arg && root_obj && root_obj->IsFixedObjectStorage()
              && (FPM_REFIN != fparam->mode))
          {
            Error(DQERR_FUNC_ARG_REF_BIND, to_string(pcnt + 1), callname);
            OExpr::DeleteTree(argexpr);
            bok = false;
            break;
          }
          if (hidden_receiver_arg)
          {
            rargs.push_back(new OObjectAddrExpr(arglval));
          }
          else if (arglval->IsObjectReferenceExpr())
          {
            rargs.push_back(new OAddrOfExpr(arglval));
          }
          else
          {
            rargs.push_back(new OObjectAddrExpr(arglval));
          }
        }
        else
        {
          rargs.push_back(new OAddrOfExpr(arglval));
        }

        if ((FPM_REFOUT == fparam->mode) && curblock && rootvalsym
            && (VSK_VARIABLE == rootvalsym->kind || VSK_PARAMETER == rootvalsym->kind))
        {
          curblock->scope->SetVarInitialized(rootvalsym);
        }
      }
    }
  }

  if (bok && (rawargs.size() < required_param_count))
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(rawargs.size()), callname, to_string(required_param_count));
    bok = false;
  }

  while (bok && (rargs.size() < tfunc->params.size()))
  {
    OFuncParam * fparam = tfunc->params[rargs.size()];
    if (!fparam->defvalue)
    {
      Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(rawargs.size()), callname, to_string(required_param_count));
      bok = false;
      break;
    }

    rargs.push_back(new OLValueVar(fparam->defvalue));
  }

  if (!bok)
  {
    for (OExpr *& arg : rargs)
    {
      OExpr::DeleteTree(arg);
      arg = nullptr;
    }
    rargs.clear();
    return false;
  }

  return true;
}

void ODqCompAst::VarInitError(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos)
{
  if (supress_varinit_check)
  {
    AddSuppressedVarInitDiag(varexpr, valsym, scpos);
  }
  else
  {
    Error(DQERR_VAR_NOT_INITIALIZED, valsym->name, &scpos);
  }
}

void ODqCompAst::AddSuppressedVarInitDiag(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos)
{
  TSuppressedVarInitDiag diag;
  diag.varexpr = varexpr;
  diag.valsym = valsym;
  diag.scpos = scpos;
  suppressed_varinit_diags.push_back(diag);
}

void ODqCompAst::EmitSuppressedVarInitDiags()
{
  if (suppressed_varinit_diags.empty())
  {
    return;
  }

  for (auto & diag : suppressed_varinit_diags)
  {
    if (diag.valsym->IsRefLike() && (FPM_REFOUT == diag.valsym->param_mode))
    {
      Error(DQERR_REFOUT_READ_BEFORE_WRITE, diag.valsym->name, &diag.scpos);
    }
    else
    {
      Error(DQERR_VAR_NOT_INITIALIZED, diag.valsym->name, &diag.scpos);
    }
  }

  suppressed_varinit_diags.clear();
}

void ODqCompAst::EmitFilteredAssignVarInitDiags(OLValueExpr * leftexpr, EBinOp op)
{
  if (suppressed_varinit_diags.empty())
  {
    return;
  }

  vector<OLValueVar *> ignored;
  if (BINOP_NONE == op)
  {
    CollectIgnoredPlainAssignVars(leftexpr, ignored);
  }

  for (auto & diag : suppressed_varinit_diags)
  {
    bool emit = true;

    if (BINOP_NONE == op)
    {
      for (OLValueVar * ignoredvar : ignored)
      {
        if (ignoredvar == diag.varexpr)
        {
          emit = false;
          break;
        }
      }
    }

    if (emit)
    {
      if (diag.valsym->IsRefLike() && (FPM_REFOUT == diag.valsym->param_mode))
      {
        Error(DQERR_REFOUT_READ_BEFORE_WRITE, diag.valsym->name, &diag.scpos);
      }
      else
      {
        Error(DQERR_VAR_NOT_INITIALIZED, diag.valsym->name, &diag.scpos);
      }
    }
  }

  suppressed_varinit_diags.clear();
}

bool ODqCompAst::FinalizeStmtAssign(OLValueExpr * leftexpr, EBinOp op, OExpr * rightexpr)
{
  if (!leftexpr || !rightexpr)
  {
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  OValSym * rootvalsym = GetAssignRootValSym(leftexpr);
  if (rootvalsym && VSK_CONST == rootvalsym->kind)
  {
    Error(DQERR_TYPE_ASSIGN_TO_CONST, rootvalsym->name);
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  if (rootvalsym && !rootvalsym->IsRefWriteable())
  {
    Error(DQERR_REF_ASSIGN_READONLY, rootvalsym->name);
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  if (leftexpr->IsFixedObjectStorageExpr())
  {
    Error(DQERR_REF_ASSIGN_READONLY, rootvalsym ? rootvalsym->name : "?");
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  OType * targettype = leftexpr->ptype;
  if (auto * idx = dynamic_cast<OLValueIndex *>(leftexpr))
  {
    OType * ctype = idx->containertype ? idx->containertype->ResolveAlias() : nullptr;
    if (ctype && TK_STRVIEW == ctype->kind)
    {
      Error(DQERR_LVALUE_NOT_WRITEABLE);
      delete leftexpr;
      delete rightexpr;
      return false;
    }
  }

  // Pointer arithmetic: p += int  or  p -= int
  if (TK_POINTER == targettype->kind and (BINOP_ADD == op or BINOP_SUB == op))
  {
    OTypePointer * ptrtype = static_cast<OTypePointer *>(targettype->ResolveAlias());
    if (!ptrtype->IsTypedPointer())
    {
      Error(DQERR_PTR_OPAQUE_USAGE, "pointer arithmetic");
      delete leftexpr;
      delete rightexpr;
      return false;
    }

    if (TK_INT != rightexpr->ptype->kind)
    {
      Error(DQERR_PTRARITH_TYPE, rightexpr->ptype->name);
      delete leftexpr;
      delete rightexpr;
      return false;
    }

    curblock->AddStatement(new OStmtModifyAssign(scpos_statement_start, leftexpr, op, rightexpr));
    return true;
  }

  if (not CheckAssignType(targettype, &rightexpr, "Assignment"))
  {
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  if (BINOP_NONE == op)
  {
    curblock->AddStatement(new OStmtAssign(scpos_statement_start, leftexpr, rightexpr));
    if (rootvalsym && (VSK_VARIABLE == rootvalsym->kind || VSK_PARAMETER == rootvalsym->kind))
    {
      curblock->scope->SetVarInitialized(rootvalsym);
    }
  }
  else
  {
    curblock->AddStatement(new OStmtModifyAssign(scpos_statement_start, leftexpr, op, rightexpr));
  }

  return true;
}
