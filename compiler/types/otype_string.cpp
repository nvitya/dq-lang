/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_string.cpp
 * authors: nvitya
 * created: 2026-06-09
 * brief:   Byte-only str, rostr, and strview type implementation
 */

#include <vector>
#include "dqc_ast.h"
#include "otype_string.h"
#include "otype_cstring.h"
#include "rtlint.h"
#include "scope_builtins.h"
#include "expressions.h"
#include "dqc.h"
#include "named_scopes.h"
#include "otype_func.h"
#include "otype_char.h"

using namespace std;

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



static LlValue * NormalizeTextIndexValue(LlValue * index, LlValue * len)
{
  LlType * native_int = g_builtins->type_int->GetLlType();
  if (index->getType() != native_int)
  {
    if (!index->getType()->isIntegerTy())
    {
      throw logic_error("Text index must be an integer value");
    }
    unsigned srcbits = index->getType()->getIntegerBitWidth();
    unsigned dstbits = native_int->getIntegerBitWidth();
    if (srcbits < dstbits)
    {
      index = ll_builder.CreateSExt(index, native_int);
    }
    else if (srcbits > dstbits)
    {
      index = ll_builder.CreateTrunc(index, native_int);
    }
  }
  LlValue * zero = llvm::ConstantInt::get(native_int, 0);
  LlValue * is_neg = ll_builder.CreateICmpSLT(index, zero, "str.idx.neg");
  return ll_builder.CreateSelect(is_neg, ll_builder.CreateAdd(len, index, "str.idx.from_end"), index, "str.idx.norm");
}

