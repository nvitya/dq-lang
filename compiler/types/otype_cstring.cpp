/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_cstring.cpp
 * authors: nvitya
 * created: 2026-03-08
 * brief:   C-string type implementation
 */

#include <vector>
#include <limits>
#include "otype_cstring.h"
#include "scope_builtins.h"
#include "dqm_if.h"
#include "expressions.h"
#include "dqc.h"

using namespace std;

// OTypeCString

LlType * OTypeCString::CreateLlType()
{
  if (maxlen > 0)
  {
    // Fixed-size: [N x i8]
    return llvm::ArrayType::get(LlType::getInt8Ty(ll_ctx), maxlen);
  }
  else
  {
    // Unsized descriptor: {ptr, i64} (same layout as array slice)
    vector<LlType *> fields = {
      llvm::PointerType::get(ll_ctx, 0),
      LlType::getInt64Ty(ll_ctx)
    };
    return llvm::StructType::get(ll_ctx, fields);
  }
}

LlDiType * OTypeCString::CreateDiType()
{
  if (maxlen > 0)
  {
    // Debug info as array of i8
    LlDiType * elem_di = di_builder->createBasicType("cchar", 8, llvm::dwarf::DW_ATE_signed_char);
    llvm::Metadata * subscripts[] = {
      di_builder->getOrCreateSubrange(0, maxlen)
    };
    return di_builder->createArrayType(
        maxlen * 8, 0, elem_di,
        di_builder->getOrCreateArray(subscripts)
    );
  }
  else
  {
    // Unsized descriptor: struct {ptr, i64}
    LlDiType * ptr_di = di_builder->createPointerType(
        di_builder->createBasicType("cchar", 8, llvm::dwarf::DW_ATE_signed_char),
        TARGET_PTRSIZE * 8);
    LlDiType * len_di = di_builder->createBasicType("uint64", 64, llvm::dwarf::DW_ATE_unsigned);

    llvm::Metadata * elements[] = {
      di_builder->createMemberType(
          nullptr, "ptr", nullptr, 0, TARGET_PTRSIZE * 8, 0,
          0, llvm::DINode::FlagZero, ptr_di),
      di_builder->createMemberType(
          nullptr, "size", nullptr, 0, 64, 0,
          TARGET_PTRSIZE * 8, llvm::DINode::FlagZero, len_di)
    };

    return di_builder->createStructType(
        nullptr, name, nullptr, 0, bytesize * 8, 0,
        llvm::DINode::FlagZero, nullptr,
        di_builder->getOrCreateArray(elements)
    );
  }
}

bool OTypeCString::IsCCharPointerType(OType * type) const
{
  auto * ptrtype = dynamic_cast<OTypePointer *>(type ? type->ResolveAlias() : nullptr);
  return ptrtype
      && ptrtype->IsTypedPointer()
      && ptrtype->basetype
      && (ptrtype->basetype->ResolveAlias() == g_builtins->type_cchar);
}

bool OTypeCString::CanStoreFrom(OExpr * srcexpr) const
{
  if (maxlen <= 0)
  {
    return false;
  }

  if (!srcexpr)
  {
    return true;
  }

  if (dynamic_cast<OCStringLit *>(srcexpr))
  {
    return true;
  }

  OType * srctype = srcexpr->ResolvedType();
  return dynamic_cast<OTypeCString *>(srctype) || IsCCharPointerType(srctype);
}

static void GetCStringCopySource(OScope * scope, OExpr * srcexpr, LlValue *& rsrcptr, LlValue *& rsrclimit)
{
  OTypeCString * srctype = dynamic_cast<OTypeCString *>(srcexpr->ResolvedType());
  if (!srctype)
  {
    rsrcptr = srcexpr->Generate(scope);
    rsrclimit = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), numeric_limits<uint64_t>::max());
    return;
  }

  if (srctype->maxlen > 0)
  {
    LlValue * srcaddr = nullptr;
    if (auto * srclval = dynamic_cast<OLValueExpr *>(srcexpr))
    {
      srcaddr = srclval->GenerateAddress(scope);
    }
    else
    {
      auto * src_alloca = ll_builder.CreateAlloca(srctype->GetLlType(), nullptr, "cstr.src.tmp");
      src_alloca->setAlignment(llvm::Align(EffectiveStorageAlign(srctype)));
      srcaddr = src_alloca;
      ll_builder.CreateStore(srcexpr->Generate(scope), srcaddr);
    }

    LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
    rsrcptr = ll_builder.CreateGEP(srctype->GetLlType(), srcaddr, {ll_zero, ll_zero}, "cstr.src.ptr");
    rsrclimit = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), srctype->maxlen);
    return;
  }

  LlValue * ll_desc = srcexpr->Generate(scope);
  rsrcptr = ll_builder.CreateExtractValue(ll_desc, {0}, "cstr.src.ptr");
  rsrclimit = ll_builder.CreateExtractValue(ll_desc, {1}, "cstr.src.size");
}

