/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_embstr.cpp
 * authors: nvitya
 * created: 2026-03-08
 * brief:   C-string type implementation
 */

#include <vector>
#include "dqc_ast.h"
#include "otype_embstr.h"
#include "otype_string.h"
#include "rtlint.h"
#include "scope_builtins.h"
#include "dqm_if.h"
#include "expressions.h"
#include "dqc.h"
#include "named_scopes.h"
#include "otype_func.h"

using namespace std;

static LlType * LlPtrType()
{
  return llvm::PointerType::get(ll_ctx, 0);
}

static LlType * LlEmbStrLenType()
{
  return LlType::getInt32Ty(ll_ctx);
}

static LlType * LlEmbStrDescType()
{
  return g_builtins->type_strslice->GetLlType();
}


static LlValue * LlU32(uint32_t value)
{
  return llvm::ConstantInt::get(LlEmbStrLenType(), value);
}

static LlValue * LlNativeInt(uint64_t value)
{
  return llvm::ConstantInt::get(g_builtins->type_int->GetLlType(), value);
}

static LlType * LlNativeIntType()
{
  return g_builtins->type_int->GetLlType();
}



static OValSymFunc * EmbStrFunc(const string & name)
{
  auto nsit = g_namespaces.find("__dq_strfunc");
  if (nsit == g_namespaces.end() || !nsit->second)
  {
    throw runtime_error("EmbStr RTL module is not loaded");
  }

  OValSym * vs = nsit->second->FindValSym(name, nullptr, false);
  auto * fn = dynamic_cast<OValSymFunc *>(vs);
  if (!fn)
  {
    if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
    {
      if (!ovset->funcs.empty())
      {
        fn = ovset->funcs[0];
      }
    }
  }
  if (!fn || !fn->ll_func)
  {
    throw runtime_error("EmbStr RTL function is not available: " + name);
  }
  return fn;
}

static LlValue * CallEmbStrFunc(const string & name, vector<LlValue *> args = {})
{
  OValSymFunc * fn = EmbStrFunc(name);
  return ll_builder.CreateCall(fn->ll_func, args);
}

// OTypeEmbStr

LlType * OTypeEmbStr::CreateLlType()
{
  if (maxlen > 0)
  {
    // Fixed-size storage includes the terminator slot.
    return llvm::ArrayType::get(LlType::getInt8Ty(ll_ctx), maxlen);
  }
  else
  {
    // Unsized aliases share a descriptor instead of copying its cached length.
    return LlPtrType();
  }
}

LlDiType * OTypeEmbStr::CreateDiType()
{
  if (maxlen > 0)
  {
    // Debug info as array of i8
    LlDiType * elem_di = di_builder->createBasicType("char", 8, llvm::dwarf::DW_ATE_signed_char);
    llvm::Metadata * subscripts[] = {
      di_builder->getOrCreateSubrange(0, maxlen)
    };
    return di_builder->createArrayType(
        uint64_t(maxlen) * 8, 0, elem_di,
        di_builder->getOrCreateArray(subscripts)
    );
  }
  else
  {
    return di_builder->createPointerType(
        g_builtins->type_strslice->GetDiType(), TARGET_PTRSIZE * 8);
  }
}

bool OTypeEmbStr::IsCCharPointerType(OType * type) const
{
  return ::IsCCharPointerType(type);
}

bool OTypeEmbStr::CanStoreFrom(OExpr * srcexpr) const
{
  if (maxlen <= 0)
  {
    return false;
  }

  if (!srcexpr)
  {
    return true;
  }

  if (dynamic_cast<OEmbStrLit *>(srcexpr))
  {
    return true;
  }

  OType * srctype = srcexpr->ResolvedType();
  return dynamic_cast<OTypeEmbStr *>(srctype) || IsCCharPointerType(srctype)
      || (srctype && TK_ROSTR == srctype->kind);
}

