/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_constexpr.cpp
 * authors: nvitya
 * created: 2026-11-25
 * brief:   Constexpr AST-template symbol, expansion, and .dqm_if expression I/O
 */

#include "otype_constexpr.h"

#include <cstring>
#include <format>

#include "dq_module.h"
#include "dqc_ast.h"
#include "dqm_if.h"
#include "expressions.h"
#include "module_intf.h"
#include "otype_array.h"
#include "otype_bool.h"
#include "otype_compound.h"
#include "otype_embstr.h"
#include "otype_enum.h"
#include "otype_float.h"
#include "otype_int.h"
#include "otype_string.h"

OValSymConstExpr::OValSymConstExpr(OScPosition & apos, const string & aname, OTypeFunc * asigtype)
:
  super(apos, aname, asigtype, VSK_CONSTEXPR)
{
  initialized = true;
}

OValSymConstExpr::~OValSymConstExpr()
{
  OExpr::DeleteTree(body_expr);
  body_expr = nullptr;
  for (OValSym * ps : param_syms) delete ps;
  param_syms.clear();
  delete ptype;
  ptype = nullptr;
}

void OValSymConstExpr::BuildParamSyms()
{
  for (OValSym * ps : param_syms) delete ps;
  param_syms.clear();
  OTypeFunc * sig = GetTypeFunc();
  if (!sig) return;
  for (OFuncParam * fp : sig->params)
  {
    OValSym * ps = fp->ptype->CreateValSym(scpos, fp->name);
    ps->kind = VSK_PARAMETER;
    ps->param_mode = fp->mode;
    ps->initialized = true;
    param_syms.push_back(ps);
  }
}

int OValSymConstExpr::FindParamIndex(const OValSym * avsym) const
{
  for (size_t i = 0; i < param_syms.size(); ++i)
  {
    if (param_syms[i] == avsym) return int(i);
  }
  return -1;
}

OExpr * OValSymConstExpr::Expand(vector<OExpr *> & args, const OScPosition & callpos) const
{
  if (!body_expr) return nullptr;

  SConstExprSubstCtx ctx;
  ctx.cexpr = this;
  ctx.args = &args;
  ctx.remaining_uses.assign(param_syms.size(), 0);
  ctx.callpos = callpos;
  body_expr->CountParamUses(this, ctx.remaining_uses);

  OExpr * expanded = body_expr->CloneSubstitute(ctx);
  if (!expanded || ctx.error)
  {
    OExpr::DeleteTree(expanded);
    return nullptr;
  }

  OExpr::FoldTree(&expanded);
  return expanded;
}

bool OValSymConstExpr::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  OTypeFunc * sig = GetTypeFunc();
  if (!sig || !sig->rettype || !body_expr)
  {
    return writer.Fail(format("Constexpr {} is incomplete", name));
  }
  if (!writer.AddRecStr(DQMIF_CONSTEXPR_BEGIN, name)) return false;
  string declaration_pos = scpos.Format();
  if (declaration_pos.empty()) declaration_pos = declaration_position;
  if (!declaration_pos.empty() && !writer.AddRecStr(DQMIF_DECL_SOURCE_POS, declaration_pos)) return false;
  if (!WriteDqmIfAttributes(writer)) return false;
  if (!sig->WriteDqmIfSignatureRecords(writer)) return false;
  if (!body_expr->WriteDqmIfExpr(writer, this)) return false;
  return writer.AddRecEmpty(DQMIF_CONSTEXPR_END);
}

OExpr * OEnumValueExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  (void)ctx;
  return new OEnumValueExpr(static_cast<OTypeEnum *>(ptype->ResolveAlias()), value);
}

bool OEnumValueExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  (void)cexpr;
  if (!writer.AddRecU64(DQMIF_EXPR_ENUM_LIT, value)) return false;
  return ptype->WriteDqmIfTypeSpec(writer);
}

void OEnumValueExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr,
                                      const function<void(OType *)> & on_type,
                                      const function<void(OValSym *)> & on_sym) const
{
  (void)cexpr;
  (void)on_sym;
  on_type(ptype);
}


// --- constexpr AST substitution and .dqm_if expression serialization ---

static OLValueExpr * UnwrapSliceLValue(OExpr * expr, OType *& rcontainertype)
{
  if (auto * arr_slice = dynamic_cast<OArrayToSliceExpr *>(expr))
  {
    OLValueExpr * lval = arr_slice->arrayexpr;
    arr_slice->arrayexpr = nullptr;
    delete arr_slice;
    if (lval) rcontainertype = lval->ptype;
    return lval;
  }
  if (auto * dyn_slice = dynamic_cast<ODynArrayToSliceExpr *>(expr))
  {
    OLValueExpr * lval = dyn_slice->arrayexpr;
    dyn_slice->arrayexpr = nullptr;
    delete dyn_slice;
    if (lval) rcontainertype = lval->ptype;
    return lval;
  }
  if (auto * lval = dynamic_cast<OLValueExpr *>(expr))
  {
    return lval;
  }
  delete expr;
  return nullptr;
}

static bool IsFoldableScalarConst(const OValSym * avsym)
{
  auto * vsconst = dynamic_cast<const OValSymConst *>(avsym);
  if (!vsconst || !vsconst->pvalue) return false;
  OValue * v = vsconst->pvalue;
  if (dynamic_cast<OValueInt *>(v) || dynamic_cast<OValueFloat *>(v)
      || dynamic_cast<OValueBool *>(v) || dynamic_cast<OValueEnum *>(v)
      || dynamic_cast<OValueEmbStr *>(v) || dynamic_cast<OValueTextLiteral *>(v))
  {
    return true;
  }
  if (auto * ptr = dynamic_cast<OValuePointer *>(v))
  {
    return !ptr->const_addr.symbol;
  }
  if (auto * fn = dynamic_cast<OValueFuncRef *>(v))
  {
    return fn->is_null;
  }
  return false;
}