static void EmitSizedCStringCopy(OScope * scope, LlValue * dstdaddr, OTypeCString * dsttype, OExpr * srcexpr)
{
  LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
  LlValue * ll_one = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 1);
  LlValue * ll_i8_zero = llvm::ConstantInt::get(LlType::getInt8Ty(ll_ctx), 0);
  LlValue * ll_dstptr = ll_builder.CreateGEP(dsttype->GetLlType(), dstdaddr, {ll_zero, ll_zero}, "cstr.dst.ptr");

  if (dsttype->maxlen <= 1)
  {
    LlValue * ll_dstnull = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_dstptr, {ll_zero}, "cstr.dst.null");
    ll_builder.CreateStore(ll_i8_zero, ll_dstnull);
    return;
  }

  LlValue * ll_srcptr = nullptr;
  LlValue * ll_srclimit = nullptr;
  GetCStringCopySource(scope, srcexpr, ll_srcptr, ll_srclimit);

  LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * entry_bb = ll_builder.GetInsertBlock();
  LlBasicBlock * cond_bb = LlBasicBlock::Create(ll_ctx, "cstr.copy.cond", ll_func);
  LlBasicBlock * load_bb = LlBasicBlock::Create(ll_ctx, "cstr.copy.load", ll_func);
  LlBasicBlock * store_bb = LlBasicBlock::Create(ll_ctx, "cstr.copy.store", ll_func);
  LlBasicBlock * end_bb = LlBasicBlock::Create(ll_ctx, "cstr.copy.end", ll_func);

  LlValue * ll_copy_limit = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), dsttype->maxlen - 1);

  ll_builder.CreateBr(cond_bb);

  ll_builder.SetInsertPoint(cond_bb);
  llvm::PHINode * ll_i = ll_builder.CreatePHI(LlType::getInt64Ty(ll_ctx), 2, "cstr.copy.i");
  ll_i->addIncoming(ll_zero, entry_bb);
  LlValue * ll_dst_room = ll_builder.CreateICmpULT(ll_i, ll_copy_limit, "cstr.copy.dst_room");
  LlValue * ll_src_room = ll_builder.CreateICmpULT(ll_i, ll_srclimit, "cstr.copy.src_room");
  LlValue * ll_can_copy = ll_builder.CreateAnd(ll_dst_room, ll_src_room, "cstr.copy.can_copy");
  ll_builder.CreateCondBr(ll_can_copy, load_bb, end_bb);

  ll_builder.SetInsertPoint(load_bb);
  LlValue * ll_srcchptr = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_srcptr, {ll_i}, "cstr.src.ch.ptr");
  LlValue * ll_srcch = ll_builder.CreateLoad(LlType::getInt8Ty(ll_ctx), ll_srcchptr, "cstr.src.ch");
  LlValue * ll_is_null = ll_builder.CreateICmpEQ(ll_srcch, ll_i8_zero, "cstr.src.is_null");
  ll_builder.CreateCondBr(ll_is_null, end_bb, store_bb);

  ll_builder.SetInsertPoint(store_bb);
  LlValue * ll_dstchptr = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_dstptr, {ll_i}, "cstr.dst.ch.ptr");
  ll_builder.CreateStore(ll_srcch, ll_dstchptr);
  LlValue * ll_i_next = ll_builder.CreateAdd(ll_i, ll_one, "cstr.copy.i.next");
  ll_i->addIncoming(ll_i_next, store_bb);
  ll_builder.CreateBr(cond_bb);

  ll_builder.SetInsertPoint(end_bb);
  llvm::PHINode * ll_term_index = ll_builder.CreatePHI(LlType::getInt64Ty(ll_ctx), 2, "cstr.term.i");
  ll_term_index->addIncoming(ll_i, cond_bb);
  ll_term_index->addIncoming(ll_i, load_bb);
  LlValue * ll_dstnull = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_dstptr, {ll_term_index}, "cstr.dst.null");
  ll_builder.CreateStore(ll_i8_zero, ll_dstnull);
}

bool OTypeCString::GenerateStore(OScope * scope, LlValue * dstdaddr, OExpr * srcexpr)
{
  if (maxlen <= 0)
  {
    return false;
  }

  if (!srcexpr)
  {
    LlConst * ll_zero = llvm::ConstantAggregateZero::get(GetLlType());
    ll_builder.CreateStore(ll_zero, dstdaddr);
    return true;
  }

  if (auto * strlit = dynamic_cast<OCStringLit *>(srcexpr))
  {
    OValueCString val(this, maxlen);
    val.value = strlit->value;
    LlConst * ll_const = val.CreateLlConst();
    ll_builder.CreateStore(ll_const, dstdaddr);
    return true;
  }

  if (CanStoreFrom(srcexpr))
  {
    EmitSizedCStringCopy(scope, dstdaddr, this, srcexpr);
    return true;
  }

  return false;
}

// OValueCString

LlConst * OValueCString::CreateLlConst()
{
  if (maxlen == 0)
  {
    return nullptr;  // unsized type has no constant representation
  }

  // Create [maxlen x i8] constant, padded with zeros
  vector<llvm::Constant *> chars;
  chars.reserve(maxlen);

  LlType * i8type = LlType::getInt8Ty(ll_ctx);

  for (uint32_t i = 0; i < maxlen; ++i)
  {
    if (i < value.size())
    {
      chars.push_back(llvm::ConstantInt::get(i8type, (uint8_t)value[i]));
    }
    else
    {
      chars.push_back(llvm::ConstantInt::get(i8type, 0));
    }
  }

  // Ensure null terminator after string content
  if (value.size() < maxlen)
  {
    // Already covered: chars[value.size()] is 0
  }

  llvm::ArrayType * arrtype = llvm::ArrayType::get(i8type, maxlen);
  return llvm::ConstantArray::get(arrtype, chars);
}

bool OValueCString::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecStr(DQMIF_VALUE_INLINE, value);
}

bool OValueCString::CalculateConstant(OExpr * expr, bool emit_errors)
{
  auto * strlit = dynamic_cast<OCStringLit *>(expr);
  if (strlit)
  {
    value = strlit->value;
    return true;
  }

  if (emit_errors)
  {
    g_compiler->Error(DQERR_CSTR_CONSTEXPR);
  }
  return false;
}