static OValSymFunc * DynStrFunc(const string & name)
{
  auto nsit = g_namespaces.find("__dq_strfunc");
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

static LlValue * CallDynStrFunc(OScope * scope, const string & name, vector<LlValue *> args = {})
{
  OValSymFunc * fn = DynStrFunc(name);
  if (scope)
  {
    return scope->GenerateCallOrInvoke(static_cast<LlFuncType *>(fn->ptype->GetLlType()), fn->ll_func, args);
  }
  return ll_builder.CreateCall(fn->ll_func, args);
}

OValSymFunc * TextFormatFunc(const string & name)
{
  auto nsit = g_namespaces.find("__dq_textformat");
  if (nsit == g_namespaces.end() || !nsit->second)
  {
    throw runtime_error("Textformat RTL module is not loaded");
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
    throw runtime_error("Textformat RTL function is not available: " + name);
  }
  return fn;
}

LlValue * CallTextFormatFunc(OScope * scope, const string & name, vector<LlValue *> args)
{
  OValSymFunc * fn = TextFormatFunc(name);
  if (scope)
  {
    return scope->GenerateCallOrInvoke(static_cast<LlFuncType *>(fn->ptype->GetLlType()), fn->ll_func, args);
  }
  return ll_builder.CreateCall(fn->ll_func, args);
}



static LlValue * TextInfoAlloca()
{
  return CreateEntryBlockAlloca(g_builtins->type_strview->GetLlType(), nullptr, "text.desc");
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
  uint32_t charlen = DQTIF_CHARLEN_INVALID;
  uint32_t info = DQTI_MAXCHLEN_MASK;
  if (auto * lit = dynamic_cast<OCStringLit *>(expr))
  {
    charlen = uint32_t(lit->value.size());
    info = (charlen & DQTI_MAXCHLEN_MASK) | DQTIF_READONLY;
  }
  return TextInfoValue(ptr, charlen, info);
}

static LlValue * GenerateCharTextInfo(OScope * scope, OExpr * expr)
{
  LlValue * tmp = CreateEntryBlockAlloca(LlType::getInt8Ty(ll_ctx), nullptr, "str.char.tmp");
  LlValue * ch = ToCharValue(expr->Generate(scope));
  LlValue * bch = CallDynStrFunc(scope, "DynStrCharToByte", {ch});
  ll_builder.CreateStore(bch, tmp);
  return TextInfoValue(tmp, 1, DQTIF_READONLY | 1);
}

static LlValue * GenerateDynStringFullView(OScope * scope, OExpr * expr)
{
  LlValue * straddr = nullptr;
  if (auto * lval = dynamic_cast<OLValueExpr *>(expr); lval && !dynamic_cast<OPropertyExpr *>(expr))
  {
    straddr = lval->GenerateAddress(scope);
  }
  else
  {
    straddr = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.tmp.slot");
    ll_builder.CreateStore(expr->Generate(scope), straddr);
  }
  LlValue * descaddr = TextInfoAlloca();
  CallDynStrFunc(scope, "DynStrGetFullView", {straddr, descaddr});
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


static bool IsByteWCharLiteral(OExpr * expr)
{
  int64_t value = 0;
  return TryGetDirectWCharLiteralValue(expr, value) && value <= 255;
}

LlValue * GenerateTextInfoValue(OScope * scope, OExpr * expr)
{
  OType * srctype = expr ? expr->ResolvedType() : nullptr;
  if (!srctype)
  {
    throw logic_error("GenerateTextInfoValue requires a typed expression");
  }

  if (TK_ROSTR == srctype->kind)
  {
    return g_builtins->type_rostr->GenerateTextInfo(scope, expr);
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
    if (cstrtype->maxlen == 0)
    {
      return expr->Generate(scope);
    }

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

  if (srctype == g_builtins->type_char)
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

// OTypeString

LlValue * OTypeString::GenerateMetaField(OScope * scope, OLValueExpr * receiver, EStringMetaField field)
{
  LlValue * addr = receiver->GenerateAddress(scope);
  if (SMF_LENGTH == field) return GenerateLength(scope, addr);
  if (SMF_PCHAR == field)  return GeneratePChar(scope, addr);
  if (SMF_WCLEN == field)  return GenerateWcLen(scope, receiver);
  throw logic_error("OTypeString::GenerateMetaField: unsupported field");
}

LlValue * OTypeString::GenerateCharAddress(OScope * scope, OLValueExpr * receiver, LlValue * index)
{
  LlValue * descaddr = GenerateTextInfoAddress(scope, receiver);
  LlValue * len = ToNativeInt(CallDynStrFunc(scope, "TextInfoGetLength", {descaddr}));
  LlValue * norm_index = NormalizeTextIndexValue(index, len);

  LlType * desctype = g_builtins->type_strview->GetLlType();
  LlValue * ptraddr = ll_builder.CreateStructGEP(desctype, descaddr, 0, "str.ptr.addr");
  LlValue * dataptr = ll_builder.CreateLoad(LlPtrType(), ptraddr, "str.ptr");
  return ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), dataptr, {norm_index}, "str.elem");
}

LlValue * OTypeString::GenerateWcLen(OScope * scope, OLValueExpr * receiver)
{
  return CallDynStrFunc(scope, "StrfWcLen", {GenerateTextInfoValue(scope, receiver)});
}

LlValue * OTypeString::GenerateWCharAt(OScope * scope, OLValueExpr * receiver, OExpr * index)
{
  LlValue * result = CallDynStrFunc(scope, "StrfWCharAt", {
      GenerateTextInfoValue(scope, receiver),
      ToNativeInt(index->Generate(scope))
  });
  EmitExpressionExceptionCheck(scope);
  return result;
}

LlValue * OTypeString::GenerateWCharSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                                          OExpr * end_expr, bool end_inclusive)
{
  LlValue * zero = LlNativeInt(0);
  LlValue * start = start_expr ? ToNativeInt(start_expr->Generate(scope)) : zero;
  LlValue * end = end_expr ? ToNativeInt(end_expr->Generate(scope)) : GenerateWcLen(scope, receiver);
  if (end_inclusive)
  {
    end = ll_builder.CreateAdd(end, LlNativeInt(1), "str.wchar.slice.end.incl");
  }

  LlValue * result = CallDynStrFunc(scope, "StrfWCharSlice", {
      GenerateTextInfoValue(scope, receiver),
      start,
      end
  });
  EmitExpressionExceptionCheck(scope);
  return result;
}

LlValue * OTypeString::GenerateToWchars(OScope * scope, OLValueExpr * receiver)
{
  LlValue * result = CallDynStrFunc(scope, "StrfToWchars", {GenerateTextInfoValue(scope, receiver)});
  EmitExpressionExceptionCheck(scope);
  return result;
}

LlValue * OTypeString::GenerateEqual(OScope * scope, OExpr * left, OExpr * right)
{
  LlValue * ldesc = GenerateTextInfoAddress(scope, left);
  LlValue * rdesc = GenerateTextInfoAddress(scope, right);
  return CallDynStrFunc(scope, "TextInfoEqual", {ldesc, rdesc});
}

// OTypeDynString

LlValue * OTypeDynString::GenerateLength(OScope * scope, LlValue * straddr)
{
  (void)scope;
  return ToNativeInt(CallDynStrFunc(scope, "DynStrGetLength", {straddr}));
}

LlValue * OTypeDynString::GenerateCapacity(OScope * scope, LlValue * straddr)
{
  (void)scope;
  return ToNativeInt(CallDynStrFunc(scope, "DynStrGetCapacity", {straddr}));
}

LlValue * OTypeDynString::GenerateRefCount(OScope * scope, LlValue * straddr)
{
  (void)scope;
  return CallDynStrFunc(scope, "DynStrGetRefCount", {straddr});
}

LlValue * OTypeDynString::GeneratePChar(OScope * scope, LlValue * straddr)
{
  (void)scope;
  return CallDynStrFunc(scope, "DynStrPChar", {straddr});
}

LlValue * OTypeDynString::GenerateGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index)
{
  LlValue * result = CallDynStrFunc(scope, "DynStrGetChar", {receiver->GenerateAddress(scope), ToNativeInt(index)});
  EmitExpressionExceptionCheck(scope);
  return result;
}

