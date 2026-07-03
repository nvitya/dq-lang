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
#include "dqc_ast.h"
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

static bool EnsureDynStringRtlUse()
{
  if (g_namespaces.end() != g_namespaces.find("__dq_dynstr"))
  {
    return true;
  }
  return g_compiler && g_compiler->AddImplicitUse("rtl/dynstrmgr", "__dq_dynstr", nullptr, true, MUM_NONE);
}

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
  LlType * ll_i64 = LlType::getInt64Ty(ll_ctx);
  if (index->getType() != ll_i64)
  {
    if (!index->getType()->isIntegerTy())
    {
      throw logic_error("Text index must be an integer value");
    }
    unsigned srcbits = index->getType()->getIntegerBitWidth();
    if (srcbits < 64)
    {
      index = ll_builder.CreateSExt(index, ll_i64);
    }
    else if (srcbits > 64)
    {
      index = ll_builder.CreateTrunc(index, ll_i64);
    }
  }
  LlValue * zero = llvm::ConstantInt::get(ll_i64, 0);
  LlValue * is_neg = ll_builder.CreateICmpSLT(index, zero, "str.idx.neg");
  return ll_builder.CreateSelect(is_neg, ll_builder.CreateAdd(len, index, "str.idx.from_end"), index, "str.idx.norm");
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

static OValSymFunc * TextFormatFunc(const string & name)
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

static LlValue * CallTextFormatFunc(const string & name, vector<LlValue *> args = {})
{
  OValSymFunc * fn = TextFormatFunc(name);
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
  LlValue * tmp = CreateEntryBlockAlloca(LlType::getInt8Ty(ll_ctx), nullptr, "str.char.tmp");
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
    straddr = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.tmp.slot");
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

LlValue * GenerateStringRefCount(OScope * scope, OType * strtype, LlValue * straddr)
{
  (void)scope;
  if (TK_DYNSTR == strtype->kind)
  {
    return CallDynStrFunc("DynStrGetRefCount", {straddr});
  }
  throw logic_error("GenerateStringRefCount requires str");
}

LlValue * GenerateStringGetChar(OScope * scope, OLValueExpr * receiver, LlValue * index)
{
  OType * rtype = receiver && receiver->ResolvedType() ? receiver->ResolvedType() : nullptr;
  if (rtype && TK_DYNSTR == rtype->kind)
  {
    LlValue * result = CallDynStrFunc("DynStrGetChar", {receiver->GenerateAddress(scope), ToNativeInt(index)});
    EmitExpressionExceptionCheck(scope);
    return result;
  }
  if (rtype && TK_STRVIEW == rtype->kind)
  {
    LlValue * result = CallDynStrFunc("TextInfoGetChar", {receiver->GenerateAddress(scope), ToNativeInt(index)});
    EmitExpressionExceptionCheck(scope);
    return result;
  }
  throw logic_error("GenerateStringGetChar requires str or strview");
}

LlValue * GenerateStringCharAddress(OScope * scope, OLValueExpr * receiver, LlValue * index)
{
  OType * rtype = receiver && receiver->ResolvedType() ? receiver->ResolvedType() : nullptr;
  if (!rtype || (TK_DYNSTR != rtype->kind && TK_STRVIEW != rtype->kind))
  {
    throw logic_error("GenerateStringCharAddress requires str or strview");
  }

  LlValue * descaddr = GenerateTextInfoAddress(scope, receiver);
  LlValue * len = ToNativeInt(CallDynStrFunc("TextInfoGetLength", {descaddr}));
  LlValue * norm_index = NormalizeTextIndexValue(index, len);

  LlType * desctype = g_builtins->type_strview->GetLlType();
  LlValue * ptraddr = ll_builder.CreateStructGEP(desctype, descaddr, 0, "str.ptr.addr");
  LlValue * dataptr = ll_builder.CreateLoad(LlPtrType(), ptraddr, "str.ptr");
  return ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), dataptr, {norm_index}, "str.elem");
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
  auto checked_dynstr_call = [scope](const string & name, vector<LlValue *> args) -> LlValue *
  {
    LlValue * result = CallDynStrFunc(name, args);
    EmitExpressionExceptionCheck(scope);
    return result;
  };
  auto checked_textformat_call = [scope](const string & name, vector<LlValue *> args) -> LlValue *
  {
    LlValue * result = CallTextFormatFunc(name, args);
    EmitExpressionExceptionCheck(scope);
    return result;
  };
  switch (method)
  {
    case STRM_CLEAR:
      checked_dynstr_call("DynStrClear", {straddr, args.empty() ? LlBool(false) : args[0]->Generate(scope)});
      return nullptr;
    case STRM_SET:
      if (!GenerateStringAssignExpr(scope, straddr, args[0]))
      {
        throw logic_error("Unsupported string Set() source");
      }
      return nullptr;
    case STRM_RESERVE:
      checked_dynstr_call("DynStrReserve", {straddr, llvm::ConstantInt::get(LlType::getInt8Ty(ll_ctx), 1), ToU32(args[0]->Generate(scope))});
      return nullptr;
    case STRM_COMPACT:
      checked_dynstr_call("DynStrCompact", {straddr});
      return nullptr;
    case STRM_SET_LENGTH:
      checked_dynstr_call("DynStrSetLengthFill", {straddr, ToU32(args[0]->Generate(scope)), args[1]->Generate(scope)});
      return nullptr;
    case STRM_SET_CAPACITY:
      checked_dynstr_call("DynStrSetCapacity", {straddr, llvm::ConstantInt::get(LlType::getInt8Ty(ll_ctx), 1), ToU32(args[0]->Generate(scope))});
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
      GenerateStringCreate(scope, tmp);
      checked_dynstr_call("DynStrClone", {straddr, tmp});
      return ll_builder.CreateLoad(g_builtins->type_str->GetLlType(), tmp, "str.clone");
    }
    case STRM_POP:
    case STRM_POP_FIRST:
    {
      LlValue * tmp = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "str.pop.tmp");
      GenerateStringCreate(scope, tmp);
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
      LlValue * arg0_val = GenerateTextInfoValue(scope, args[0]);
      LlValue * arg1_val = args[1]->Generate(scope);
      checked_textformat_call("DynStrAddFmt", {straddr, arg0_val, arg1_val});
      return nullptr;
    }
  }
  throw logic_error("Unhandled string method");
}


bool OTypeStrView::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
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
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      *rexpr = new OTextSourceToViewExpr(src, this);
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
    if (IsTextSourceType(resolved_src)) return is_explicit_cast ? -1 : 1;
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
    if (IsTextSourceType(resolved_src))
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
    if (IsTextSourceType(resolved_src)) return is_explicit_cast ? -1 : 1;
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  return is_explicit_cast ? -1 : 0;
}
