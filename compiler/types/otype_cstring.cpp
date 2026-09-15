/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_cstring.cpp
 * authors: nvitya
 * created: 2026-03-08
 * brief:   C-string type implementation
 */

#include <vector>
#include "dqc_ast.h"
#include "otype_cstring.h"
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

static LlType * LlCStringLenType()
{
  return LlType::getInt32Ty(ll_ctx);
}

static LlType * LlCStringDescType()
{
  return g_builtins->type_strslice->GetLlType();
}


static LlValue * LlU32(uint32_t value)
{
  return llvm::ConstantInt::get(LlCStringLenType(), value);
}

static LlValue * LlNativeInt(uint64_t value)
{
  return llvm::ConstantInt::get(g_builtins->type_int->GetLlType(), value);
}

static LlType * LlNativeIntType()
{
  return g_builtins->type_int->GetLlType();
}



static OValSymFunc * CStringFunc(const string & name)
{
  auto nsit = g_namespaces.find("__dq_strfunc");
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
    // Fixed-size storage includes the terminator slot.
    return llvm::ArrayType::get(LlType::getInt8Ty(ll_ctx), maxlen);
  }
  else
  {
    // Unsized aliases share a descriptor instead of copying its cached length.
    return LlPtrType();
  }
}

LlDiType * OTypeCString::CreateDiType()
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
  return dynamic_cast<OTypeCString *>(srctype) || IsCCharPointerType(srctype)
      || (srctype && TK_ROSTR == srctype->kind);
}

LlValue * OTypeCString::GenerateDataPtr(OScope * scope, LlValue * cstraddr)
{
  (void)scope;
  if (maxlen > 0)
  {
    LlValue * ll_zero = LlNativeInt(0);
    return ll_builder.CreateGEP(GetLlType(), cstraddr, {ll_zero, ll_zero}, "cstr.data");
  }

  LlValue * descaddr = GenerateDescriptor(scope, cstraddr);
  LlValue * ll_ptr_addr = ll_builder.CreateStructGEP(LlCStringDescType(), descaddr, 0, "cstr.ptr.addr");
  return ll_builder.CreateLoad(LlPtrType(), ll_ptr_addr, "cstr.ptr");
}

LlValue * OTypeCString::GenerateDescriptor(OScope * scope, LlValue * cstraddr)
{
  if (maxlen == 0)
  {
    return ll_builder.CreateLoad(GetLlType(), cstraddr, "cstr.desc");
  }

  auto cache_it = descriptor_caches.find(cstraddr);
  if (cache_it != descriptor_caches.end())
  {
    return cache_it->second;
  }

  LlValue * descaddr = CreateEntryBlockAlloca(LlCStringDescType(), nullptr, "cstr.desc.tmp");
  LlValue * dataptr = GenerateDataPtr(scope, cstraddr);
  LlType * desctype = LlCStringDescType();
  LlValue * ptraddr = ll_builder.CreateStructGEP(desctype, descaddr, 0, "cstr.desc.ptr.addr");
  LlValue * lenaddr = ll_builder.CreateStructGEP(desctype, descaddr, 1, "cstr.desc.len.addr");
  LlValue * infoaddr = ll_builder.CreateStructGEP(desctype, descaddr, 2, "cstr.desc.info.addr");
  ll_builder.CreateStore(dataptr, ptraddr);
  ll_builder.CreateStore(LlU32(DQTIF_CHARLEN_INVALID), lenaddr);
  ll_builder.CreateStore(LlU32((maxlen - 1) & DQTI_MAXCHLEN_MASK), infoaddr);
  descriptor_caches[cstraddr] = descaddr;
  return descaddr;
}

void OTypeCString::ResetDescriptorLength(OScope * scope, LlValue * cstraddr)
{
  LlValue * descaddr = GenerateDescriptor(scope, cstraddr);
  LlValue * lenaddr = ll_builder.CreateStructGEP(LlCStringDescType(), descaddr, 1, "cstr.len.addr");
  ll_builder.CreateStore(LlU32(DQTIF_CHARLEN_INVALID), lenaddr);
}

