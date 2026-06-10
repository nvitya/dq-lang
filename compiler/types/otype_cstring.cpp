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
#include "otype_string.h"
#include "scope_builtins.h"
#include "dqm_if.h"
#include "expressions.h"
#include "dqc.h"
#include "named_scopes.h"
#include "otype_func.h"

using namespace std;

static constexpr uint32_t DQTI_MAXCHLEN_MASK = 0x00FFFFFF;
static constexpr uint32_t DQTIF_CHARLEN_VALID = 0x01000000;

static LlType * LlPtrType()
{
  return llvm::PointerType::get(ll_ctx, 0);
}

static LlType * LlCStringLenType()
{
  return LlType::getInt32Ty(ll_ctx);
}

static bool IsCCharPointerType(OType * type)
{
  auto * ptrtype = dynamic_cast<OTypePointer *>(type ? type->ResolveAlias() : nullptr);
  return ptrtype
      && ptrtype->IsTypedPointer()
      && ptrtype->basetype
      && (ptrtype->basetype->ResolveAlias() == g_builtins->type_cchar);
}

static LlValue * LlU32(uint32_t value)
{
  return llvm::ConstantInt::get(LlCStringLenType(), value);
}

static LlValue * LlNativeInt(uint64_t value)
{
  return llvm::ConstantInt::get(g_builtins->type_int->GetLlType(), value);
}

static LlValue * ToNativeInt(LlValue * value)
{
  LlType * dst = g_builtins->type_int->GetLlType();
  if (value->getType() == dst)
  {
    return value;
  }
  if (!value->getType()->isIntegerTy())
  {
    return value;
  }
  unsigned srcbits = value->getType()->getIntegerBitWidth();
  unsigned dstbits = static_cast<llvm::IntegerType *>(dst)->getBitWidth();
  if (srcbits < dstbits)
  {
    return ll_builder.CreateZExt(value, dst, "cstr.int.ext");
  }
  if (srcbits > dstbits)
  {
    return ll_builder.CreateTrunc(value, dst, "cstr.int.trunc");
  }
  return value;
}

static LlValue * ToCharValue(LlValue * value)
{
  LlType * dst = g_builtins->type_char->GetLlType();
  if (value->getType() == dst)
  {
    return value;
  }
  if (!value->getType()->isIntegerTy())
  {
    return value;
  }
  unsigned srcbits = value->getType()->getIntegerBitWidth();
  unsigned dstbits = static_cast<llvm::IntegerType *>(dst)->getBitWidth();
  if (srcbits < dstbits)
  {
    return ll_builder.CreateZExt(value, dst, "cstr.ch.ext");
  }
  if (srcbits > dstbits)
  {
    return ll_builder.CreateTrunc(value, dst, "cstr.ch.trunc");
  }
  return value;
}

static OValSymFunc * CStringFunc(const string & name)
{
  auto nsit = g_namespaces.find("__dq_cstring");
  if (nsit == g_namespaces.end() || !nsit->second)
  {
    throw runtime_error("CString RTL module is not loaded");
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
    throw runtime_error("CString RTL function is not available: " + name);
  }
  return fn;
}

static LlValue * CallCStringFunc(const string & name, vector<LlValue *> args = {})
{
  OValSymFunc * fn = CStringFunc(name);
  return ll_builder.CreateCall(fn->ll_func, args);
}

// OTypeCString