static bool WriteFoldedScalarConstExpr(ODqmIfWriter & writer, const OValSymConst * vsconst)
{
  OValue * v = vsconst->pvalue;
  if (auto * vi = dynamic_cast<OValueInt *>(v))
  {
    return writer.AddRecI64(DQMIF_EXPR_INT_LIT, vi->value) && vsconst->ptype->WriteDqmIfTypeSpec(writer);
  }
  if (auto * vf = dynamic_cast<OValueFloat *>(v))
  {
    uint64_t bits = 0;
    memcpy(&bits, &vf->value, sizeof(bits));
    return writer.AddRecU64(DQMIF_EXPR_FLOAT_LIT, bits) && vsconst->ptype->WriteDqmIfTypeSpec(writer);
  }
  if (auto * vb = dynamic_cast<OValueBool *>(v))
  {
    return writer.AddRecU8(DQMIF_EXPR_BOOL_LIT, vb->value ? 1 : 0);
  }
  if (auto * ve = dynamic_cast<OValueEnum *>(v))
  {
    return writer.AddRecU64(DQMIF_EXPR_ENUM_LIT, ve->value) && vsconst->ptype->WriteDqmIfTypeSpec(writer);
  }
  if (auto * vs = dynamic_cast<OValueEmbStr *>(v))
  {
    return writer.AddRecStr(DQMIF_EXPR_STR_LIT, vs->value);
  }
  if (auto * vt = dynamic_cast<OValueTextLiteral *>(v))
  {
    const string * txt = vt->literal.GetStringLiteral();
    return writer.AddRecEmpty(DQMIF_EXPR_TEXT_BORROW)
        && vsconst->ptype->WriteDqmIfTypeSpec(writer)
        && writer.AddRecStr(DQMIF_EXPR_STR_LIT, txt ? *txt : "");
  }
  if (auto * vp = dynamic_cast<OValuePointer *>(v))
  {
    if (const string * txt = vp->GetStringLiteral())
    {
      return writer.AddRecStr(DQMIF_EXPR_STR_LIT, *txt);
    }
    if (0 == vp->const_addr.address)
    {
      return writer.AddRecEmpty(DQMIF_EXPR_NULL_LIT);
    }
    return writer.AddRecEmpty(DQMIF_EXPR_TYPE_CONV)
        && vsconst->ptype->WriteDqmIfTypeSpec(writer)
        && writer.AddRecI64(DQMIF_EXPR_INT_LIT, int64_t(vp->const_addr.address))
        && g_builtins->native_uint->WriteDqmIfTypeSpec(writer);
  }
  if (auto * vfn = dynamic_cast<OValueFuncRef *>(v); vfn && vfn->is_null)
  {
    return writer.AddRecEmpty(DQMIF_EXPR_NULL_LIT);
  }
  return false;
}

bool OArrayMetaFieldExpr::TryFoldSelf(OExpr ** rreplacement)
{
  if ((AMF_LENGTH == field) && containertype && (TK_ARRAY == containertype->ResolveAlias()->kind))
  {
    auto * arrtype = static_cast<OTypeArray *>(containertype->ResolveAlias());
    *rreplacement = new OIntLit(int64_t(arrtype->arraylength), g_builtins->type_int);
    return true;
  }
  return false;
}

void OExprTypeConv::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (src) src->CountParamUses(cexpr, counts);
}
OExpr * OExprTypeConv::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * ns = src ? src->CloneSubstitute(ctx) : nullptr;
  return ns ? new OExprTypeConv(ptype, ns) : nullptr;
}
bool OExprTypeConv::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return ptype && src && src->CanSerializeDqmIfExpr(cexpr);
}
bool OExprTypeConv::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecEmpty(DQMIF_EXPR_TYPE_CONV)
      && ptype->WriteDqmIfTypeSpec(writer)
      && src->WriteDqmIfExpr(writer, cexpr);
}
void OExprTypeConv::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                     const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  if (src) src->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

OExpr * OIntLit::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  (void)ctx;
  auto * res = new OIntLit(value, ptype);
  res->is_plain_literal = is_plain_literal;
  return res;
}
bool OIntLit::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  (void)cexpr;
  return writer.AddRecI64(DQMIF_EXPR_INT_LIT, value) && ptype->WriteDqmIfTypeSpec(writer);
}
void OIntLit::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                               const function<void(OValSym *)> & on_sym) const
{
  (void)cexpr; (void)on_sym;
  on_type(ptype);
}

OExpr * OFloatLit::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  (void)ctx;
  return new OFloatLit(value, ptype);
}
bool OFloatLit::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  (void)cexpr;
  uint64_t bits = 0;
  memcpy(&bits, &value, sizeof(bits));
  return writer.AddRecU64(DQMIF_EXPR_FLOAT_LIT, bits) && ptype->WriteDqmIfTypeSpec(writer);
}
void OFloatLit::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                 const function<void(OValSym *)> & on_sym) const
{
  (void)cexpr; (void)on_sym;
  on_type(ptype);
}

OExpr * OBoolLit::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  (void)ctx;
  return new OBoolLit(value, ptype);
}
bool OBoolLit::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  (void)cexpr;
  return writer.AddRecU8(DQMIF_EXPR_BOOL_LIT, value ? 1 : 0);
}

OExpr * ONullLit::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  (void)ctx;
  auto * res = new ONullLit();
  res->ptype = ptype;
  return res;
}
bool ONullLit::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  (void)cexpr;
  return writer.AddRecEmpty(DQMIF_EXPR_NULL_LIT);
}

OExpr * OEmbStrLit::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  (void)ctx;
  auto * res = new OEmbStrLit(value);
  res->ptype = ptype;
  return res;
}
bool OEmbStrLit::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  (void)cexpr;
  return writer.AddRecStr(DQMIF_EXPR_STR_LIT, value);
}