LlValue * OTypeEmbStr::GenerateDataPtr(OScope * scope, LlValue * embstraddr)
{
  (void)scope;
  if (maxlen > 0)
  {
    LlValue * ll_zero = LlNativeInt(0);
    return ll_builder.CreateGEP(GetLlType(), embstraddr, {ll_zero, ll_zero}, "embstr.data");
  }

  LlValue * descaddr = GenerateDescriptor(scope, embstraddr);
  LlValue * ll_ptr_addr = ll_builder.CreateStructGEP(LlEmbStrDescType(), descaddr, 0, "embstr.ptr.addr");
  return ll_builder.CreateLoad(LlPtrType(), ll_ptr_addr, "embstr.ptr");
}

LlValue * OTypeEmbStr::GenerateDescriptor(OScope * scope, LlValue * embstraddr)
{
  if (maxlen == 0)
  {
    return ll_builder.CreateLoad(GetLlType(), embstraddr, "embstr.desc");
  }

  auto cache_it = descriptor_caches.find(embstraddr);
  if (cache_it != descriptor_caches.end())
  {
    return cache_it->second;
  }

  LlValue * descaddr = CreateEntryBlockAlloca(LlEmbStrDescType(), nullptr, "embstr.desc.tmp");
  LlValue * dataptr = GenerateDataPtr(scope, embstraddr);
  LlType * desctype = LlEmbStrDescType();
  LlValue * ptraddr = ll_builder.CreateStructGEP(desctype, descaddr, 0, "embstr.desc.ptr.addr");
  LlValue * lenaddr = ll_builder.CreateStructGEP(desctype, descaddr, 1, "embstr.desc.len.addr");
  LlValue * infoaddr = ll_builder.CreateStructGEP(desctype, descaddr, 2, "embstr.desc.info.addr");
  ll_builder.CreateStore(dataptr, ptraddr);
  ll_builder.CreateStore(LlU32(DQTIF_CHARLEN_INVALID), lenaddr);
  ll_builder.CreateStore(LlU32((maxlen - 1) & DQTI_MAXCHLEN_MASK), infoaddr);
  descriptor_caches[embstraddr] = descaddr;
  return descaddr;
}

void OTypeEmbStr::ResetDescriptorLength(OScope * scope, LlValue * embstraddr)
{
  LlValue * descaddr = GenerateDescriptor(scope, embstraddr);
  LlValue * lenaddr = ll_builder.CreateStructGEP(LlEmbStrDescType(), descaddr, 1, "embstr.len.addr");
  ll_builder.CreateStore(LlU32(DQTIF_CHARLEN_INVALID), lenaddr);
}

static LlValue * EmbStrSourceDescriptor(OScope * scope, OExpr * srcexpr)
{
  auto * srctype = dynamic_cast<OTypeEmbStr *>(srcexpr->ResolvedType());
  if (!srctype)
  {
    return nullptr;
  }

  if (auto * srclval = dynamic_cast<OLValueExpr *>(srcexpr))
  {
    return srctype->GenerateDescriptor(scope, srclval->GenerateAddress(scope));
  }

  LlValue * tmp = CreateEntryBlockAlloca(srctype->GetLlType(), nullptr, "embstr.src.tmp");
  ll_builder.CreateStore(srcexpr->Generate(scope), tmp);
  return srctype->GenerateDescriptor(scope, tmp);
}

LlValue * OTypeEmbStr::GenerateMetaField(OScope * scope, LlValue * embstraddr, EEmbStrMetaField field)
{
  if (ESMF_PCHAR == field)
  {
    // The caller may change the raw storage before the next descriptor use.
    ResetDescriptorLength(scope, embstraddr);
    return GenerateDataPtr(scope, embstraddr);
  }

  if (maxlen > 0)
  {
    if (ESMF_MAXLENGTH == field)
    {
      return LlNativeInt(maxlen - 1);
    }
    if (ESMF_STORAGE_SIZE == field)
    {
      return LlNativeInt(maxlen);
    }
  }

  LlValue * descaddr = GenerateDescriptor(scope, embstraddr);
  switch (field)
  {
    case ESMF_LENGTH:
      return ToNativeInt(CallEmbStrFunc("EmbStrLen", {descaddr}));
    case ESMF_MAXLENGTH:
      return ToNativeInt(CallEmbStrFunc("EmbStrMaxLen", {descaddr}));
    case ESMF_STORAGE_SIZE:
      return ToNativeInt(CallEmbStrFunc("EmbStrStorageSize", {descaddr}));
  }
  return LlNativeInt(0);
}