LlType * OTypeCString::CreateLlType()
{
  if (maxlen > 0)
  {
    // Fixed-size storage: N usable bytes plus the hidden terminator slot.
    return llvm::ArrayType::get(LlType::getInt8Ty(ll_ctx), uint64_t(maxlen) + 1);
  }
  else
  {
    // Unsized descriptor: SDqTextInfo {ptr, u32 charlen, u32 info}
    vector<LlType *> fields = {
      LlPtrType(),
      LlCStringLenType(),
      LlCStringLenType()
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
      di_builder->getOrCreateSubrange(0, uint64_t(maxlen) + 1)
    };
    return di_builder->createArrayType(
        (uint64_t(maxlen) + 1) * 8, 0, elem_di,
        di_builder->getOrCreateArray(subscripts)
    );
  }
  else
  {
    // Unsized descriptor: struct {ptr, u32, u32}
    LlDiType * ptr_di = di_builder->createPointerType(
        di_builder->createBasicType("cchar", 8, llvm::dwarf::DW_ATE_signed_char),
        TARGET_PTRSIZE * 8);
    LlDiType * u32_di = di_builder->createBasicType("uint32", 32, llvm::dwarf::DW_ATE_unsigned);

    llvm::Metadata * elements[] = {
      di_builder->createMemberType(
          nullptr, "dataptr", nullptr, 0, TARGET_PTRSIZE * 8, 0,
          0, llvm::DINode::FlagZero, ptr_di),
      di_builder->createMemberType(
          nullptr, "charlen", nullptr, 0, 32, 0,
          TARGET_PTRSIZE * 8, llvm::DINode::FlagZero, u32_di),
      di_builder->createMemberType(
          nullptr, "info", nullptr, 0, 32, 0,
          TARGET_PTRSIZE * 8 + 32, llvm::DINode::FlagZero, u32_di)
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
  return ::IsCCharPointerType(type);
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

LlValue * GenerateCStringDataPtr(OScope * scope, OTypeCString * cstrtype, LlValue * cstraddr)
{
  (void)scope;
  if (cstrtype->maxlen > 0)
  {
    LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
    return ll_builder.CreateGEP(cstrtype->GetLlType(), cstraddr, {ll_zero, ll_zero}, "cstr.data");
  }

  LlValue * ll_ptr_addr = ll_builder.CreateStructGEP(cstrtype->GetLlType(), cstraddr, 0, "cstr.ptr.addr");
  return ll_builder.CreateLoad(LlPtrType(), ll_ptr_addr, "cstr.ptr");
}

LlValue * OTypeCString::GenerateDescriptor(OScope * scope, LlValue * cstraddr)
{
  if (maxlen == 0)
  {
    return cstraddr;
  }

  LlValue * descaddr = ll_builder.CreateAlloca(g_builtins->type_cstring->GetLlType(), nullptr, "cstr.desc.tmp");
  LlValue * dataptr = GenerateCStringDataPtr(scope, this, cstraddr);
  LlType * desctype = g_builtins->type_cstring->GetLlType();
  LlValue * ptraddr = ll_builder.CreateStructGEP(desctype, descaddr, 0, "cstr.desc.ptr.addr");
  LlValue * lenaddr = ll_builder.CreateStructGEP(desctype, descaddr, 1, "cstr.desc.len.addr");
  LlValue * infoaddr = ll_builder.CreateStructGEP(desctype, descaddr, 2, "cstr.desc.info.addr");
  ll_builder.CreateStore(dataptr, ptraddr);
  ll_builder.CreateStore(LlU32(0), lenaddr);
  ll_builder.CreateStore(LlU32(maxlen & DQTI_MAXCHLEN_MASK), infoaddr);
  return descaddr;
}

static LlValue * CStringSourceDescriptor(OScope * scope, OExpr * srcexpr)
{
  auto * srctype = dynamic_cast<OTypeCString *>(srcexpr->ResolvedType());
  if (!srctype)
  {
    return nullptr;
  }

  if (auto * srclval = dynamic_cast<OLValueExpr *>(srcexpr))
  {
    return srctype->GenerateDescriptor(scope, srclval->GenerateAddress(scope));
  }

  LlValue * tmp = ll_builder.CreateAlloca(srctype->GetLlType(), nullptr, "cstr.src.tmp");
  ll_builder.CreateStore(srcexpr->Generate(scope), tmp);
  return srctype->GenerateDescriptor(scope, tmp);
}

LlValue * GenerateCStringMetaField(OScope * scope, OTypeCString * cstrtype, LlValue * cstraddr, ECStringMetaField field)
{
  if (cstrtype->maxlen > 0)
  {
    if (CSMF_MAXLENGTH == field)
    {
      return LlNativeInt(cstrtype->maxlen);
    }
    if (CSMF_STORAGE_SIZE == field)
    {
      return LlNativeInt(uint64_t(cstrtype->maxlen) + 1);
    }
  }

  LlValue * descaddr = cstrtype->GenerateDescriptor(scope, cstraddr);
  switch (field)
  {
    case CSMF_LENGTH:
      return ToNativeInt(CallCStringFunc("CStrLen", {descaddr}));
    case CSMF_MAXLENGTH:
      return ToNativeInt(CallCStringFunc("CStrMaxLen", {descaddr}));
    case CSMF_STORAGE_SIZE:
      return ToNativeInt(CallCStringFunc("CStrStorageSize", {descaddr}));
  }
  return LlNativeInt(0);
}

static void CallCStringStore(OScope * scope, LlValue * dstdesc, OExpr * srcexpr)
{
  OType * srctype = srcexpr->ResolvedType();
  if (srctype && !IsCCharPointerType(srctype) && IsTextSourceType(srctype))
  {
    CallCStringFunc("CStrAssignDesc", {dstdesc, GenerateTextInfoAddress(scope, srcexpr)});
    return;
  }

  CallCStringFunc("CStrAssignPtr", {dstdesc, srcexpr->Generate(scope)});
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
    LlValue * dstdesc = GenerateDescriptor(scope, dstdaddr);
    CallCStringStore(scope, dstdesc, srcexpr);
    return true;
  }

  return false;
}

static bool IsCStringCharSource(OExpr * expr)
{
  OType * type = expr ? expr->ResolvedType() : nullptr;
  return type && (type == g_builtins->type_char || type == g_builtins->type_cchar);
}

static LlValue * GenerateCStringMethodSource(OScope * scope, OExpr * expr, const string & ptr_func,
                                             const string & desc_func, const string & char_func,
                                             LlValue * dstdesc, LlValue * index = nullptr)
{
  if (IsCStringCharSource(expr))
  {
    vector<LlValue *> args = {dstdesc};
    if (index)
    {
      args.push_back(index);
    }
    args.push_back(ToCharValue(expr->Generate(scope)));
    return CallCStringFunc(char_func, args);
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
    return CallCStringFunc(desc_func, args);
  }

  vector<LlValue *> args = {dstdesc};
  if (index)
  {
    args.push_back(index);
  }
  args.push_back(expr->Generate(scope));
  return CallCStringFunc(ptr_func, args);
}

LlValue * GenerateCStringMethodCall(OScope * scope, OTypeCString * cstrtype, LlValue * cstraddr,
                                    ECStringMethod method, const vector<OExpr *> & args)
{
  LlValue * dstdesc = cstrtype->GenerateDescriptor(scope, cstraddr);
  switch (method)
  {
    case CSM_CLEAR:
      return CallCStringFunc("CStrClear", {dstdesc});

    case CSM_SET:
      return GenerateCStringMethodSource(scope, args[0], "CStrAssignPtr", "CStrAssignDesc",
                                         "CStrAssignChar", dstdesc);

    case CSM_APPEND:
      return GenerateCStringMethodSource(scope, args[0], "CStrAppendPtr", "CStrAppendDesc",
                                         "CStrAppendChar", dstdesc);

    case CSM_PREPEND:
      return GenerateCStringMethodSource(scope, args[0], "CStrPrependPtr", "CStrPrependDesc",
                                         "CStrPrependChar", dstdesc);

    case CSM_INSERT:
    {
      LlValue * index = ToNativeInt(args[0]->Generate(scope));
      return GenerateCStringMethodSource(scope, args[1], "CStrInsertPtr", "CStrInsertDesc",
                                         "CStrInsertChar", dstdesc, index);
    }

    case CSM_DELETE:
    {
      LlValue * index = ToNativeInt(args[0]->Generate(scope));
      LlValue * count = (args.size() > 1 ? ToNativeInt(args[1]->Generate(scope)) : LlNativeInt(1));
      return CallCStringFunc("CStrDelete", {dstdesc, index, count});
    }
  }
  return nullptr;
}

// OValueCString

LlConst * OValueCString::CreateLlConst()
{
  if (maxlen == 0)
  {
    return nullptr;  // unsized type has no constant representation
  }

  // Create [maxlen + 1 x i8] constant, padded with zeros.
  vector<llvm::Constant *> chars;
  chars.reserve(uint64_t(maxlen) + 1);

  LlType * i8type = LlType::getInt8Ty(ll_ctx);

  for (uint32_t i = 0; i <= maxlen; ++i)
  {
    if ((i < maxlen) && (i < value.size()))
    {
      chars.push_back(llvm::ConstantInt::get(i8type, (uint8_t)value[i]));
    }
    else
    {
      chars.push_back(llvm::ConstantInt::get(i8type, 0));
    }
  }

  llvm::ArrayType * arrtype = llvm::ArrayType::get(i8type, uint64_t(maxlen) + 1);
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