void OLValueVar::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  int idx = cexpr ? cexpr->FindParamIndex(pvalsym) : -1;
  if (idx >= 0 && size_t(idx) < counts.size())
  {
    ++counts[size_t(idx)];
  }
}
OExpr * OLValueVar::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  int idx = ctx.cexpr ? ctx.cexpr->FindParamIndex(pvalsym) : -1;
  if (idx >= 0 && ctx.args && size_t(idx) < ctx.args->size())
  {
    OExpr *& arg = (*ctx.args)[size_t(idx)];
    if (!arg)
    {
      ctx.error = true;
      return nullptr;
    }
    if (size_t(idx) < ctx.remaining_uses.size() && ctx.remaining_uses[size_t(idx)] <= 1)
    {
      OExpr * moved = arg;
      arg = nullptr;
      ctx.remaining_uses[size_t(idx)] = 0;
      return moved;
    }
    --ctx.remaining_uses[size_t(idx)];
    SConstExprSubstCtx empty_ctx;
    OExpr * cloned = arg->CloneSubstitute(empty_ctx);
    if (!cloned)
    {
      ctx.error = true;
    }
    return cloned;
  }
  return new OLValueVar(pvalsym);
}
bool OLValueVar::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  if (!pvalsym) return false;
  if (cexpr && cexpr->FindParamIndex(pvalsym) >= 0) return true;
  if (IsFoldableScalarConst(pvalsym)) return true;
  return !pvalsym->owner_function && !pvalsym->IsRefLike();
}
bool OLValueVar::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  int idx = cexpr ? cexpr->FindParamIndex(pvalsym) : -1;
  if (idx >= 0)
  {
    return writer.AddRecU32(DQMIF_EXPR_PARAM, uint32_t(idx));
  }
  if (IsFoldableScalarConst(pvalsym))
  {
    return WriteFoldedScalarConstExpr(writer, static_cast<const OValSymConst *>(pvalsym));
  }
  string mod_name = pvalsym->owner_module_name.empty()
      ? (pvalsym->module ? pvalsym->module->name : (g_module ? g_module->name : ""))
      : pvalsym->owner_module_name;
  return writer.AddRecStringPair(DQMIF_EXPR_SYMBOL, mod_name, pvalsym->name);
}
void OLValueVar::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                  const function<void(OValSym *)> & on_sym) const
{
  if (!pvalsym || (cexpr && cexpr->FindParamIndex(pvalsym) >= 0)) return;
  if (IsFoldableScalarConst(pvalsym))
  {
    on_type(pvalsym->ptype);
    return;
  }
  on_sym(pvalsym);
}

