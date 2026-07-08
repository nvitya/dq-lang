/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 */

#include "otype_enum.h"

#include "dqc.h"
#include "dqm_if.h"
#include "expressions.h"
#include "named_scopes.h"
#include "scope_builtins.h"

using namespace std;

static constexpr const char * ENUM_RTL_NAMESPACE = "__dq_enum";

bool EnsureEnumRtlUse()
{
  if (g_namespaces.end() != g_namespaces.find(ENUM_RTL_NAMESPACE))
  {
    return true;
  }
  return g_compiler->AddImplicitUse("rtl/enum", ENUM_RTL_NAMESPACE, nullptr, true, MUM_NONE);
}

static OValSymFunc * EnumRtlFunc(const string & name)
{
  auto nsit = g_namespaces.find(ENUM_RTL_NAMESPACE);
  if (nsit == g_namespaces.end() || !nsit->second)
  {
    throw runtime_error("Enum RTL module is not loaded");
  }
  auto * fn = dynamic_cast<OValSymFunc *>(nsit->second->FindValSym(name, nullptr, false));
  if (!fn || !fn->ll_func)
  {
    throw runtime_error("Enum RTL function is not available: " + name);
  }
  return fn;
}

LlConst * OValueEnum::CreateLlConst()
{
  return llvm::ConstantInt::get(ptype->GetLlType(), value);
}

bool OValueEnum::CalculateConstant(OExpr * expr, bool emit_errors)
{
  auto * enum_expr = dynamic_cast<OEnumValueExpr *>(expr);
  if (!enum_expr || enum_expr->ResolvedType() != ResolvedType())
  {
    if (emit_errors) g_compiler->Error(DQERR_CONSTEXPR_INVALID_FOR, ptype->name);
    return false;
  }
  value = enum_expr->value;
  return true;
}

bool OValueEnum::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecU64(DQMIF_VALUE_INLINE, value);
}

const SEnumItem * OTypeEnum::FindItem(const string & aname) const
{
  for (const SEnumItem & item : items)
  {
    if (item.name == aname) return &item;
  }
  return nullptr;
}

bool OTypeEnum::HasValue(uint64_t avalue) const
{
  for (const SEnumItem & item : items)
  {
    if (item.value == avalue) return true;
  }
  return false;
}

LlType * OTypeEnum::CreateLlType()
{
  return storage_type->GetLlType();
}

LlDiType * OTypeEnum::CreateDiType()
{
  return storage_type->GetDiType();
}

OValue * OTypeEnum::CreateValue()
{
  return new OValueEnum(this, items.empty() ? 0 : items.front().value);
}

bool OTypeEnum::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  if (auto * unresolved = dynamic_cast<OUnresolvedEnumItemExpr *>(*rexpr))
  {
    const SEnumItem * item = FindItem(unresolved->item_name);
    if (!item)
    {
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ENUM_TYPE_INFER, unresolved->item_name);
      return false;
    }
    delete unresolved;
    *rexpr = new OEnumValueExpr(this, item->value);
    return true;
  }

  if (aflags & EXPCF_EXPLICIT_CAST)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_TYPEMISM, name, (*rexpr)->ptype->name);
  }
  else if (aflags & EXPCF_GENERATE_ERRORS)
  {
    g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", name, (*rexpr)->ptype->name);
  }
  return false;
}

int OTypeEnum::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  (void)aflags;
  if (auto * unresolved = dynamic_cast<OUnresolvedEnumItemExpr *>(expr))
  {
    return FindItem(unresolved->item_name) ? 0 : -1;
  }
  return -1;
}

bool OTypeEnum::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (!writer.AddRecStr(DQMIF_ENUM_BEGIN, name)) return false;
  if (!storage_type->WriteDqmIfTypeSpec(writer)) return false;
  for (const SEnumItem & item : items)
  {
    if (!writer.AddRecStr(DQMIF_ENUM_ITEM_NAME, item.name)) return false;
    if (!writer.AddRecU64(DQMIF_ENUM_ITEM_VALUE, item.value)) return false;
  }
  return writer.AddRecEmpty(DQMIF_ENUM_END);
}

OEnumValueExpr::OEnumValueExpr(OTypeEnum * atype, uint64_t avalue) : value(avalue)
{
  ptype = atype;
}

LlValue * OEnumValueExpr::Generate(OScope * scope)
{
  (void)scope;
  return llvm::ConstantInt::get(ptype->GetLlType(), value);
}

OUnresolvedEnumItemExpr::OUnresolvedEnumItemExpr(const string & aname) : item_name(aname)
{
  ptype = nullptr;
}

OEnumOrdExpr::OEnumOrdExpr(OExpr * asource) : source(asource)
{
  auto * enum_type = static_cast<OTypeEnum *>(source->ResolvedType());
  ptype = enum_type->storage_type;
}

LlValue * OEnumOrdExpr::Generate(OScope * scope)
{
  return source->Generate(scope);
}

void OEnumOrdExpr::FoldChildren()
{
  OExpr::FoldTree(&source);
}

void OEnumOrdExpr::DeleteChildTree()
{
  OExpr::DeleteTree(source);
  source = nullptr;
}

OEnumFromOrdExpr::OEnumFromOrdExpr(EEnumFromOrdKind akind, OTypeEnum * atype, OExpr * avalue)
:
  kind(akind),
  enum_type(atype),
  value_expr(avalue)
{
  ptype = (EFOK_TRY == kind ? static_cast<OType *>(g_builtins->type_bool)
                            : static_cast<OType *>(enum_type));
}