void OTypeCString::InvalidateDescriptor(OScope * scope, LlValue * cstraddr)
{
  LlValue * descaddr = GenerateDescriptor(scope, cstraddr);
  LlValue * infoaddr = ll_builder.CreateStructGEP(LlCStringDescType(), descaddr, 2, "cstr.info.addr");
  LlValue * info = ll_builder.CreateLoad(LlCStringLenType(), infoaddr, "cstr.info");
  ll_builder.CreateStore(ll_builder.CreateOr(info, LlU32(DQTIF_CHARLEN_EXTERNAL), "cstr.info.external"), infoaddr);
  ResetDescriptorLength(scope, cstraddr);
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

  LlValue * tmp = CreateEntryBlockAlloca(srctype->GetLlType(), nullptr, "cstr.src.tmp");
  ll_builder.CreateStore(srcexpr->Generate(scope), tmp);
  return srctype->GenerateDescriptor(scope, tmp);
}

LlValue * OTypeCString::GenerateMetaField(OScope * scope, LlValue * cstraddr, ECStringMetaField field)
{
  if (CSMF_PCHAR == field)
  {
    // A raw pointer can escape to unknown code, so its shared length cache can
    // no longer be trusted after this access.
    InvalidateDescriptor(scope, cstraddr);
    return GenerateDataPtr(scope, cstraddr);
  }

  if (maxlen > 0)
  {
    if (CSMF_MAXLENGTH == field)
    {
      return LlNativeInt(maxlen - 1);
    }
    if (CSMF_STORAGE_SIZE == field)
    {
      return LlNativeInt(maxlen);
    }
  }

  LlValue * descaddr = GenerateDescriptor(scope, cstraddr);
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
      auto * src_alloca = CreateEntryBlockAlloca(srctype->GetLlType(), nullptr, "cstr.src.tmp");
      src_alloca->setAlignment(llvm::Align(EffectiveStorageAlign(srctype)));
      srcaddr = src_alloca;
      ll_builder.CreateStore(srcexpr->Generate(scope), srcaddr);
    }

    LlValue * ll_zero = LlNativeInt(0);
    rsrcptr = ll_builder.CreateGEP(srctype->GetLlType(), srcaddr, {ll_zero, ll_zero}, "cstr.src.ptr");
    rsrclimit = LlNativeInt(srctype->maxlen - 1);
    return;
  }

  LlValue * descaddr = srcexpr->Generate(scope);
  LlValue * ptraddr = ll_builder.CreateStructGEP(LlCStringDescType(), descaddr, 0, "cstr.src.ptr.addr");
  LlValue * lenaddr = ll_builder.CreateStructGEP(LlCStringDescType(), descaddr, 1, "cstr.src.len.addr");
  rsrcptr = ll_builder.CreateLoad(LlPtrType(), ptraddr, "cstr.src.ptr");
  rsrclimit = ToNativeInt(ll_builder.CreateLoad(LlCStringLenType(), lenaddr, "cstr.src.size"));
}