void OLValueDeref::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (ptrexpr) ptrexpr->CountParamUses(cexpr, counts);
}
OExpr * OLValueDeref::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * np = ptrexpr ? ptrexpr->CloneSubstitute(ctx) : nullptr;
  return np ? new OLValueDeref(np) : nullptr;
}
bool OLValueDeref::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return ptrexpr && ptrexpr->CanSerializeDqmIfExpr(cexpr);
}
bool OLValueDeref::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecEmpty(DQMIF_EXPR_DEREF) && ptrexpr->WriteDqmIfExpr(writer, cexpr);
}
void OLValueDeref::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                    const function<void(OValSym *)> & on_sym) const
{
  if (ptrexpr) ptrexpr->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OLValueMember::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (base) base->CountParamUses(cexpr, counts);
}
OExpr * OLValueMember::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * nb = base ? base->CloneSubstitute(ctx) : nullptr;
  auto * lval = dynamic_cast<OLValueExpr *>(nb);
  if (!lval)
  {
    delete nb;
    ctx.error = true;
    return nullptr;
  }
  return new OLValueMember(lval, structtype, memberindex, membersym);
}
bool OLValueMember::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return base && structtype && base->CanSerializeDqmIfExpr(cexpr);
}
bool OLValueMember::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecU32(DQMIF_EXPR_MEMBER, memberindex)
      && structtype->WriteDqmIfTypeSpec(writer)
      && base->WriteDqmIfExpr(writer, cexpr);
}
void OLValueMember::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                     const function<void(OValSym *)> & on_sym) const
{
  on_type(structtype);
  if (base) base->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OLValueIndex::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (base) base->CountParamUses(cexpr, counts);
  if (indexexpr) indexexpr->CountParamUses(cexpr, counts);
}
OExpr * OLValueIndex::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * nb = base ? base->CloneSubstitute(ctx) : nullptr;
  OType * ctype = containertype;
  OLValueExpr * lval = UnwrapSliceLValue(nb, ctype);
  if (!lval)
  {
    ctx.error = true;
    return nullptr;
  }
  OExpr * ni = indexexpr ? indexexpr->CloneSubstitute(ctx) : nullptr;
  if (!ni)
  {
    delete lval;
    return nullptr;
  }
  return new OLValueIndex(lval, ctype, ni);
}
bool OLValueIndex::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return base && containertype && indexexpr
      && base->CanSerializeDqmIfExpr(cexpr) && indexexpr->CanSerializeDqmIfExpr(cexpr);
}
bool OLValueIndex::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecEmpty(DQMIF_EXPR_INDEX)
      && containertype->WriteDqmIfTypeSpec(writer)
      && base->WriteDqmIfExpr(writer, cexpr)
      && indexexpr->WriteDqmIfExpr(writer, cexpr);
}
void OLValueIndex::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                    const function<void(OValSym *)> & on_sym) const
{
  on_type(containertype);
  if (base) base->CollectDqmIfDeps(cexpr, on_type, on_sym);
  if (indexexpr) indexexpr->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OBinExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (left) left->CountParamUses(cexpr, counts);
  if (right) right->CountParamUses(cexpr, counts);
}
OExpr * OBinExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * nl = left ? left->CloneSubstitute(ctx) : nullptr;
  OExpr * nr = nl && right ? right->CloneSubstitute(ctx) : nullptr;
  if (!nl || !nr)
  {
    delete nl;
    delete nr;
    return nullptr;
  }
  auto * res = new OBinExpr(op, nl, nr);
  res->ptype = ptype;
  return res;
}
bool OBinExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return ptype && left && right && left->CanSerializeDqmIfExpr(cexpr) && right->CanSerializeDqmIfExpr(cexpr);
}
bool OBinExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecU32(DQMIF_EXPR_BIN, uint32_t(op))
      && ptype->WriteDqmIfTypeSpec(writer)
      && left->WriteDqmIfExpr(writer, cexpr)
      && right->WriteDqmIfExpr(writer, cexpr);
}
void OBinExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  if (left) left->CollectDqmIfDeps(cexpr, on_type, on_sym);
  if (right) right->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OCompareExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (left) left->CountParamUses(cexpr, counts);
  if (right) right->CountParamUses(cexpr, counts);
}
OExpr * OCompareExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * nl = left ? left->CloneSubstitute(ctx) : nullptr;
  OExpr * nr = nl && right ? right->CloneSubstitute(ctx) : nullptr;
  if (!nl || !nr)
  {
    delete nl;
    delete nr;
    return nullptr;
  }
  return new OCompareExpr(op, nl, nr);
}
bool OCompareExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return left && right && left->CanSerializeDqmIfExpr(cexpr) && right->CanSerializeDqmIfExpr(cexpr);
}
bool OCompareExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecU32(DQMIF_EXPR_COMPARE, uint32_t(op))
      && left->WriteDqmIfExpr(writer, cexpr)
      && right->WriteDqmIfExpr(writer, cexpr);
}
void OCompareExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                    const function<void(OValSym *)> & on_sym) const
{
  if (left) left->CollectDqmIfDeps(cexpr, on_type, on_sym);
  if (right) right->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OIifExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (condition) condition->CountParamUses(cexpr, counts);
  if (true_expr) true_expr->CountParamUses(cexpr, counts);
  if (false_expr) false_expr->CountParamUses(cexpr, counts);
}
OExpr * OIifExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * nc = condition ? condition->CloneSubstitute(ctx) : nullptr;
  OExpr * nt = nc && true_expr ? true_expr->CloneSubstitute(ctx) : nullptr;
  OExpr * nf = nt && false_expr ? false_expr->CloneSubstitute(ctx) : nullptr;
  if (!nc || !nt || !nf)
  {
    delete nc;
    delete nt;
    delete nf;
    return nullptr;
  }
  return new OIifExpr(nc, nt, nf, ptype);
}
bool OIifExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return ptype && condition && true_expr && false_expr
      && condition->CanSerializeDqmIfExpr(cexpr)
      && true_expr->CanSerializeDqmIfExpr(cexpr)
      && false_expr->CanSerializeDqmIfExpr(cexpr);
}
bool OIifExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecEmpty(DQMIF_EXPR_IIF)
      && ptype->WriteDqmIfTypeSpec(writer)
      && condition->WriteDqmIfExpr(writer, cexpr)
      && true_expr->WriteDqmIfExpr(writer, cexpr)
      && false_expr->WriteDqmIfExpr(writer, cexpr);
}
void OIifExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  if (condition) condition->CollectDqmIfDeps(cexpr, on_type, on_sym);
  if (true_expr) true_expr->CollectDqmIfDeps(cexpr, on_type, on_sym);
  if (false_expr) false_expr->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OLogicalExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (left) left->CountParamUses(cexpr, counts);
  if (right) right->CountParamUses(cexpr, counts);
}
OExpr * OLogicalExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * nl = left ? left->CloneSubstitute(ctx) : nullptr;
  OExpr * nr = nl && right ? right->CloneSubstitute(ctx) : nullptr;
  if (!nl || !nr)
  {
    delete nl;
    delete nr;
    return nullptr;
  }
  return new OLogicalExpr(op, nl, nr);
}
bool OLogicalExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return left && right && left->CanSerializeDqmIfExpr(cexpr) && right->CanSerializeDqmIfExpr(cexpr);
}
bool OLogicalExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecU8(DQMIF_EXPR_LOGICAL, uint8_t(op))
      && left->WriteDqmIfExpr(writer, cexpr)
      && right->WriteDqmIfExpr(writer, cexpr);
}
void OLogicalExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                    const function<void(OValSym *)> & on_sym) const
{
  if (left) left->CollectDqmIfDeps(cexpr, on_type, on_sym);
  if (right) right->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void ONotExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (operand) operand->CountParamUses(cexpr, counts);
}
OExpr * ONotExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * no = operand ? operand->CloneSubstitute(ctx) : nullptr;
  return no ? new ONotExpr(no) : nullptr;
}
bool ONotExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return operand && operand->CanSerializeDqmIfExpr(cexpr);
}
bool ONotExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecU8(DQMIF_EXPR_UNARY, 2)
      && ptype->WriteDqmIfTypeSpec(writer)
      && operand->WriteDqmIfExpr(writer, cexpr);
}
void ONotExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                const function<void(OValSym *)> & on_sym) const
{
  if (operand) operand->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OBinNotExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (operand) operand->CountParamUses(cexpr, counts);
}
OExpr * OBinNotExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * no = operand ? operand->CloneSubstitute(ctx) : nullptr;
  if (!no) return nullptr;
  auto * res = new OBinNotExpr(no);
  res->ptype = ptype;
  return res;
}
bool OBinNotExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return ptype && operand && operand->CanSerializeDqmIfExpr(cexpr);
}
bool OBinNotExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecU8(DQMIF_EXPR_UNARY, 1)
      && ptype->WriteDqmIfTypeSpec(writer)
      && operand->WriteDqmIfExpr(writer, cexpr);
}
void OBinNotExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                   const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  if (operand) operand->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void ONegExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (operand) operand->CountParamUses(cexpr, counts);
}
OExpr * ONegExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * no = operand ? operand->CloneSubstitute(ctx) : nullptr;
  if (!no) return nullptr;
  auto * res = new ONegExpr(no);
  res->ptype = ptype;
  return res;
}
bool ONegExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return ptype && operand && operand->CanSerializeDqmIfExpr(cexpr);
}
bool ONegExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecU8(DQMIF_EXPR_UNARY, 0)
      && ptype->WriteDqmIfTypeSpec(writer)
      && operand->WriteDqmIfExpr(writer, cexpr);
}
void ONegExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  if (operand) operand->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OAddrOfExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (target) target->CountParamUses(cexpr, counts);
}
OExpr * OAddrOfExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * nt = target ? target->CloneSubstitute(ctx) : nullptr;
  auto * lval = dynamic_cast<OLValueExpr *>(nt);
  if (!lval)
  {
    delete nt;
    ctx.error = true;
    return nullptr;
  }
  return new OAddrOfExpr(lval);
}
bool OAddrOfExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return target && target->CanSerializeDqmIfExpr(cexpr);
}
bool OAddrOfExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecEmpty(DQMIF_EXPR_ADDROF) && target->WriteDqmIfExpr(writer, cexpr);
}
void OAddrOfExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                   const function<void(OValSym *)> & on_sym) const
{
  if (target) target->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OPointerIndexExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (ptrexpr) ptrexpr->CountParamUses(cexpr, counts);
  if (indexexpr) indexexpr->CountParamUses(cexpr, counts);
}
OExpr * OPointerIndexExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * np = ptrexpr ? ptrexpr->CloneSubstitute(ctx) : nullptr;
  OExpr * ni = np && indexexpr ? indexexpr->CloneSubstitute(ctx) : nullptr;
  if (!np || !ni)
  {
    delete np;
    delete ni;
    return nullptr;
  }
  return new OPointerIndexExpr(np, ni);
}
bool OPointerIndexExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return ptrexpr && indexexpr && ptrexpr->CanSerializeDqmIfExpr(cexpr) && indexexpr->CanSerializeDqmIfExpr(cexpr);
}
bool OPointerIndexExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecEmpty(DQMIF_EXPR_PTR_INDEX)
      && ptrexpr->WriteDqmIfExpr(writer, cexpr)
      && indexexpr->WriteDqmIfExpr(writer, cexpr);
}
void OPointerIndexExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                         const function<void(OValSym *)> & on_sym) const
{
  if (ptrexpr) ptrexpr->CollectDqmIfDeps(cexpr, on_type, on_sym);
  if (indexexpr) indexexpr->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OArrayToSliceExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (arrayexpr) arrayexpr->CountParamUses(cexpr, counts);
}
OExpr * OArrayToSliceExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * na = arrayexpr ? arrayexpr->CloneSubstitute(ctx) : nullptr;
  auto * lval = dynamic_cast<OLValueExpr *>(na);
  if (!lval)
  {
    delete na;
    ctx.error = true;
    return nullptr;
  }
  return new OArrayToSliceExpr(lval, ptype);
}

void ODynArrayToSliceExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (arrayexpr) arrayexpr->CountParamUses(cexpr, counts);
}
OExpr * ODynArrayToSliceExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * na = arrayexpr ? arrayexpr->CloneSubstitute(ctx) : nullptr;
  auto * lval = dynamic_cast<OLValueExpr *>(na);
  if (!lval)
  {
    delete na;
    ctx.error = true;
    return nullptr;
  }
  return new ODynArrayToSliceExpr(lval, ptype);
}

void OArrayMetaFieldExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (target) target->CountParamUses(cexpr, counts);
}
OExpr * OArrayMetaFieldExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * nt = target ? target->CloneSubstitute(ctx) : nullptr;
  OType * ctype = containertype;
  OLValueExpr * lval = UnwrapSliceLValue(nt, ctype);
  if (!lval)
  {
    ctx.error = true;
    return nullptr;
  }
  return new OArrayMetaFieldExpr(lval, ctype, field);
}
bool OArrayMetaFieldExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return target && containertype && target->CanSerializeDqmIfExpr(cexpr);
}
bool OArrayMetaFieldExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecU8(DQMIF_EXPR_ARRAY_META, uint8_t(field))
      && containertype->WriteDqmIfTypeSpec(writer)
      && target->WriteDqmIfExpr(writer, cexpr);
}
void OArrayMetaFieldExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                           const function<void(OValSym *)> & on_sym) const
{
  on_type(containertype);
  if (target) target->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OFloatRoundExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (src) src->CountParamUses(cexpr, counts);
}
OExpr * OFloatRoundExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * ns = src ? src->CloneSubstitute(ctx) : nullptr;
  if (!ns) return nullptr;
  auto * res = new OFloatRoundExpr(mode, ns);
  res->ptype = ptype;
  return res;
}
bool OFloatRoundExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return ptype && src && src->CanSerializeDqmIfExpr(cexpr);
}
bool OFloatRoundExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecU8(DQMIF_EXPR_ROUND, uint8_t(mode))
      && ptype->WriteDqmIfTypeSpec(writer)
      && src->WriteDqmIfExpr(writer, cexpr);
}
void OFloatRoundExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                       const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  if (src) src->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OBoundMethodExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (receiver) receiver->CountParamUses(cexpr, counts);
}
OExpr * OBoundMethodExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * nr = receiver ? receiver->CloneSubstitute(ctx) : nullptr;
  auto * lval = dynamic_cast<OLValueExpr *>(nr);
  if (!lval)
  {
    delete nr;
    ctx.error = true;
    return nullptr;
  }
  auto * res = new OBoundMethodExpr(vsfunc, lval);
  res->ptype = ptype;
  return res;
}
bool OBoundMethodExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return vsfunc && receiver && ptype && receiver->CanSerializeDqmIfExpr(cexpr);
}
bool OBoundMethodExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecStr(DQMIF_EXPR_BOUND_METHOD, vsfunc->name)
      && ptype->WriteDqmIfTypeSpec(writer)
      && receiver->WriteDqmIfExpr(writer, cexpr);
}
void OBoundMethodExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                        const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  if (receiver) receiver->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OArrayLit::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  for (OExpr * e : elements) if (e) e->CountParamUses(cexpr, counts);
}
OExpr * OArrayLit::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  vector<OExpr *> new_elems;
  new_elems.reserve(elements.size());
  for (OExpr * e : elements)
  {
    OExpr * ne = e ? e->CloneSubstitute(ctx) : nullptr;
    if (!ne)
    {
      for (OExpr * x : new_elems) delete x;
      return nullptr;
    }
    new_elems.push_back(ne);
  }
  auto * res = new OArrayLit(new_elems);
  res->ptype = ptype;
  return res;
}
bool OArrayLit::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  if (!ptype) return false;
  for (OExpr * e : elements)
  {
    if (!e || !e->CanSerializeDqmIfExpr(cexpr)) return false;
  }
  return true;
}
bool OArrayLit::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  if (!writer.AddRecEmpty(DQMIF_EXPR_ARRAY_LIT_BEGIN)) return false;
  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  for (OExpr * e : elements)
  {
    if (!e->WriteDqmIfExpr(writer, cexpr)) return false;
  }
  return writer.AddRecEmpty(DQMIF_EXPR_ARRAY_LIT_END);
}
void OArrayLit::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                 const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  for (OExpr * e : elements) if (e) e->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OStructLit::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  for (const OStructLitEntry & e : entries) if (e.value) e.value->CountParamUses(cexpr, counts);
}
OExpr * OStructLit::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  vector<OStructLitEntry> new_entries;
  new_entries.reserve(entries.size());
  for (const OStructLitEntry & e : entries)
  {
    OExpr * nv = e.value ? e.value->CloneSubstitute(ctx) : nullptr;
    if (e.value && !nv)
    {
      for (OStructLitEntry & ne : new_entries) delete ne.value;
      return nullptr;
    }
    new_entries.push_back({e.name, nv, e.field, e.ll_path});
  }
  auto * res = new OStructLit(std::move(new_entries), fill_missing);
  res->ptype = ptype;
  return res;
}
bool OStructLit::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  if (!ptype) return false;
  for (const OStructLitEntry & e : entries)
  {
    if (!e.value || !e.value->CanSerializeDqmIfExpr(cexpr)) return false;
  }
  return true;
}
bool OStructLit::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  if (!ptype) return writer.Fail("Constexpr struct literal has no type");
  if (!writer.AddRecU8(DQMIF_EXPR_STRUCT_LIT_BEGIN, fill_missing ? 1 : 0)) return false;
  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  if (!entries.empty())
  {
    auto * ctype = dynamic_cast<OCompoundType *>(ptype->ResolveAlias());
    if (!ctype) return writer.Fail("Constexpr struct literal has no compound type");
    vector<SStructInitField> init_fields;
    ctype->CollectInitFields({}, init_fields);
    for (const OStructLitEntry & e : entries)
    {
      uint32_t fidx = UINT32_MAX;
      for (uint32_t i = 0; i < init_fields.size(); ++i)
      {
        if (init_fields[i].field == e.field)
        {
          fidx = i;
          break;
        }
      }
      if (UINT32_MAX == fidx) return writer.Fail("Constexpr struct literal entry has unresolved field");
      if (!writer.AddRecU32(DQMIF_EXPR_STRUCT_ENTRY, fidx)) return false;
      if (!e.value->WriteDqmIfExpr(writer, cexpr)) return false;
    }
  }
  return writer.AddRecEmpty(DQMIF_EXPR_STRUCT_LIT_END);
}
void OStructLit::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                  const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  for (const OStructLitEntry & e : entries) if (e.value) e.value->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