void OTypeDynString::GenerateSetChar(OScope * scope, OLValueExpr * receiver, OExpr * index, OExpr * value)
{
  CallDynStrFunc(scope, "DynStrSetChar", {receiver->GenerateAddress(scope), ToNativeInt(index->Generate(scope)), value->Generate(scope)});
}

LlValue * OTypeDynString::GenerateSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                                        OExpr * end_expr, bool end_inclusive)
{
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
    end = GenerateLength(scope, receiver->GenerateAddress(scope));
    end = ToNativeInt(end);
  }

  CallDynStrFunc(scope, "DynStrGetView", {receiver->GenerateAddress(scope), descaddr, start, end});
  return ll_builder.CreateLoad(g_builtins->type_strview->GetLlType(), descaddr, "str.slice");
}

LlValue * OTypeDynString::GenerateMetaField(OScope * scope, OLValueExpr * receiver, EStringMetaField field)
{
  LlValue * addr = receiver->GenerateAddress(scope);
  if (SMF_CAPACITY == field) return GenerateCapacity(scope, addr);
  if (SMF_REFCOUNT == field) return GenerateRefCount(scope, addr);
  return super::GenerateMetaField(scope, receiver, field);
}

void OTypeDynString::GenerateCreate(OScope * scope, LlValue * straddr)
{
  (void)scope;
  ll_builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)), straddr);
}

void OTypeDynString::GenerateIncRef(OScope * scope, LlValue * straddr)
{
  (void)scope;
  CallDynStrFunc(scope, "DynStrIncRef", {straddr});
}

void OTypeDynString::GenerateDestroy(OScope * scope, LlValue * straddr)
{
  (void)scope;
  CallDynStrFunc(scope, "DynStrDecRef", {straddr});
}

bool OTypeDynString::GenerateAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value)
{
  if (!value || !value->ptype)
  {
    GenerateDestroy(scope, targetaddr);
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
      CallDynStrFunc(scope, "DynStrAssignOther", {targetaddr, srcmgr});
    }
    else
    {
      GenerateDestroy(scope, targetaddr);
      ll_builder.CreateStore(srcmgr, targetaddr);
    }
    return true;
  }

  if (srctype->IsTextSource())
  {
    CallDynStrFunc(scope, "DynStrAssignData", {targetaddr, GenerateTextInfoAddress(scope, value)});
    return true;
  }

  return false;
}

bool OTypeDynString::GenerateAssignment(OScope * scope, LlValue * targetaddr, OExpr * value, bool volatile_store)
{
  (void)volatile_store;
  return GenerateAssignExpr(scope, targetaddr, value);
}

LlValue * OTypeDynString::GenerateConcat(OScope * scope, OExpr * left, OExpr * right)
{
  LlValue * tmp = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.concat.tmp");
  g_builtins->type_str->GenerateCreate(scope, tmp);
  CallDynStrFunc(scope, "DynStrCreate", {tmp});
  CallDynStrFunc(scope, "DynStrAppend", {tmp, GenerateTextInfoAddress(scope, left), LlI32(-1)});
  EmitExpressionExceptionCheck(scope);
  CallDynStrFunc(scope, "DynStrAppend", {tmp, GenerateTextInfoAddress(scope, right), LlI32(-1)});
  EmitExpressionExceptionCheck(scope);
  return ll_builder.CreateLoad(g_builtins->type_str->GetLlType(), tmp, "str.concat");
}