static void CallEmbStrStore(OScope * scope, LlValue * dstdesc, OExpr * srcexpr)
{
  OType * srctype = srcexpr->ResolvedType();
  if (srctype && !IsCCharPointerType(srctype) && IsTextSourceType(srctype))
  {
    CallEmbStrFunc("EmbStrAssignDesc", {dstdesc, GenerateTextInfoAddress(scope, srcexpr)});
    return;
  }

  CallEmbStrFunc("EmbStrAssignPtr", {dstdesc, srcexpr->Generate(scope)});
}

static void GetEmbStrCopySource(OScope * scope, OExpr * srcexpr, LlValue *& rsrcptr, LlValue *& rsrclimit)
{
  OTypeEmbStr * srctype = dynamic_cast<OTypeEmbStr *>(srcexpr->ResolvedType());
  if (!srctype)
  {
    rsrcptr = srcexpr->Generate(scope);
    rsrclimit = llvm::ConstantInt::getSigned(LlNativeIntType(), -1);
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
      auto * src_alloca = CreateEntryBlockAlloca(srctype->GetLlType(), nullptr, "embstr.src.tmp");
      src_alloca->setAlignment(llvm::Align(EffectiveStorageAlign(srctype)));
      srcaddr = src_alloca;
      ll_builder.CreateStore(srcexpr->Generate(scope), srcaddr);
    }

    LlValue * ll_zero = LlNativeInt(0);
    rsrcptr = ll_builder.CreateGEP(srctype->GetLlType(), srcaddr, {ll_zero, ll_zero}, "embstr.src.ptr");
    rsrclimit = LlNativeInt(srctype->maxlen - 1);
    return;
  }

  LlValue * descaddr = srcexpr->Generate(scope);
  LlValue * ptraddr = ll_builder.CreateStructGEP(LlEmbStrDescType(), descaddr, 0, "embstr.src.ptr.addr");
  LlValue * lenaddr = ll_builder.CreateStructGEP(LlEmbStrDescType(), descaddr, 1, "embstr.src.len.addr");
  rsrcptr = ll_builder.CreateLoad(LlPtrType(), ptraddr, "embstr.src.ptr");
  rsrclimit = ToNativeInt(ll_builder.CreateLoad(LlEmbStrLenType(), lenaddr, "embstr.src.size"));
}

