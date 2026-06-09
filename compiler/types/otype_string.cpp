/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_string.cpp
 * authors: nvitya
 * created: 2026-06-09
 * brief:   Byte-only str and strview type implementation
 */

#include <vector>
#include "otype_string.h"
#include "otype_cstring.h"
#include "scope_builtins.h"
#include "expressions.h"
#include "dqc.h"
#include "named_scopes.h"
#include "otype_func.h"

using namespace std;

static constexpr uint32_t DQTI_MAXCHLEN_MASK = 0x00FFFFFF;
static constexpr uint32_t DQTIF_CHARLEN_VALID = 0x01000000;
static constexpr uint32_t DQTIF_READONLY = 0x02000000;

static LlType * LlPtrType()
{
  return llvm::PointerType::get(ll_ctx, 0);
}

static LlType * LlU32Type()
{
  return LlType::getInt32Ty(ll_ctx);
}

static LlValue * LlU32(uint32_t value)
{
  return llvm::ConstantInt::get(LlU32Type(), value);
}

static LlValue * LlI32(int32_t value)
{
  return llvm::ConstantInt::get(LlType::getInt32Ty(ll_ctx), value);
}

static LlValue * LlNativeInt(int64_t value)
{
  return llvm::ConstantInt::get(g_builtins->type_int->GetLlType(), value);
}

static LlValue * LlBool(bool value)
{
  return llvm::ConstantInt::get(g_builtins->type_bool->GetLlType(), value);
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
    return ll_builder.CreateSExt(value, dst, "str.i.ext");
  }
  if (srcbits > dstbits)
  {
    return ll_builder.CreateTrunc(value, dst, "str.i.trunc");
  }
  return value;
}

static LlValue * ToU32(LlValue * value)
{
  LlType * dst = LlU32Type();
  if (value->getType() == dst)
  {
    return value;
  }
  if (!value->getType()->isIntegerTy())
  {
    return value;
  }
  unsigned srcbits = value->getType()->getIntegerBitWidth();
  if (srcbits < 32)
  {
    return ll_builder.CreateZExt(value, dst, "str.u32.ext");
  }
  if (srcbits > 32)
  {
    return ll_builder.CreateTrunc(value, dst, "str.u32.trunc");
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
    return ll_builder.CreateZExt(value, dst, "str.ch.ext");
  }
  if (srcbits > dstbits)
  {
    return ll_builder.CreateTrunc(value, dst, "str.ch.trunc");
  }
  return value;
}

static OValSymFunc * DynStrFunc(const string & name)
{
  auto nsit = g_namespaces.find("__dq_dynstr");
  if (nsit == g_namespaces.end() || !nsit->second)
  {
    throw runtime_error("Dynamic string RTL module is not loaded");
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
    throw runtime_error("Dynamic string RTL function is not available: " + name);
  }
  return fn;
}

static LlValue * CallDynStrFunc(const string & name, vector<LlValue *> args = {})
{
  OValSymFunc * fn = DynStrFunc(name);
  return ll_builder.CreateCall(fn->ll_func, args);
}

static bool IsCCharPointerType(OType * type)
{
  auto * ptrtype = dynamic_cast<OTypePointer *>(type ? type->ResolveAlias() : nullptr);
  return ptrtype
      && ptrtype->IsTypedPointer()
      && ptrtype->basetype
      && (ptrtype->basetype->ResolveAlias() == g_builtins->type_cchar);
}

static LlValue * TextInfoAlloca()
{
  return ll_builder.CreateAlloca(g_builtins->type_strview->GetLlType(), nullptr, "text.desc");
}

static LlValue * TextInfoValue(LlValue * ptr, uint32_t charlen, uint32_t info)
{
  LlValue * desc = llvm::UndefValue::get(g_builtins->type_strview->GetLlType());
  desc = ll_builder.CreateInsertValue(desc, ptr, 0, "text.desc.ptr");
  desc = ll_builder.CreateInsertValue(desc, LlU32(charlen), 1, "text.desc.len");
  desc = ll_builder.CreateInsertValue(desc, LlU32(info), 2, "text.desc.info");
  return desc;
}