LlValue * OTypeDynString::GenerateConcatFromStringValue(OScope * scope, LlValue * leftvalue, OExpr * right)
{
  LlValue * lefttmp = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.concat.left");
  ll_builder.CreateStore(leftvalue, lefttmp);
  LlValue * ldesc = TextInfoAlloca();
  CallDynStrFunc(scope, "DynStrGetFullView", {lefttmp, ldesc});

  LlValue * tmp = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.concat.tmp");
  g_builtins->type_str->GenerateCreate(scope, tmp);
  CallDynStrFunc(scope, "DynStrCreate", {tmp});
  CallDynStrFunc(scope, "DynStrAppend", {tmp, ldesc, LlI32(-1)});
  EmitExpressionExceptionCheck(scope);
  CallDynStrFunc(scope, "DynStrAppend", {tmp, GenerateTextInfoAddress(scope, right), LlI32(-1)});
  EmitExpressionExceptionCheck(scope);
  return ll_builder.CreateLoad(g_builtins->type_str->GetLlType(), tmp, "str.concat");
}

LlValue * OTypeDynString::GenerateMethodCall(OScope * scope, OLValueExpr * receiver, EStringMethod method,
                                             const vector<OExpr *> & args)
{
  LlValue * straddr = receiver->GenerateAddress(scope);
  auto checked_dynstr_call = [scope](const string & name, vector<LlValue *> args) -> LlValue *
  {
    LlValue * result = CallDynStrFunc(scope, name, args);
    EmitExpressionExceptionCheck(scope);
    return result;
  };
  auto checked_textformat_call = [scope](const string & name, vector<LlValue *> args) -> LlValue *
  {
    LlValue * result = CallTextFormatFunc(scope, name, args);
    EmitExpressionExceptionCheck(scope);
    return result;
  };
  switch (method)
  {
    case STRM_CLEAR:
      checked_dynstr_call("DynStrClear", {straddr, args.empty() ? LlBool(false) : args[0]->Generate(scope)});
      return nullptr;
    case STRM_SET:
      if (!GenerateAssignExpr(scope, straddr, args[0]))
      {
        throw logic_error("Unsupported string Set() source");
      }
      return nullptr;
    case STRM_RESERVE:
      checked_dynstr_call("DynStrReserve", {straddr, ToU32(args[0]->Generate(scope))});
      return nullptr;
    case STRM_COMPACT:
      checked_dynstr_call("DynStrCompact", {straddr});
      return nullptr;
    case STRM_SET_LENGTH:
      checked_dynstr_call("DynStrSetLengthFill", {straddr, ToU32(args[0]->Generate(scope)), args[1]->Generate(scope)});
      return nullptr;
    case STRM_SET_CAPACITY:
      checked_dynstr_call("DynStrSetCapacity", {straddr, ToU32(args[0]->Generate(scope))});
      return nullptr;
    case STRM_TRUNCATE:
      checked_dynstr_call("DynStrTruncate", {straddr, ToU32(args[0]->Generate(scope))});
      return nullptr;
    case STRM_APPEND:
      checked_dynstr_call("DynStrAppend", {straddr, GenerateTextInfoAddress(scope, args[0]), LlI32(-1)});
      return nullptr;
    case STRM_PREPEND:
      checked_dynstr_call("DynStrInsert", {straddr, LlNativeInt(0), GenerateTextInfoAddress(scope, args[0]), LlI32(-1)});
      return nullptr;
    case STRM_INSERT:
      checked_dynstr_call("DynStrInsert", {straddr, ToNativeInt(args[0]->Generate(scope)), GenerateTextInfoAddress(scope, args[1]), LlI32(-1)});
      return nullptr;
    case STRM_DELETE:
      checked_dynstr_call("DynStrDelete", {straddr, ToNativeInt(args[0]->Generate(scope)),
          args.size() > 1 ? ToNativeInt(args[1]->Generate(scope)) : LlNativeInt(1)});
      return nullptr;
    case STRM_CLONE:
    {
      LlValue * tmp = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.clone.tmp");
      GenerateCreate(scope, tmp);
      checked_dynstr_call("DynStrClone", {straddr, tmp});
      return ll_builder.CreateLoad(g_builtins->type_str->GetLlType(), tmp, "str.clone");
    }
    case STRM_POP:
    case STRM_POP_FIRST:
    {
      LlValue * tmp = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.pop.tmp");
      GenerateCreate(scope, tmp);
      checked_dynstr_call(STRM_POP == method ? "DynStrPop" : "DynStrPopFirst",
          {straddr, ToNativeInt(args[0]->Generate(scope)), tmp});
      return ll_builder.CreateLoad(g_builtins->type_str->GetLlType(), tmp, "str.pop");
    }
    case STRM_POP_CHAR:
      return checked_dynstr_call("DynStrPopChar", {straddr});
    case STRM_POP_FIRST_CHAR:
      return checked_dynstr_call("DynStrPopFirstChar", {straddr});
    case STRM_ADDFMT:
    {
      LlValue * arg0_val = g_builtins->type_rostr->GenerateBorrow(scope, args[0]);
      LlValue * arg1_val = args[1]->Generate(scope);
      checked_textformat_call("DynStrAddFmt", {straddr, arg0_val, arg1_val});
      return nullptr;
    }
    case STRM_TO_WCHARS:
      return GenerateToWchars(scope, receiver);
  }
  throw logic_error("Unhandled string method");
}