static void EmitSizedEmbStrCopy(OScope * scope, LlValue * dstdaddr, OTypeEmbStr * dsttype, OExpr * srcexpr)
{
  LlValue * ll_zero = LlNativeInt(0);
  LlValue * ll_one = LlNativeInt(1);
  LlValue * ll_i8_zero = llvm::ConstantInt::get(LlType::getInt8Ty(ll_ctx), 0);
  LlValue * ll_dstptr = ll_builder.CreateGEP(dsttype->GetLlType(), dstdaddr, {ll_zero, ll_zero}, "embstr.dst.ptr");

  if (dsttype->maxlen <= 1)
  {
    LlValue * ll_dstnull = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_dstptr, {ll_zero}, "embstr.dst.null");
    ll_builder.CreateStore(ll_i8_zero, ll_dstnull);
    return;
  }

  LlValue * ll_srcptr = nullptr;
  LlValue * ll_srclimit = nullptr;
  GetEmbStrCopySource(scope, srcexpr, ll_srcptr, ll_srclimit);

  LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * entry_bb = ll_builder.GetInsertBlock();
  LlBasicBlock * cond_bb = LlBasicBlock::Create(ll_ctx, "embstr.copy.cond", ll_func);
  LlBasicBlock * load_bb = LlBasicBlock::Create(ll_ctx, "embstr.copy.load", ll_func);
  LlBasicBlock * store_bb = LlBasicBlock::Create(ll_ctx, "embstr.copy.store", ll_func);
  LlBasicBlock * end_bb = LlBasicBlock::Create(ll_ctx, "embstr.copy.end", ll_func);

  LlValue * ll_copy_limit = LlNativeInt(dsttype->maxlen - 1);

  ll_builder.CreateBr(cond_bb);

  ll_builder.SetInsertPoint(cond_bb);
  llvm::PHINode * ll_i = ll_builder.CreatePHI(LlNativeIntType(), 2, "embstr.copy.i");
  ll_i->addIncoming(ll_zero, entry_bb);
  LlValue * ll_dst_room = ll_builder.CreateICmpULT(ll_i, ll_copy_limit, "embstr.copy.dst_room");
  LlValue * ll_src_room = ll_builder.CreateICmpULT(ll_i, ll_srclimit, "embstr.copy.src_room");
  LlValue * ll_can_copy = ll_builder.CreateAnd(ll_dst_room, ll_src_room, "embstr.copy.can_copy");
  ll_builder.CreateCondBr(ll_can_copy, load_bb, end_bb);

  ll_builder.SetInsertPoint(load_bb);
  LlValue * ll_srcchptr = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_srcptr, {ll_i}, "embstr.src.ch.ptr");
  LlValue * ll_srcch = ll_builder.CreateLoad(LlType::getInt8Ty(ll_ctx), ll_srcchptr, "embstr.src.ch");
  LlValue * ll_is_null = ll_builder.CreateICmpEQ(ll_srcch, ll_i8_zero, "embstr.src.is_null");
  ll_builder.CreateCondBr(ll_is_null, end_bb, store_bb);

  ll_builder.SetInsertPoint(store_bb);
  LlValue * ll_dstchptr = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_dstptr, {ll_i}, "embstr.dst.ch.ptr");
  ll_builder.CreateStore(ll_srcch, ll_dstchptr);
  LlValue * ll_i_next = ll_builder.CreateAdd(ll_i, ll_one, "embstr.copy.i.next");
  ll_i->addIncoming(ll_i_next, store_bb);
  ll_builder.CreateBr(cond_bb);

  ll_builder.SetInsertPoint(end_bb);
  llvm::PHINode * ll_term_index = ll_builder.CreatePHI(LlNativeIntType(), 2, "embstr.term.i");
  ll_term_index->addIncoming(ll_i, cond_bb);
  ll_term_index->addIncoming(ll_i, load_bb);
  LlValue * ll_dstnull = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_dstptr, {ll_term_index}, "embstr.dst.null");
  ll_builder.CreateStore(ll_i8_zero, ll_dstnull);
}

bool OTypeEmbStr::GenerateStore(OScope * scope, LlValue * dstdaddr, OExpr * srcexpr)
{
  if (maxlen <= 0)
  {
    return false;
  }

  if (!srcexpr)
  {
    LlConst * ll_zero = llvm::ConstantAggregateZero::get(GetLlType());
    ll_builder.CreateStore(ll_zero, dstdaddr);
    ResetDescriptorLength(scope, dstdaddr);
    return true;
  }

  if (auto * strlit = dynamic_cast<OEmbStrLit *>(srcexpr))
  {
    OValueEmbStr val(this, maxlen);
    val.value = strlit->value;
    LlConst * ll_const = val.CreateLlConst();
    ll_builder.CreateStore(ll_const, dstdaddr);
    ResetDescriptorLength(scope, dstdaddr);
    return true;
  }

  if (CanStoreFrom(srcexpr))
  {
    LlValue * dstdesc = GenerateDescriptor(scope, dstdaddr);
    CallEmbStrStore(scope, dstdesc, srcexpr);
    return true;
  }

  return false;
}

bool OTypeEmbStr::GenerateAssignment(OScope * scope, LlValue * targetaddr, OExpr * value, bool volatile_store)
{
  if (maxlen > 0)
  {
    return GenerateStore(scope, targetaddr, value);
  }
  return OType::GenerateAssignment(scope, targetaddr, value, volatile_store);
}

static bool IsEmbStrCharSource(OExpr * expr)
{
  OType * type = expr ? expr->ResolvedType() : nullptr;
  return type && type == g_builtins->type_char;
}