static void EmitSizedCStringCopy(OScope * scope, LlValue * dstdaddr, OTypeCString * dsttype, OExpr * srcexpr)
{
  LlValue * ll_zero = LlNativeInt(0);
  LlValue * ll_one = LlNativeInt(1);
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

  LlValue * ll_copy_limit = LlNativeInt(dsttype->maxlen - 1);

  ll_builder.CreateBr(cond_bb);

  ll_builder.SetInsertPoint(cond_bb);
  llvm::PHINode * ll_i = ll_builder.CreatePHI(LlNativeIntType(), 2, "cstr.copy.i");
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
  llvm::PHINode * ll_term_index = ll_builder.CreatePHI(LlNativeIntType(), 2, "cstr.term.i");
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
    ResetDescriptorLength(scope, dstdaddr);
    return true;
  }

  if (auto * strlit = dynamic_cast<OCStringLit *>(srcexpr))
  {
    OValueCString val(this, maxlen);
    val.value = strlit->value;
    LlConst * ll_const = val.CreateLlConst();
    ll_builder.CreateStore(ll_const, dstdaddr);
    ResetDescriptorLength(scope, dstdaddr);
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

bool OTypeCString::GenerateAssignment(OScope * scope, LlValue * targetaddr, OExpr * value, bool volatile_store)
{
  if (maxlen > 0)
  {
    return GenerateStore(scope, targetaddr, value);
  }
  return OType::GenerateAssignment(scope, targetaddr, value, volatile_store);
}

static bool IsCStringCharSource(OExpr * expr)
{
  OType * type = expr ? expr->ResolvedType() : nullptr;
  return type && type == g_builtins->type_char;
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

LlValue * OTypeCString::GenerateMethodCall(OScope * scope, LlValue * cstraddr,
                                    ECStringMethod method, const vector<OExpr *> & args)
{
  LlValue * dstdesc = GenerateDescriptor(scope, cstraddr);
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

    case CSM_ADDFMT:
    {
      LlValue * arg0_val = g_builtins->type_rostr->GenerateBorrow(scope, args[0]);
      LlValue * arg1_val = args[1]->Generate(scope);
      return CallTextFormatFunc(scope, "CStrAddFmt", {dstdesc, arg0_val, arg1_val});
    }
  }
  return nullptr;
}

// OValueCString

LlConst * OValueCString::CreateLlConst()
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
        ".cstr.const");
    str_gv->setAlignment(llvm::Align(1));

    uint32_t charlen = uint32_t(value.size());
    uint32_t info = (charlen & DQTI_MAXCHLEN_MASK) | DQTIF_READONLY;
    vector<llvm::Constant *> fields = {
      llvm::ConstantExpr::getBitCast(str_gv, LlPtrType()),
      llvm::ConstantInt::get(LlCStringLenType(), charlen),
      llvm::ConstantInt::get(LlCStringLenType(), info)
    };
    auto * desc_gv = new llvm::GlobalVariable(
        *ll_module,
        LlCStringDescType(),
        true,
        llvm::GlobalValue::PrivateLinkage,
        llvm::ConstantStruct::get(static_cast<llvm::StructType *>(LlCStringDescType()), fields),
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


bool OTypeCString::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ROSTR == tks && maxlen > 0 && !is_explicit_cast
      && (aflags & EXPCF_ALLOW_LAZY_CSTRING)) return true;

  if (TK_CSTRING != tks)
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
        if ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && this->CanStoreFrom(src)) return true;
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
        return false;
      }
      if ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && IsCCharPointerType(resolved_src))
      {
        auto * strlit = dynamic_cast<OCStringLit *>(src);
        uint32_t known_len = (strlit ? uint32_t(strlit->value.size() + 1) : 0);
        *rexpr = new OCStringLitToDescExpr(src, known_len, this);
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

  OTypeCString * cstrsrc = static_cast<OTypeCString *>(resolved_src);
  if ((this->maxlen == 0) and (cstrsrc->maxlen > 0))
  {
    OLValueExpr * lval = dynamic_cast<OLValueExpr *>(src);
    if (!lval)
    {
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->ErrorTxt(DQERR_CSTR_CONVERSION, "cannot convert non-lvalue embstr to descriptor");
      return false;
    }
    *rexpr = new OCStringLValueToDescExpr(lval, this);
    return true;
  }

  if ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && this->CanStoreFrom(src)) return true;

  if (this->maxlen != cstrsrc->maxlen)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->ErrorTxt(DQERR_CSTR_CONVERSION, "embstr sizes do not match");
    return false;
  }

  return true;
}

int OTypeCString::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ROSTR == tks)
    return (maxlen > 0 && !is_explicit_cast && (aflags & EXPCF_ALLOW_LAZY_CSTRING)) ? 1 : -1;

  if (TK_CSTRING != tks)
  {
    if (TK_POINTER == tks)
    {
      if (is_explicit_cast) return -1;
      if (this->maxlen != 0) return ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && this->CanStoreFrom(expr)) ? 0 : -1;
      return ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && IsCCharPointerType(resolved_src)) ? 1 : -1;
    }
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  if (is_explicit_cast) return -1;

  OTypeCString * cstrsrc = static_cast<OTypeCString *>(resolved_src);
  if ((this->maxlen == 0) && (cstrsrc->maxlen > 0)) return (dynamic_cast<OLValueExpr *>(expr) ? 1 : -1);
  if ((aflags & EXPCF_ALLOW_LAZY_CSTRING) && this->CanStoreFrom(expr)) return 0;
  return (this->maxlen == cstrsrc->maxlen) ? 0 : -1;
}