// OTypeStrView

LlValue * OTypeStrView::GenerateLength(OScope * scope, LlValue * straddr)
{
  (void)scope;
  return ToNativeInt(CallDynStrFunc(scope, "TextInfoGetLength", {straddr}));
}

LlValue * OTypeStrView::GeneratePChar(OScope * scope, LlValue * straddr)
{
  (void)scope;
  return CallDynStrFunc(scope, "TextInfoPChar", {straddr});
}

LlValue * OTypeStrView::GenerateGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index)
{
  LlValue * result = CallDynStrFunc(scope, "TextInfoGetChar", {receiver->GenerateAddress(scope), ToNativeInt(index)});
  EmitExpressionExceptionCheck(scope);
  return result;
}

LlValue * OTypeStrView::GenerateSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                                      OExpr * end_expr, bool end_inclusive)
{
  LlValue * sourceaddr = GenerateTextInfoAddress(scope, receiver);
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
    end = ToNativeInt(CallDynStrFunc(scope, "TextInfoGetLength", {sourceaddr}));
    end = ToNativeInt(end);
  }

  CallDynStrFunc(scope, "TextInfoGetView", {sourceaddr, descaddr, start, end});
  return ll_builder.CreateLoad(g_builtins->type_strview->GetLlType(), descaddr, "str.slice");
}

// Free function helpers

void GenerateStringCreate(OScope * scope, LlValue * straddr)
{
  g_builtins->type_str->GenerateCreate(scope, straddr);
}

void GenerateStringIncRef(OScope * scope, LlValue * straddr)
{
  g_builtins->type_str->GenerateIncRef(scope, straddr);
}

void GenerateStringDestroy(OScope * scope, LlValue * straddr)
{
  g_builtins->type_str->GenerateDestroy(scope, straddr);
}

bool GenerateStringAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value)
{
  return g_builtins->type_str->GenerateAssignExpr(scope, targetaddr, value);
}


bool OTypeStrView::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_STRVIEW != tks)
  {
    if (IsTextSourceType(resolved_src) || IsByteWCharLiteral(src))
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      if (!IsTextSourceType(resolved_src))
      {
        src = new OExprTypeConv(g_builtins->type_char, src);
      }
      *rexpr = new OTextBorrowExpr(src, this);
      return true;
    }
    return OType::ConvertFromExpr(rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }
  return true;
}

int OTypeStrView::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_STRVIEW != tks)
  {
    if (IsTextSourceType(resolved_src) || IsByteWCharLiteral(expr)) return is_explicit_cast ? -1 : 1;
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  return is_explicit_cast ? -1 : 0;
}

bool OTypeDynString::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_DYNSTR != tks)
  {
    if (IsTextSourceType(resolved_src) || IsByteWCharLiteral(src))
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      if (!EnsureDynStringRtlUse())
      {
        return false;
      }
      if (!IsTextSourceType(resolved_src))
      {
        src = new OExprTypeConv(g_builtins->type_char, src);
      }
      *rexpr = new OTextSourceToStringExpr(src, this);
      return true;
    }
    return OType::ConvertFromExpr(rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }
  return true;
}