static LlValue * GenerateEmbStrMethodSource(OScope * scope, OExpr * expr, const string & ptr_func,
                                             const string & desc_func, const string & char_func,
                                             LlValue * dstdesc, LlValue * index = nullptr)
{
  if (IsEmbStrCharSource(expr))
  {
    vector<LlValue *> args = {dstdesc};
    if (index)
    {
      args.push_back(index);
    }
    args.push_back(ToCharValue(expr->Generate(scope)));
    return CallEmbStrFunc(char_func, args);
  }

  OType * srctype = expr->ResolvedType();
  if (srctype && !IsCCharPointerType(srctype) && IsTextSourceType(srctype))
  {
    vector<LlValue *> args = {dstdesc};
    if (index)
    {
      args.push_back(index);
    }
    args.push_back(GenerateTextInfoAddress(scope, expr));
    return CallEmbStrFunc(desc_func, args);
  }

  vector<LlValue *> args = {dstdesc};
  if (index)
  {
    args.push_back(index);
  }
  args.push_back(expr->Generate(scope));
  return CallEmbStrFunc(ptr_func, args);
}

LlValue * OTypeEmbStr::GenerateMethodCall(OScope * scope, LlValue * embstraddr,
                                    EEmbStrMethod method, const vector<OExpr *> & args)
{
  LlValue * dstdesc = GenerateDescriptor(scope, embstraddr);
  switch (method)
  {
    case ESM_CLEAR:
      return CallEmbStrFunc("EmbStrClear", {dstdesc});

    case ESM_SET:
      return GenerateEmbStrMethodSource(scope, args[0], "EmbStrAssignPtr", "EmbStrAssignDesc",
                                         "EmbStrAssignChar", dstdesc);

    case ESM_APPEND:
      return GenerateEmbStrMethodSource(scope, args[0], "EmbStrAppendPtr", "EmbStrAppendDesc",
                                         "EmbStrAppendChar", dstdesc);

    case ESM_PREPEND:
      return GenerateEmbStrMethodSource(scope, args[0], "EmbStrPrependPtr", "EmbStrPrependDesc",
                                         "EmbStrPrependChar", dstdesc);

    case ESM_INSERT:
    {
      LlValue * index = ToNativeInt(args[0]->Generate(scope));
      return GenerateEmbStrMethodSource(scope, args[1], "EmbStrInsertPtr", "EmbStrInsertDesc",
                                         "EmbStrInsertChar", dstdesc, index);
    }

    case ESM_DELETE:
    {
      LlValue * index = ToNativeInt(args[0]->Generate(scope));
      LlValue * count = (args.size() > 1 ? ToNativeInt(args[1]->Generate(scope)) : LlNativeInt(1));
      return CallEmbStrFunc("EmbStrDelete", {dstdesc, index, count});
    }

    case ESM_ADDFMT:
    {
      LlValue * arg0_val = g_builtins->type_rostr->GenerateBorrow(scope, args[0]);
      LlValue * arg1_val = args[1]->Generate(scope);
      return CallTextFormatFunc(scope, "EmbStrAddFmt", {dstdesc, arg0_val, arg1_val});
    }
  }
  return nullptr;
}

// OValueEmbStr

LlConst * OValueEmbStr::CreateLlConst()
{
  if (maxlen == 0)
  {
    auto * str_init = llvm::ConstantDataArray::getString(ll_ctx, value);
    auto * str_gv = new llvm::GlobalVariable(
        *ll_module,
        str_init->getType(),
        true,
        llvm::GlobalValue::PrivateLinkage,
        str_init,
        ".embstr.const");
    str_gv->setAlignment(llvm::Align(1));

    uint32_t charlen = uint32_t(value.size());
    uint32_t info = (charlen & DQTI_MAXCHLEN_MASK) | DQTIF_READONLY;
    vector<llvm::Constant *> fields = {
      llvm::ConstantExpr::getBitCast(str_gv, LlPtrType()),
      llvm::ConstantInt::get(LlEmbStrLenType(), charlen),
      llvm::ConstantInt::get(LlEmbStrLenType(), info)
    };
    auto * desc_gv = new llvm::GlobalVariable(
        *ll_module,
        LlEmbStrDescType(),
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantStruct::get(static_cast<llvm::StructType *>(LlEmbStrDescType()), fields),
        ".embstr.desc.const");
    desc_gv->setAlignment(llvm::Align(TARGET_PTRSIZE));
    return desc_gv;
  }

  // Create [maxlen x i8] constant, reserving the final byte for the terminator.
  vector<llvm::Constant *> chars;
  chars.reserve(maxlen);

  LlType * i8type = LlType::getInt8Ty(ll_ctx);

  for (uint32_t i = 0; i < maxlen; ++i)
  {
    if ((i < maxlen - 1) && (i < value.size()))
    {
      chars.push_back(llvm::ConstantInt::get(i8type, (uint8_t)value[i]));
    }
    else
    {
      chars.push_back(llvm::ConstantInt::get(i8type, 0));
    }
  }

  llvm::ArrayType * arrtype = llvm::ArrayType::get(i8type, maxlen);
  return llvm::ConstantArray::get(arrtype, chars);
}