static LlValue * StoreTextInfoValue(LlValue * desc)
{
  LlValue * descaddr = TextInfoAlloca();
  ll_builder.CreateStore(desc, descaddr);
  return descaddr;
}

static LlValue * GeneratePointerTextInfo(OScope * scope, OExpr * expr)
{
  (void)scope;
  LlValue * ptr = expr->Generate(scope);
  uint32_t charlen = 0;
  uint32_t info = DQTI_MAXCHLEN_MASK;
  if (auto * lit = dynamic_cast<OCStringLit *>(expr))
  {
    charlen = uint32_t(lit->value.size());
    info = (charlen & DQTI_MAXCHLEN_MASK) | DQTIF_CHARLEN_VALID | DQTIF_READONLY;
  }
  return TextInfoValue(ptr, charlen, info);
}

static LlValue * GenerateCharTextInfo(OScope * scope, OExpr * expr)
{
  LlValue * tmp = ll_builder.CreateAlloca(LlType::getInt8Ty(ll_ctx), nullptr, "str.char.tmp");
  LlValue * ch = ToCharValue(expr->Generate(scope));
  LlValue * bch = CallDynStrFunc("DynStrCharToByte", {ch});
  ll_builder.CreateStore(bch, tmp);
  return TextInfoValue(tmp, 1, DQTIF_CHARLEN_VALID | DQTIF_READONLY | 1);
}

static LlValue * GenerateDynStringFullView(OScope * scope, OExpr * expr)
{
  LlValue * straddr = nullptr;
  if (auto * lval = dynamic_cast<OLValueExpr *>(expr))
  {
    straddr = lval->GenerateAddress(scope);
  }
  else
  {
    straddr = ll_builder.CreateAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.tmp.slot");
    ll_builder.CreateStore(expr->Generate(scope), straddr);
  }
  LlValue * descaddr = TextInfoAlloca();
  CallDynStrFunc("DynStrGetFullView", {straddr, descaddr});
  return ll_builder.CreateLoad(g_builtins->type_strview->GetLlType(), descaddr, "str.full.view");
}

// OTypeDynString / OTypeStrView

LlType * OTypeDynString::CreateLlType()
{
  return LlPtrType();
}

LlDiType * OTypeDynString::CreateDiType()
{
  LlDiType * mgr_di = di_builder->createBasicType("ODynStrMgr", TARGET_PTRSIZE * 8, llvm::dwarf::DW_ATE_address);
  return di_builder->createPointerType(mgr_di, TARGET_PTRSIZE * 8);
}

LlType * OTypeStrView::CreateLlType()
{
  vector<LlType *> fields = {
    LlPtrType(),
    LlU32Type(),
    LlU32Type()
  };
  return llvm::StructType::get(ll_ctx, fields);
}

LlDiType * OTypeStrView::CreateDiType()
{
  LlDiType * ptr_di = di_builder->createPointerType(
      di_builder->createBasicType("char", 8, llvm::dwarf::DW_ATE_unsigned_char),
      TARGET_PTRSIZE * 8);
  LlDiType * u32_di = di_builder->createBasicType("uint32", 32, llvm::dwarf::DW_ATE_unsigned);
  llvm::Metadata * elements[] = {
    di_builder->createMemberType(nullptr, "dataptr", nullptr, 0, TARGET_PTRSIZE * 8, 0,
        0, llvm::DINode::FlagZero, ptr_di),
    di_builder->createMemberType(nullptr, "charlen", nullptr, 0, 32, 0,
        TARGET_PTRSIZE * 8, llvm::DINode::FlagZero, u32_di),
    di_builder->createMemberType(nullptr, "info", nullptr, 0, 32, 0,
        TARGET_PTRSIZE * 8 + 32, llvm::DINode::FlagZero, u32_di)
  };
  return di_builder->createStructType(
      nullptr, name, nullptr, 0, bytesize * 8, 0,
      llvm::DINode::FlagZero, nullptr,
      di_builder->getOrCreateArray(elements));
}