int OTypeDynString::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_DYNSTR != tks)
  {
    if (IsTextSourceType(resolved_src) || IsByteWCharLiteral(expr)) return is_explicit_cast ? -1 : 1;
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  return is_explicit_cast ? -1 : 0;
}

LlValue * GenerateStringLength(OScope * scope, OType * strtype, LlValue * straddr)
{
  auto * st = dynamic_cast<OTypeString *>(strtype ? strtype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringLength requires str or strview");
  return st->GenerateLength(scope, straddr);
}

LlValue * GenerateStringPChar(OScope * scope, OType * strtype, LlValue * straddr)
{
  auto * st = dynamic_cast<OTypeString *>(strtype ? strtype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringPChar requires str or strview");
  return st->GeneratePChar(scope, straddr);
}

LlValue * GenerateStringCapacity(OScope * scope, OType * strtype, LlValue * straddr)
{
  auto * st = dynamic_cast<OTypeDynString *>(strtype ? strtype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringCapacity requires str");
  return st->GenerateCapacity(scope, straddr);
}

LlValue * GenerateStringRefCount(OScope * scope, OType * strtype, LlValue * straddr)
{
  auto * st = dynamic_cast<OTypeDynString *>(strtype ? strtype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringRefCount requires str");
  return st->GenerateRefCount(scope, straddr);
}

LlValue * GenerateStringEqual(OScope * scope, OExpr * left, OExpr * right)
{
  return OTypeString::GenerateEqual(scope, left, right);
}

LlValue * GenerateStringConcat(OScope * scope, OExpr * left, OExpr * right)
{
  return OTypeDynString::GenerateConcat(scope, left, right);
}

LlValue * GenerateStringConcatFromStringValue(OScope * scope, LlValue * leftvalue, OExpr * right)
{
  return OTypeDynString::GenerateConcatFromStringValue(scope, leftvalue, right);
}

LlValue * GenerateStringGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index)
{
  auto * st = dynamic_cast<OTypeString *>(receiver->ptype ? receiver->ptype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringGetChar requires str or strview");
  return st->GenerateGetChar(scope, receiver, index);
}

LlValue * GenerateStringCharAddress(OScope * scope, OLValueExpr * receiver, LlValue * index)
{
  auto * st = dynamic_cast<OTypeString *>(receiver->ptype ? receiver->ptype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringCharAddress requires str or strview");
  return st->GenerateCharAddress(scope, receiver, index);
}

void GenerateStringSetChar(OScope * scope, OLValueExpr * receiver, OExpr * index, OExpr * value)
{
  auto * st = dynamic_cast<OTypeDynString *>(receiver->ptype ? receiver->ptype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringSetChar requires str");
  st->GenerateSetChar(scope, receiver, index, value);
}

LlValue * GenerateStringSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                              OExpr * end_expr, bool end_inclusive)
{
  auto * st = dynamic_cast<OTypeString *>(receiver->ptype ? receiver->ptype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringSlice requires str or strview");
  return st->GenerateSlice(scope, receiver, start_expr, end_expr, end_inclusive);
}

LlValue * GenerateStringWcLen(OScope * scope, OLValueExpr * receiver)
{
  auto * st = dynamic_cast<OTypeString *>(receiver->ptype ? receiver->ptype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringWcLen requires str or strview");
  return st->GenerateWcLen(scope, receiver);
}

LlValue * GenerateStringWCharAt(OScope * scope, OLValueExpr * receiver, OExpr * index)
{
  auto * st = dynamic_cast<OTypeString *>(receiver->ptype ? receiver->ptype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringWCharAt requires str or strview");
  return st->GenerateWCharAt(scope, receiver, index);
}

LlValue * GenerateStringWCharSlice(OScope * scope, OLValueExpr * receiver, OExpr * start_expr,
                                   OExpr * end_expr, bool end_inclusive)
{
  auto * st = dynamic_cast<OTypeString *>(receiver->ptype ? receiver->ptype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringWCharSlice requires str or strview");
  return st->GenerateWCharSlice(scope, receiver, start_expr, end_expr, end_inclusive);
}

LlValue * GenerateStringToWchars(OScope * scope, OLValueExpr * receiver)
{
  auto * st = dynamic_cast<OTypeString *>(receiver->ptype ? receiver->ptype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringToWchars requires str or strview");
  return st->GenerateToWchars(scope, receiver);
}

LlValue * GenerateStringMethodCall(OScope * scope, OLValueExpr * receiver, EStringMethod method,
                                   const vector<OExpr *> & args)
{
  auto * st = dynamic_cast<OTypeDynString *>(receiver->ptype ? receiver->ptype->ResolveAlias() : nullptr);
  if (!st) throw logic_error("GenerateStringMethodCall requires str");
  return st->GenerateMethodCall(scope, receiver, method, args);
}

// rostr keeps the zero-termination contract separately from arbitrary text views.
LlType * OTypeRoStr::CreateLlType()
{
  return llvm::StructType::get(ll_ctx, {LlPtrType(), LlU32Type()});
}

LlDiType * OTypeRoStr::CreateDiType()
{
  LlDiType * ptr_di = di_builder->createPointerType(
      di_builder->createBasicType("char", 8, llvm::dwarf::DW_ATE_unsigned_char), TARGET_PTRSIZE * 8);
  LlDiType * len_di = di_builder->createBasicType("uint32", 32, llvm::dwarf::DW_ATE_unsigned);
  llvm::Metadata * fields[] = {
    di_builder->createMemberType(nullptr, "dataptr", nullptr, 0, TARGET_PTRSIZE * 8,
        TARGET_PTRSIZE * 8, 0, llvm::DINode::FlagZero, ptr_di),
    di_builder->createMemberType(nullptr, "charlen", nullptr, 0, 32, 32,
        TARGET_PTRSIZE * 8, llvm::DINode::FlagZero, len_di)
  };
  return di_builder->createStructType(nullptr, name, nullptr, 0, bytesize * 8,
      alignsize * 8, llvm::DINode::FlagZero, nullptr, di_builder->getOrCreateArray(fields));
}

int OTypeRoStr::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  if (aflags & EXPCF_EXPLICIT_CAST) return -1;
  OType * source = expr->ResolvedType();
  if (TK_ROSTR == source->kind) return 0;
  uint8_t ch;
  return (TK_DYNSTR == source->kind || TK_CSTRING == source->kind
          || IsCCharPointerType(source) || IsCharLiteralExpr(expr, ch)) ? 1 : -1;
}

bool OTypeRoStr::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  if (GetConversionCostFromExpr(*rexpr, aflags) < 0)
  {
    if (aflags & EXPCF_EXPLICIT_CAST)
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
        g_compiler->Error(DQERR_CAST_INVALID, (*rexpr)->ResolvedType()->name, name);
      return false;
    }
    return OType::ConvertFromExpr(rexpr, aflags);
  }
  if (TK_ROSTR == (*rexpr)->ResolvedType()->kind) return true;
  uint8_t ch;
  if (IsCharLiteralExpr(*rexpr, ch))
  {
    OExpr::DeleteTree(*rexpr);
    *rexpr = new OCStringLit(string(1, char(ch)));
  }
  *rexpr = new OTextBorrowExpr(*rexpr, this);
  return true;
}

LlValue * OTypeRoStr::ExtractPChar(LlValue * value)
{
  LlValue * ptr = ll_builder.CreateExtractValue(value, 0, "rostr.ptr");
  // Zero-initialized aggregate fields are valid empty strings too.
  auto * empty = ll_module->getGlobalVariable(".rostr.empty", true);
  if (!empty)
  {
    auto * init = llvm::ConstantDataArray::getString(ll_ctx, "");
    empty = new llvm::GlobalVariable(*ll_module, init->getType(), true,
        llvm::GlobalValue::PrivateLinkage, init, ".rostr.empty");
    empty->setAlignment(llvm::Align(1));
  }
  return ll_builder.CreateSelect(ll_builder.CreateIsNull(ptr), empty, ptr, "rostr.pchar");
}

LlValue * OTypeRoStr::GenerateBorrow(OScope * scope, OExpr * source)
{
  OType * srctype = source->ResolvedType();
  if (TK_ROSTR == srctype->kind) return source->Generate(scope);
  LlValue * ptr;
  LlValue * len;
  if (IsCCharPointerType(srctype))
  {
    ptr = source->Generate(scope);
    auto * literal = dynamic_cast<OCStringLit *>(source);
    len = LlU32(literal ? uint32_t(literal->value.size()) : DQTIF_CHARLEN_INVALID);
  }
  else
  {
    LlValue * info = GenerateTextInfoValue(scope, source);
    ptr = ll_builder.CreateExtractValue(info, 0);
    LlValue * charlen = ll_builder.CreateExtractValue(info, 1);
    LlValue * known = ll_builder.CreateICmpEQ(
        ll_builder.CreateAnd(charlen, LlU32(DQTIF_CHARLEN_INVALID)), LlU32(0));
    len = ll_builder.CreateSelect(known, charlen, LlU32(DQTIF_CHARLEN_INVALID));
  }
  LlValue * value = llvm::UndefValue::get(GetLlType());
  value = ll_builder.CreateInsertValue(value, ptr, 0);
  len = ll_builder.CreateSelect(ll_builder.CreateIsNull(ptr), LlU32(0), len);
  value = ll_builder.CreateInsertValue(value, len, 1);
  return ll_builder.CreateInsertValue(value, ExtractPChar(value), 0);
}

LlValue * OTypeRoStr::GenerateLength(OScope * scope, LlValue * straddr)
{
  LlValue * len = CallDynStrFunc(scope, "RoStrGetLength", {straddr});
  EmitExpressionExceptionCheck(scope);
  return ToNativeInt(len);
}

LlValue * OTypeRoStr::GeneratePChar(OScope * scope, LlValue * straddr)
{
  return ExtractPChar(ll_builder.CreateLoad(GetLlType(), straddr));
}

LlValue * OTypeRoStr::GenerateTextInfo(OScope * scope, OExpr * source)
{
  LlValue * addr;
  if (auto * lval = dynamic_cast<OLValueExpr *>(source)) addr = lval->GenerateAddress(scope);
  else
  {
    addr = CreateEntryBlockAlloca(GetLlType(), nullptr, "rostr.tmp");
    ll_builder.CreateStore(source->Generate(scope), addr);
  }
  LlValue * len = ToU32(GenerateLength(scope, addr));
  LlValue * info = TextInfoValue(GeneratePChar(scope, addr), 0, DQTIF_READONLY);
  return ll_builder.CreateInsertValue(info, len, 1);
}

LlValue * OTypeRoStr::GenerateGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index)
{
  LlValue * value = CallDynStrFunc(scope, "TextInfoGetChar",
      {GenerateTextInfoAddress(scope, receiver), ToNativeInt(index)});
  EmitExpressionExceptionCheck(scope);
  return value;
}

LlValue * OTypeRoStr::GenerateSlice(OScope * scope, OLValueExpr * receiver,
    OExpr * start_expr, OExpr * end_expr, bool end_inclusive)
{
  return g_builtins->type_strview->GenerateSlice(scope, receiver, start_expr, end_expr, end_inclusive);
}

OValueRoStr::OValueRoStr(OType * atype)
  : OValue(atype), literal(g_builtins->type_char->GetPointerType(), 0)
{
  literal.has_string_literal = true;
}

LlConst * OValueRoStr::CreateLlConst()
{
  const string * text = literal.GetStringLiteral();
  return llvm::ConstantStruct::get(static_cast<llvm::StructType *>(ptype->GetLlType()),
      {literal.GetLlConst(), llvm::ConstantInt::get(LlU32Type(), text ? text->size() : 0)});
}

bool OValueRoStr::CalculateConstant(OExpr * expr, bool emit_errors)
{
  if (auto * borrow = dynamic_cast<OTextBorrowExpr *>(expr)) expr = borrow->source;
  if (auto * ref = dynamic_cast<OLValueVar *>(expr))
  {
    auto * symbol = dynamic_cast<OValSymConst *>(ref->pvalsym);
    auto * value = symbol ? dynamic_cast<OValueRoStr *>(symbol->pvalue) : nullptr;
    if (value)
    {
      literal.string_literal_source = &value->literal;
      literal.has_string_literal = false;
      return true;
    }
  }
  uint8_t ch;
  if (IsCharLiteralExpr(expr, ch))
  {
    literal.has_string_literal = true;
    literal.string_literal = string(1, char(ch));
    return true;
  }
  if (!literal.CalculateConstant(expr, emit_errors)) return false;
  if (!literal.GetStringLiteral())
  {
    if (literal.address != 0)
    {
      if (emit_errors) g_compiler->Error(DQERR_CONSTEXPR_INVALID_FOR, ptype->name);
      return false;
    }
    literal.has_string_literal = true;
  }
  return true;
}

bool OValueRoStr::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return literal.WriteDqmIfValue(writer);
}