void OTextBorrowExpr::CountParamUses(const OValSymConstExpr * cexpr, vector<uint32_t> & counts) const
{
  if (source) source->CountParamUses(cexpr, counts);
}
OExpr * OTextBorrowExpr::CloneSubstitute(SConstExprSubstCtx & ctx) const
{
  OExpr * ns = source ? source->CloneSubstitute(ctx) : nullptr;
  return ns ? new OTextBorrowExpr(ns, ptype) : nullptr;
}
bool OTextBorrowExpr::CanSerializeDqmIfExpr(const OValSymConstExpr * cexpr) const
{
  return ptype && source && source->CanSerializeDqmIfExpr(cexpr);
}
bool OTextBorrowExpr::WriteDqmIfExpr(ODqmIfWriter & writer, const OValSymConstExpr * cexpr) const
{
  return writer.AddRecEmpty(DQMIF_EXPR_TEXT_BORROW)
      && ptype->WriteDqmIfTypeSpec(writer)
      && source->WriteDqmIfExpr(writer, cexpr);
}
void OTextBorrowExpr::CollectDqmIfDeps(const OValSymConstExpr * cexpr, const function<void(OType *)> & on_type,
                                       const function<void(OValSym *)> & on_sym) const
{
  on_type(ptype);
  if (source) source->CollectDqmIfDeps(cexpr, on_type, on_sym);
}

// --- .dqm_if constexpr deserialization ---