static LlValue * CastIntegerValue(LlValue * value, OTypeInt * src_type, OTypeInt * dst_type)
{
  if (src_type->bitlength < dst_type->bitlength)
  {
    return src_type->issigned
        ? ll_builder.CreateSExt(value, dst_type->GetLlType())
        : ll_builder.CreateZExt(value, dst_type->GetLlType());
  }
  if (src_type->bitlength > dst_type->bitlength)
  {
    return ll_builder.CreateTrunc(value, dst_type->GetLlType());
  }
  return value;
}

static LlValue * EnumValidity(OScope * scope, OTypeEnum * enum_type, OExpr * value_expr,
                              LlValue *& rconverted)
{
  auto * src_type = static_cast<OTypeInt *>(value_expr->ResolvedType());
  LlValue * original = value_expr->Generate(scope);
  rconverted = CastIntegerValue(original, src_type, enum_type->storage_type);

  LlValue * fits = llvm::ConstantInt::getTrue(ll_ctx);
  if (src_type->bitlength > enum_type->storage_type->bitlength)
  {
    LlValue * roundtrip = enum_type->storage_type->issigned
        ? ll_builder.CreateSExt(rconverted, src_type->GetLlType())
        : ll_builder.CreateZExt(rconverted, src_type->GetLlType());
    fits = ll_builder.CreateICmpEQ(original, roundtrip, "enum.ord.fits");
  }
  if (src_type->issigned && !enum_type->storage_type->issigned)
  {
    LlValue * zero = llvm::ConstantInt::get(src_type->GetLlType(), 0);
    fits = ll_builder.CreateAnd(fits,
        ll_builder.CreateICmpSGE(original, zero), "enum.ord.nonnegative");
  }
  else if (!src_type->issigned && enum_type->storage_type->issigned
           && src_type->bitlength >= enum_type->storage_type->bitlength)
  {
    uint64_t max_signed = (enum_type->storage_type->bitlength == 64)
        ? uint64_t(INT64_MAX)
        : ((uint64_t(1) << (enum_type->storage_type->bitlength - 1)) - 1);
    LlValue * max_value = llvm::ConstantInt::get(src_type->GetLlType(), max_signed);
    fits = ll_builder.CreateAnd(fits,
        ll_builder.CreateICmpULE(original, max_value), "enum.ord.signed_range");
  }

  LlValue * declared = llvm::ConstantInt::getFalse(ll_ctx);
  for (const SEnumItem & item : enum_type->items)
  {
    LlValue * item_value = llvm::ConstantInt::get(enum_type->GetLlType(), item.value);
    declared = ll_builder.CreateOr(declared,
        ll_builder.CreateICmpEQ(rconverted, item_value), "enum.ord.declared");
  }
  return ll_builder.CreateAnd(fits, declared, "enum.ord.valid");
}

LlValue * OEnumFromOrdExpr::Generate(OScope * scope)
{
  LlValue * converted = nullptr;
  LlValue * valid = EnumValidity(scope, enum_type, value_expr, converted);

  if (EFOK_DEFAULT == kind)
  {
    return ll_builder.CreateSelect(valid, converted, default_expr->Generate(scope), "enum.fromord");
  }

  if (EFOK_TRY == kind)
  {
    LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
    LlBasicBlock * valid_bb = LlBasicBlock::Create(ll_ctx, "enum.try.valid", ll_func);
    LlBasicBlock * done_bb = LlBasicBlock::Create(ll_ctx, "enum.try.done", ll_func);
    ll_builder.CreateCondBr(valid, valid_bb, done_bb);
    ll_builder.SetInsertPoint(valid_bb);
    ll_builder.CreateStore(converted, output_expr->GenerateAddress(scope));
    ll_builder.CreateBr(done_bb);
    ll_builder.SetInsertPoint(done_bb);
    return valid;
  }

  LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * invalid_bb = LlBasicBlock::Create(ll_ctx, "enum.fromord.invalid", ll_func);
  LlBasicBlock * done_bb = LlBasicBlock::Create(ll_ctx, "enum.fromord.done", ll_func);
  ll_builder.CreateCondBr(valid, done_bb, invalid_bb);
  ll_builder.SetInsertPoint(invalid_bb);
  OValSymFunc * fn = EnumRtlFunc("DqEnumInvalidOrd");
  if (scope)
  {
    scope->GenerateCallOrInvoke(static_cast<LlFuncType *>(fn->ptype->GetLlType()), fn->ll_func, {});
  }
  else
  {
    ll_builder.CreateCall(fn->ll_func, {});
  }
  
  EmitExpressionExceptionCheck(scope);
  if (!ll_builder.GetInsertBlock()->getTerminator()) ll_builder.CreateBr(done_bb);
  ll_builder.SetInsertPoint(done_bb);
  return converted;
}

void OEnumFromOrdExpr::FoldChildren()
{
  OExpr::FoldTree(&value_expr);
  OExpr::FoldTree(&default_expr);
}

void OEnumFromOrdExpr::DeleteChildTree()
{
  OExpr::DeleteTree(value_expr);
  OExpr::DeleteTree(default_expr);
  OExpr::DeleteTree(output_expr);
  value_expr = nullptr;
  default_expr = nullptr;
  output_expr = nullptr;
}