bool IsTextSourceType(OType * type)
{
  OType * resolved = type ? type->ResolveAlias() : nullptr;
  return resolved
      && (TK_DYNSTR == resolved->kind
          || TK_STRVIEW == resolved->kind
          || TK_CSTRING == resolved->kind
          || resolved == g_builtins->type_char
          || resolved == g_builtins->type_cchar
      || IsCCharPointerType(resolved));
}

bool IsStringComparableTextType(OType * type)
{
  OType * resolved = type ? type->ResolveAlias() : nullptr;
  return resolved
      && (TK_DYNSTR == resolved->kind
          || TK_STRVIEW == resolved->kind
          || TK_CSTRING == resolved->kind
          || IsCCharPointerType(resolved));
}

bool IsStringFamilyTextType(OType * type)
{
  OType * resolved = type ? type->ResolveAlias() : nullptr;
  return resolved
      && (TK_DYNSTR == resolved->kind
          || TK_STRVIEW == resolved->kind
          || TK_CSTRING == resolved->kind);
}

LlValue * GenerateTextInfoValue(OScope * scope, OExpr * expr)
{
  OType * srctype = expr ? expr->ResolvedType() : nullptr;
  if (!srctype)
  {
    throw logic_error("GenerateTextInfoValue requires a typed expression");
  }

  if (TK_STRVIEW == srctype->kind)
  {
    return expr->Generate(scope);
  }

  if (TK_DYNSTR == srctype->kind)
  {
    return GenerateDynStringFullView(scope, expr);
  }

  if (TK_CSTRING == srctype->kind)
  {
    OTypeCString * cstrtype = static_cast<OTypeCString *>(srctype);
    LlValue * cstraddr = nullptr;
    if (auto * lval = dynamic_cast<OLValueExpr *>(expr))
    {
      cstraddr = lval->GenerateAddress(scope);
    }
    if (!cstraddr)
    {
      throw logic_error("cstring text source requires an lvalue");
    }
    LlValue * descaddr = cstrtype->GenerateDescriptor(scope, cstraddr);
    return ll_builder.CreateLoad(g_builtins->type_strview->GetLlType(), descaddr, "cstr.text");
  }

  if (IsCCharPointerType(srctype))
  {
    return GeneratePointerTextInfo(scope, expr);
  }

  if (srctype == g_builtins->type_char || srctype == g_builtins->type_cchar)
  {
    return GenerateCharTextInfo(scope, expr);
  }

  throw logic_error("unsupported text source type: " + srctype->name);
}

LlValue * GenerateTextInfoAddress(OScope * scope, OExpr * expr)
{
  if (auto * lval = dynamic_cast<OLValueExpr *>(expr);
      lval && lval->ResolvedType() && TK_STRVIEW == lval->ResolvedType()->kind)
  {
    return lval->GenerateAddress(scope);
  }
  return StoreTextInfoValue(GenerateTextInfoValue(scope, expr));
}

LlValue * GenerateStringLength(OScope * scope, OType * strtype, LlValue * straddr)
{
  (void)scope;
  OType * resolved = strtype ? strtype->ResolveAlias() : nullptr;
  if (resolved && TK_DYNSTR == resolved->kind)
  {
    return ToNativeInt(CallDynStrFunc("DynStrGetLength", {straddr}));
  }
  if (resolved && TK_STRVIEW == resolved->kind)
  {
    return ToNativeInt(CallDynStrFunc("TextInfoGetLength", {straddr}));
  }
  throw logic_error("GenerateStringLength requires str or strview");
}