bool OModuleIntf::ReadDqmIfExpr(ODqmIfReader & reader, const OValSymConstExpr * cexpr, OExpr *& rexpr)
{
  rexpr = nullptr;
  switch (reader.recid)
  {
    case DQMIF_EXPR_INT_LIT:
    {
      int64_t val = 0;
      OType * t = nullptr;
      if (!reader.ReadI64(val) || !reader.NextRec() || !ReadTypeSpec(reader, t)) return false;
      rexpr = new OIntLit(val, t);
      return true;
    }
    case DQMIF_EXPR_FLOAT_LIT:
    {
      uint64_t bits = 0;
      double val = 0.0;
      OType * t = nullptr;
      if (!reader.ReadU64(bits) || !reader.NextRec() || !ReadTypeSpec(reader, t)) return false;
      memcpy(&val, &bits, sizeof(val));
      rexpr = new OFloatLit(val, t);
      return true;
    }
    case DQMIF_EXPR_BOOL_LIT:
    {
      uint8_t val = 0;
      if (!reader.ReadU8(val)) return false;
      rexpr = new OBoolLit(val != 0);
      return true;
    }
    case DQMIF_EXPR_NULL_LIT:
    {
      if (!reader.ExpectEmpty(DQMIF_EXPR_NULL_LIT)) return false;
      rexpr = new ONullLit();
      return true;
    }
    case DQMIF_EXPR_STR_LIT:
    {
      string val;
      if (!reader.ReadString(val)) return false;
      rexpr = new OEmbStrLit(val);
      return true;
    }
    case DQMIF_EXPR_ENUM_LIT:
    {
      uint64_t val = 0;
      OType * t = nullptr;
      if (!reader.ReadU64(val) || !reader.NextRec() || !ReadTypeSpec(reader, t)) return false;
      auto * etype = dynamic_cast<OTypeEnum *>(t ? t->ResolveAlias() : nullptr);
      if (!etype) return reader.Fail("DQM interface enum literal has non-enum type");
      rexpr = new OEnumValueExpr(etype, val);
      rexpr->ptype = t;
      return true;
    }
    case DQMIF_EXPR_PARAM:
    {
      uint32_t idx = 0;
      if (!reader.ReadU32(idx)) return false;
      if (!cexpr || idx >= cexpr->param_syms.size())
      {
        return reader.Fail(format("DQM interface constexpr parameter index {} out of range", idx));
      }
      rexpr = new OLValueVar(cexpr->param_syms[idx]);
      return true;
    }
    case DQMIF_EXPR_SYMBOL:
    {
      string mod_name, sym_name;
      if (!reader.ReadStringPair(mod_name, sym_name)) return false;
      OValSym * sym = ResolveDqmIfQualifiedValSym(mod_name, sym_name);
      if (!sym)
      {
        return reader.Fail(format("Unknown DQM interface symbol {}.{} in constexpr", mod_name, sym_name));
      }
      rexpr = new OLValueVar(sym);
      return true;
    }
    case DQMIF_EXPR_TYPE_CONV:
    {
      OType * dst = nullptr;
      OExpr * src = nullptr;
      if (!reader.ExpectEmpty(DQMIF_EXPR_TYPE_CONV) || !reader.NextRec() || !ReadTypeSpec(reader, dst)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, src))
      {
        delete src;
        return false;
      }
      rexpr = new OExprTypeConv(dst, src);
      return true;
    }
    case DQMIF_EXPR_BIN:
    {
      uint32_t raw_op = 0;
      OType * res_type = nullptr;
      OExpr * left = nullptr;
      OExpr * right = nullptr;
      if (!reader.ReadU32(raw_op) || !reader.NextRec() || !ReadTypeSpec(reader, res_type)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, left)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, right))
      {
        delete left;
        delete right;
        return false;
      }
      auto * bin = new OBinExpr(EBinOp(raw_op), left, right);
      bin->ptype = res_type;
      rexpr = bin;
      return true;
    }
    case DQMIF_EXPR_COMPARE:
    {
      uint32_t raw_op = 0;
      OExpr * left = nullptr;
      OExpr * right = nullptr;
      if (!reader.ReadU32(raw_op)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, left)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, right))
      {
        delete left;
        delete right;
        return false;
      }
      rexpr = new OCompareExpr(ECompareOp(raw_op), left, right);
      return true;
    }
    case DQMIF_EXPR_LOGICAL:
    {
      uint8_t raw_op = 0;
      OExpr * left = nullptr;
      OExpr * right = nullptr;
      if (!reader.ReadU8(raw_op)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, left)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, right))
      {
        delete left;
        delete right;
        return false;
      }
      rexpr = new OLogicalExpr(ELogicalOp(raw_op), left, right);
      return true;
    }
    case DQMIF_EXPR_UNARY:
    {
      uint8_t kind = 0;
      OType * res_type = nullptr;
      OExpr * opnd = nullptr;
      if (!reader.ReadU8(kind) || !reader.NextRec() || !ReadTypeSpec(reader, res_type)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, opnd))
      {
        delete opnd;
        return false;
      }
      if (0 == kind)      rexpr = new ONegExpr(opnd);
      else if (1 == kind) rexpr = new OBinNotExpr(opnd);
      else if (2 == kind) rexpr = new ONotExpr(opnd);
      else { delete opnd; return reader.Fail("Invalid DQM interface unary expression kind"); }
      rexpr->ptype = res_type;
      return true;
    }
    case DQMIF_EXPR_ROUND:
    {
      uint8_t raw_mode = 0;
      OType * res_type = nullptr;
      OExpr * opnd = nullptr;
      if (!reader.ReadU8(raw_mode) || !reader.NextRec() || !ReadTypeSpec(reader, res_type)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, opnd))
      {
        delete opnd;
        return false;
      }
      rexpr = new OFloatRoundExpr(ERoundMode(raw_mode), opnd);
      rexpr->ptype = res_type;
      return true;
    }
    case DQMIF_EXPR_IIF:
    {
      OType * res_type = nullptr;
      OExpr * cond = nullptr;
      OExpr * texpr = nullptr;
      OExpr * fexpr = nullptr;
      if (!reader.ExpectEmpty(DQMIF_EXPR_IIF) || !reader.NextRec() || !ReadTypeSpec(reader, res_type)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, cond)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, texpr)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, fexpr))
      {
        delete cond;
        delete texpr;
        delete fexpr;
        return false;
      }
      rexpr = new OIifExpr(cond, texpr, fexpr, res_type);
      return true;
    }
    case DQMIF_EXPR_TEXT_BORROW:
    {
      OType * dst = nullptr;
      OExpr * src = nullptr;
      if (!reader.ExpectEmpty(DQMIF_EXPR_TEXT_BORROW) || !reader.NextRec() || !ReadTypeSpec(reader, dst)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, src))
      {
        delete src;
        return false;
      }
      rexpr = new OTextBorrowExpr(src, dst);
      return true;
    }
    case DQMIF_EXPR_STRUCT_LIT_BEGIN:
    {
      uint8_t fill_missing = 0;
      OType * stype = nullptr;
      if (!reader.ReadU8(fill_missing) || !reader.NextRec() || !ReadTypeSpec(reader, stype)) return false;

      vector<SStructInitField> init_fields;
      if (auto * ctype = dynamic_cast<OCompoundType *>(stype ? stype->ResolveAlias() : nullptr))
      {
        ctype->CollectInitFields({}, init_fields);
      }

      vector<OStructLitEntry> entries;
      while (true)
      {
        if (!reader.NextRec())
        {
          for (OStructLitEntry & e : entries) delete e.value;
          return false;
        }
        if (DQMIF_EXPR_STRUCT_LIT_END == reader.recid)
        {
          if (!reader.ExpectEmpty(DQMIF_EXPR_STRUCT_LIT_END))
          {
            for (OStructLitEntry & e : entries) delete e.value;
            return false;
          }
          break;
        }
        if (DQMIF_EXPR_STRUCT_ENTRY != reader.recid)
        {
          for (OStructLitEntry & e : entries) delete e.value;
          return reader.Fail(format("Unexpected record 0x{:04X} in constexpr struct literal", reader.recid));
        }
        uint32_t fidx = 0;
        OExpr * fval = nullptr;
        if (!reader.ReadU32(fidx) || fidx >= init_fields.size()
            || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, fval))
        {
          delete fval;
          for (OStructLitEntry & e : entries) delete e.value;
          return false;
        }
        entries.push_back({init_fields[fidx].field->name, fval, init_fields[fidx].field, init_fields[fidx].ll_path});
      }
      auto * lit = new OStructLit(std::move(entries), fill_missing != 0);
      lit->ptype = stype;
      rexpr = lit;
      return true;
    }
    case DQMIF_EXPR_ARRAY_LIT_BEGIN:
    {
      OType * atype = nullptr;
      if (!reader.ExpectEmpty(DQMIF_EXPR_ARRAY_LIT_BEGIN) || !reader.NextRec() || !ReadTypeSpec(reader, atype))
      {
        return false;
      }
      vector<OExpr *> elems;
      while (true)
      {
        if (!reader.NextRec())
        {
          for (OExpr * e : elems) delete e;
          return false;
        }
        if (DQMIF_EXPR_ARRAY_LIT_END == reader.recid)
        {
          if (!reader.ExpectEmpty(DQMIF_EXPR_ARRAY_LIT_END))
          {
            for (OExpr * e : elems) delete e;
            return false;
          }
          break;
        }
        OExpr * elem = nullptr;
        if (!ReadDqmIfExpr(reader, cexpr, elem))
        {
          delete elem;
          for (OExpr * e : elems) delete e;
          return false;
        }
        elems.push_back(elem);
      }
      auto * lit = new OArrayLit(elems);
      lit->ptype = atype;
      rexpr = lit;
      return true;
    }
    case DQMIF_EXPR_ADDROF:
    {
      OExpr * tgt = nullptr;
      if (!reader.ExpectEmpty(DQMIF_EXPR_ADDROF) || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, tgt))
      {
        delete tgt;
        return false;
      }
      auto * lval = dynamic_cast<OLValueExpr *>(tgt);
      if (!lval) { delete tgt; return reader.Fail("DQM interface address-of target is not an lvalue"); }
      rexpr = new OAddrOfExpr(lval);
      return true;
    }
    case DQMIF_EXPR_MEMBER:
    {
      uint32_t midx = 0;
      OType * stype = nullptr;
      OExpr * base = nullptr;
      if (!reader.ReadU32(midx) || !reader.NextRec() || !ReadTypeSpec(reader, stype)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, base))
      {
        delete base;
        return false;
      }
      auto * lval = dynamic_cast<OLValueExpr *>(base);
      auto * ctype = dynamic_cast<OCompoundType *>(stype ? stype->ResolveAlias() : nullptr);
      if (!lval || !ctype || midx >= ctype->member_order.size())
      {
        delete base;
        return reader.Fail("Invalid DQM interface member expression in constexpr");
      }
      rexpr = new OLValueMember(lval, stype, midx, ctype->member_order[midx]);
      return true;
    }
    case DQMIF_EXPR_INDEX:
    {
      OType * ctype = nullptr;
      OExpr * base = nullptr;
      OExpr * idx = nullptr;
      if (!reader.ExpectEmpty(DQMIF_EXPR_INDEX) || !reader.NextRec() || !ReadTypeSpec(reader, ctype)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, base)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, idx))
      {
        delete base;
        delete idx;
        return false;
      }
      auto * lval = dynamic_cast<OLValueExpr *>(base);
      if (!lval) { delete base; delete idx; return reader.Fail("Invalid DQM interface index base in constexpr"); }
      rexpr = new OLValueIndex(lval, ctype, idx);
      return true;
    }
    case DQMIF_EXPR_DEREF:
    {
      OExpr * ptr = nullptr;
      if (!reader.ExpectEmpty(DQMIF_EXPR_DEREF) || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, ptr))
      {
        delete ptr;
        return false;
      }
      rexpr = new OLValueDeref(ptr);
      return true;
    }
    case DQMIF_EXPR_PTR_INDEX:
    {
      OExpr * ptr = nullptr;
      OExpr * idx = nullptr;
      if (!reader.ExpectEmpty(DQMIF_EXPR_PTR_INDEX)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, ptr)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, idx))
      {
        delete ptr;
        delete idx;
        return false;
      }
      rexpr = new OPointerIndexExpr(ptr, idx);
      return true;
    }
    case DQMIF_EXPR_ARRAY_META:
    {
      uint8_t raw_field = 0;
      OType * ctype = nullptr;
      OExpr * tgt = nullptr;
      if (!reader.ReadU8(raw_field) || !reader.NextRec() || !ReadTypeSpec(reader, ctype)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, tgt))
      {
        delete tgt;
        return false;
      }
      auto * lval = dynamic_cast<OLValueExpr *>(tgt);
      if (!lval) { delete tgt; return reader.Fail("Invalid DQM interface array meta target in constexpr"); }
      rexpr = new OArrayMetaFieldExpr(lval, ctype, EArrayMetaField(raw_field));
      return true;
    }
    case DQMIF_EXPR_BOUND_METHOD:
    {
      string mname;
      OType * ftype = nullptr;
      OExpr * recv = nullptr;
      if (!reader.ReadString(mname) || !reader.NextRec() || !ReadTypeSpec(reader, ftype)
          || !reader.NextRec() || !ReadDqmIfExpr(reader, cexpr, recv))
      {
        delete recv;
        return false;
      }
      auto * lval = dynamic_cast<OLValueExpr *>(recv);
      auto * fref_type = dynamic_cast<OTypeFuncRef *>(ftype ? ftype->ResolveAlias() : nullptr);
      auto * ctype = dynamic_cast<OCompoundType *>(lval && lval->ptype ? lval->ptype->ResolveAlias() : nullptr);
      if (!lval || !fref_type || !ctype)
      {
        delete recv;
        return reader.Fail("Invalid DQM interface bound method in constexpr");
      }
      OValSym * msym = ctype->FindMemberSymbol(mname);
      OValSymFunc * method = dynamic_cast<OValSymFunc *>(msym);
      if (!method)
      {
        if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(msym))
        {
          for (OValSymFunc * fn : ovset->funcs)
          {
            if (fref_type->CanAcceptMethod(fn)) { method = fn; break; }
          }
        }
      }
      if (!method)
      {
        delete recv;
        return reader.Fail(format("Unknown DQM interface bound method {}.{} in constexpr", ctype->name, mname));
      }
      auto * b = new OBoundMethodExpr(method, lval);
      b->ptype = ftype;
      rexpr = b;
      return true;
    }
    default:
      return reader.Fail(format("Unexpected DQM interface constexpr expression record 0x{:04X}", reader.recid));
  }
}