bool OValueEmbStr::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecStr(DQMIF_VALUE_INLINE, value);
}

bool OValueEmbStr::CalculateConstant(OExpr * expr, bool emit_errors)
{
  auto * strlit = dynamic_cast<OEmbStrLit *>(expr);
  if (strlit)
  {
    value = strlit->value;
    return true;
  }

  if (emit_errors)
  {
    g_compiler->Error(DQERR_EMBSTR_CONSTEXPR);
  }
  return false;
}


bool OTypeEmbStr::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ROSTR == tks && maxlen > 0 && !is_explicit_cast
      && (aflags & EXPCF_ALLOW_LAZY_EMBSTR)) return true;

  if (TK_EMBSTR != tks)
  {
    if (TK_POINTER == tks)
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      if (this->maxlen != 0)
      {
        if ((aflags & EXPCF_ALLOW_LAZY_EMBSTR) && this->CanStoreFrom(src)) return true;
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
        return false;
      }
      if ((aflags & EXPCF_ALLOW_LAZY_EMBSTR) && IsCCharPointerType(resolved_src))
      {
        auto * strlit = dynamic_cast<OEmbStrLit *>(src);
        uint32_t known_len = (strlit ? uint32_t(strlit->value.size() + 1) : 0);
        *rexpr = new OEmbStrLitToDescExpr(src, known_len, this);
        return true;
      }
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
      return false;
    }
    return OType::ConvertFromExpr(rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }

  OTypeEmbStr * embstrsrc = static_cast<OTypeEmbStr *>(resolved_src);
  if ((this->maxlen == 0) and (embstrsrc->maxlen > 0))
  {
    OLValueExpr * lval = dynamic_cast<OLValueExpr *>(src);
    if (!lval)
    {
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->ErrorTxt(DQERR_EMBSTR_CONVERSION, "cannot convert non-lvalue embstr to descriptor");
      return false;
    }
    *rexpr = new OEmbStrLValueToDescExpr(lval, this);
    return true;
  }

  if ((aflags & EXPCF_ALLOW_LAZY_EMBSTR) && this->CanStoreFrom(src)) return true;

  if (this->maxlen != embstrsrc->maxlen)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->ErrorTxt(DQERR_EMBSTR_CONVERSION, "embstr sizes do not match");
    return false;
  }

  return true;
}

int OTypeEmbStr::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ROSTR == tks)
    return (maxlen > 0 && !is_explicit_cast && (aflags & EXPCF_ALLOW_LAZY_EMBSTR)) ? 1 : -1;

  if (TK_EMBSTR != tks)
  {
    if (TK_POINTER == tks)
    {
      if (is_explicit_cast) return -1;
      if (this->maxlen != 0) return ((aflags & EXPCF_ALLOW_LAZY_EMBSTR) && this->CanStoreFrom(expr)) ? 0 : -1;
      return ((aflags & EXPCF_ALLOW_LAZY_EMBSTR) && IsCCharPointerType(resolved_src)) ? 1 : -1;
    }
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  if (is_explicit_cast) return -1;

  OTypeEmbStr * embstrsrc = static_cast<OTypeEmbStr *>(resolved_src);
  if ((this->maxlen == 0) && (embstrsrc->maxlen > 0)) return (dynamic_cast<OLValueExpr *>(expr) ? 1 : -1);
  if ((aflags & EXPCF_ALLOW_LAZY_EMBSTR) && this->CanStoreFrom(expr)) return 0;
  return (this->maxlen == embstrsrc->maxlen) ? 0 : -1;
}