LlValue * GenerateStringCapacity(OScope * scope, OType * strtype, LlValue * straddr)
{
  (void)scope;
  OType * resolved = strtype ? strtype->ResolveAlias() : nullptr;
  if (resolved && TK_DYNSTR == resolved->kind)
  {
    return ToNativeInt(CallDynStrFunc("DynStrGetCapacity", {straddr}));
  }
  throw logic_error("GenerateStringCapacity requires str");
}

LlValue * GenerateStringGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index)
{
  OType * rtype = receiver && receiver->ResolvedType() ? receiver->ResolvedType() : nullptr;
  if (rtype && TK_DYNSTR == rtype->kind)
  {
    return CallDynStrFunc("DynStrGetChar", {receiver->GenerateAddress(scope), ToNativeInt(index)});
  }
  if (rtype && TK_STRVIEW == rtype->kind)
  {
    return CallDynStrFunc("TextInfoGetChar", {receiver->GenerateAddress(scope), ToNativeInt(index)});
  }
  throw logic_error("GenerateStringGetChar requires str or strview");
}

void GenerateStringSetChar(OScope * scope, OLValueExpr * receiver, OExpr * index, OExpr * value)
{
  CallDynStrFunc("DynStrSetChar", {receiver->GenerateAddress(scope), ToNativeInt(index->Generate(scope)), value->Generate(scope)});
}

LlValue * GenerateStringSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                              OExpr * end_expr, bool end_inclusive)
{
  OType * rtype = receiver && receiver->ResolvedType() ? receiver->ResolvedType() : nullptr;
  LlValue * descaddr = TextInfoAlloca();
  LlValue * zero = LlNativeInt(0);
  LlValue * start = start_expr ? ToNativeInt(start_expr->Generate(scope)) : zero;
  LlValue * end = nullptr;
  if (end_expr)
  {
    end = ToNativeInt(end_expr->Generate(scope));
    if (end_inclusive)
    {
      end = ll_builder.CreateAdd(end, LlNativeInt(1), "str.slice.end.incl");
    }
  }
  else
  {
    end = GenerateStringLength(scope, rtype, receiver->GenerateAddress(scope));
    end = ToNativeInt(end);
  }

  if (rtype && TK_DYNSTR == rtype->kind)
  {
    CallDynStrFunc("DynStrGetView", {receiver->GenerateAddress(scope), descaddr, start, end});
  }
  else if (rtype && TK_STRVIEW == rtype->kind)
  {
    CallDynStrFunc("TextInfoGetView", {receiver->GenerateAddress(scope), descaddr, start, end});
  }
  else
  {
    throw logic_error("GenerateStringSlice requires str or strview");
  }
  return ll_builder.CreateLoad(g_builtins->type_strview->GetLlType(), descaddr, "str.slice");
}

LlValue * GenerateStringEqual(OScope * scope, OExpr * left, OExpr * right)
{
  LlValue * ldesc = GenerateTextInfoAddress(scope, left);
  LlValue * rdesc = GenerateTextInfoAddress(scope, right);
  return CallDynStrFunc("TextInfoEqual", {ldesc, rdesc});
}

void GenerateStringCreate(OScope * scope, LlValue * straddr)
{
  (void)scope;
  ll_builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)), straddr);
}

void GenerateStringIncRef(OScope * scope, LlValue * straddr)
{
  (void)scope;
  CallDynStrFunc("DynStrIncRef", {straddr});
}

void GenerateStringDestroy(OScope * scope, LlValue * straddr)
{
  (void)scope;
  CallDynStrFunc("DynStrDecRef", {straddr});
}

bool GenerateStringAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value)
{
  if (!value || !value->ptype)
  {
    GenerateStringDestroy(scope, targetaddr);
    ll_builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)), targetaddr);
    return true;
  }

  OType * srctype = value->ResolvedType();
  if (!srctype)
  {
    return false;
  }

  if (TK_DYNSTR == srctype->kind)
  {
    LlValue * srcmgr = value->Generate(scope);
    if (dynamic_cast<OLValueExpr *>(value))
    {
      CallDynStrFunc("DynStrAssignOther", {targetaddr, srcmgr});
    }
    else
    {
      GenerateStringDestroy(scope, targetaddr);
      ll_builder.CreateStore(srcmgr, targetaddr);
    }
    return true;
  }

  if (IsTextSourceType(srctype))
  {
    CallDynStrFunc("DynStrAssignData", {targetaddr, GenerateTextInfoAddress(scope, value)});
    return true;
  }

  return false;
}