bool OModuleIntf::ReadConstExprDecl(ODqmIfReader & reader)
{
  string declname;
  if (!reader.ReadString(declname) || !reader.NextRec()) return false;

  string declaration_pos;
  if (DQMIF_DECL_SOURCE_POS == reader.recid)
  {
    if (!reader.ReadString(declaration_pos) || !reader.NextRec()) return false;
  }

  SDqmIfAttributes attrs;
  if (!ReadAttributes(reader, attrs)) return false;

  OTypeFunc * sigtype = new OTypeFunc(declname);
  if (DQMIF_FUNC_RETVAL != reader.recid
      || !reader.ExpectEmpty(DQMIF_FUNC_RETVAL)
      || !reader.NextRec()
      || !ReadTypeSpec(reader, sigtype->rettype)
      || !reader.NextRec())
  {
    delete sigtype;
    return reader.Fail(format("DQM interface constexpr {} is missing return type", declname));
  }

  while (DQMIF_FUNC_PARAM_BEGIN == reader.recid)
  {
    if (!ReadFunctionParam(reader, sigtype) || !reader.NextRec())
    {
      delete sigtype;
      return false;
    }
  }

  OScPosition scpos;
  auto * vsym = new OValSymConstExpr(scpos, declname, sigtype);
  vsym->declaration_position = std::move(declaration_pos);
  vsym->owner_module_name = name;
  ApplyDqmIfAttributes(vsym, attrs);
  vsym->BuildParamSyms();

  if (!ReadDqmIfExpr(reader, vsym, vsym->body_expr)
      || !reader.NextRec()
      || !reader.ExpectEmpty(DQMIF_CONSTEXPR_END))
  {
    delete vsym;
    return false;
  }

  if (!AddPublicValSym(vsym))
  {
    delete vsym;
    return false;
  }
  return true;
}