LlValue * GenerateStringMethodCall(OScope * scope, OLValueExpr * receiver, EStringMethod method,
                                   const vector<OExpr *> & args)
{
  LlValue * straddr = receiver->GenerateAddress(scope);
  switch (method)
  {
    case STRM_CLEAR:
      CallDynStrFunc("DynStrClear", {straddr, args.empty() ? LlBool(false) : args[0]->Generate(scope)});
      return nullptr;
    case STRM_RESERVE:
      CallDynStrFunc("DynStrReserve", {straddr, llvm::ConstantInt::get(LlType::getInt8Ty(ll_ctx), 1), ToU32(args[0]->Generate(scope))});
      return nullptr;
    case STRM_COMPACT:
      CallDynStrFunc("DynStrCompact", {straddr});
      return nullptr;
    case STRM_SET_LENGTH:
      CallDynStrFunc("DynStrSetLengthFill", {straddr, ToU32(args[0]->Generate(scope)), args[1]->Generate(scope)});
      return nullptr;
    case STRM_SET_CAPACITY:
      CallDynStrFunc("DynStrSetCapacity", {straddr, llvm::ConstantInt::get(LlType::getInt8Ty(ll_ctx), 1), ToU32(args[0]->Generate(scope))});
      return nullptr;
    case STRM_TRUNCATE:
      CallDynStrFunc("DynStrTruncate", {straddr, ToU32(args[0]->Generate(scope))});
      return nullptr;
    case STRM_APPEND:
      CallDynStrFunc("DynStrAppend", {straddr, GenerateTextInfoAddress(scope, args[0]), LlI32(-1)});
      return nullptr;
    case STRM_PREPEND:
      CallDynStrFunc("DynStrInsert", {straddr, LlNativeInt(0), GenerateTextInfoAddress(scope, args[0]), LlI32(-1)});
      return nullptr;
    case STRM_INSERT:
      CallDynStrFunc("DynStrInsert", {straddr, ToNativeInt(args[0]->Generate(scope)), GenerateTextInfoAddress(scope, args[1]), LlI32(-1)});
      return nullptr;
    case STRM_DELETE:
      CallDynStrFunc("DynStrDelete", {straddr, ToNativeInt(args[0]->Generate(scope)),
          args.size() > 1 ? ToNativeInt(args[1]->Generate(scope)) : LlNativeInt(1)});
      return nullptr;
    case STRM_CLONE:
    {
      LlValue * tmp = ll_builder.CreateAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.clone.tmp");
      GenerateStringCreate(scope, tmp);
      CallDynStrFunc("DynStrClone", {straddr, tmp});
      return ll_builder.CreateLoad(g_builtins->type_str->GetLlType(), tmp, "str.clone");
    }
    case STRM_POP:
    case STRM_POP_FIRST:
    {
      LlValue * tmp = ll_builder.CreateAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.pop.tmp");
      GenerateStringCreate(scope, tmp);
      CallDynStrFunc(STRM_POP == method ? "DynStrPop" : "DynStrPopFirst",
          {straddr, ToNativeInt(args[0]->Generate(scope)), tmp});
      return ll_builder.CreateLoad(g_builtins->type_str->GetLlType(), tmp, "str.pop");
    }
    case STRM_POP_CHAR:
      return CallDynStrFunc("DynStrPopChar", {straddr});
    case STRM_POP_FIRST_CHAR:
      return CallDynStrFunc("DynStrPopFirstChar", {straddr});
  }
  throw logic_error("Unhandled string method");
}
