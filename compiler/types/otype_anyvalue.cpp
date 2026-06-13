/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_anyvalue.cpp
 * authors: nvitya
 * created: 2026-06-10
 * brief:   anyvalue builtin type and lowering helpers
 */

#include <vector>
#include "dqc_ast.h"
#include "otype_anyvalue.h"
#include "otype_bool.h"
#include "otype_cstring.h"
#include "otype_float.h"
#include "otype_int.h"
#include "otype_compound.h"
#include "otype_string.h"
#include "scope_builtins.h"
#include "expressions.h"
#include "dqc.h"
#include "named_scopes.h"
#include "otype_func.h"

using namespace std;

static constexpr const char * ANYVALUE_NAMESPACE = "__dq_anyvalue";

static LlType * LlI8Type()
{
  return LlType::getInt8Ty(ll_ctx);
}



static LlValue * ToNativeUInt(LlValue * value)
{
  LlType * dst = g_builtins->type_uint->GetLlType();
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
    return ll_builder.CreateZExt(value, dst, "any.u.ext");
  }
  if (srcbits > dstbits)
  {
    return ll_builder.CreateTrunc(value, dst, "any.u.trunc");
  }
  return value;
}

static LlValue * ToFloat32(LlValue * value)
{
  LlType * dst = g_builtins->type_float32->GetLlType();
  if (value->getType() == dst)
  {
    return value;
  }
  return ll_builder.CreateFPTrunc(value, dst, "any.f32");
}

static LlValue * ToFloat64(LlValue * value)
{
  LlType * dst = g_builtins->type_float64->GetLlType();
  if (value->getType() == dst)
  {
    return value;
  }
  return ll_builder.CreateFPExt(value, dst, "any.f64");
}



bool EnsureAnyValueRtlUse()
{
  if (g_namespaces.end() != g_namespaces.find(ANYVALUE_NAMESPACE))
  {
    return true;
  }
  return g_compiler->AddImplicitUse("rtl/anyvalue", ANYVALUE_NAMESPACE, nullptr, true, MUM_NONE);
}

static OValSymFunc * AnyValueFunc(const string & name)
{
  auto nsit = g_namespaces.find(ANYVALUE_NAMESPACE);
  if (nsit == g_namespaces.end() || !nsit->second)
  {
    throw runtime_error("AnyValue RTL module is not loaded");
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
    throw runtime_error("AnyValue RTL function is not available: " + name);
  }
  return fn;
}

static LlValue * CallAnyValueFunc(const string & name, vector<LlValue *> args = {})
{
  OValSymFunc * fn = AnyValueFunc(name);
  return ll_builder.CreateCall(fn->ll_func, args);
}

LlType * OTypeAnyValue::CreateLlType()
{
  vector<LlType *> fields = {
    llvm::ArrayType::get(LlI8Type(), 16),
    LlI8Type(),
    LlI8Type(),
    LlI8Type(),
    llvm::ArrayType::get(LlI8Type(), TARGET_PTRSIZE - 1)
  };
  return llvm::StructType::get(ll_ctx, fields);
}

LlDiType * OTypeAnyValue::CreateDiType()
{
  return di_builder->createStructType(
      nullptr, name, nullptr, 0, bytesize * 8, 0,
      llvm::DINode::FlagZero, nullptr,
      di_builder->getOrCreateArray({})
  );
}

bool IsAnyValueSourceType(OType * type)
{
  OType * resolved = type ? type->ResolveAlias() : nullptr;
  if (!resolved)
  {
    return false;
  }
  switch (resolved->kind)
  {
    case TK_ANYVALUE:
    case TK_BOOL:
    case TK_INT:
    case TK_FLOAT:
    case TK_POINTER:
    case TK_CSTRING:
    case TK_STRVIEW:
    case TK_DYNSTR:
      return true;
    default:
      return false;
  }
}

void GenerateAnyValueCreate(OScope * scope, LlValue * addr)
{
  (void)scope;
  ll_builder.CreateStore(llvm::ConstantAggregateZero::get(g_builtins->type_anyvalue->GetLlType()), addr);
}

void GenerateAnyValueDestroy(OScope * scope, LlValue * addr)
{
  (void)scope;
  CallAnyValueFunc("AnyValDestroy", {addr});
}

void GenerateAnyValueCopy(OScope * scope, LlValue * dstaddr, LlValue * srcaddr)
{
  (void)scope;
  CallAnyValueFunc("AnyValCopy", {dstaddr, srcaddr});
}

void GenerateAnyValueMove(OScope * scope, LlValue * dstaddr, LlValue * srcaddr)
{
  (void)scope;
  CallAnyValueFunc("AnyValMove", {dstaddr, srcaddr});
}

static bool GenerateAnyValueTextAssign(OScope * scope, LlValue * targetaddr, OExpr * value, OType * srctype)
{
  if (TK_DYNSTR == srctype->kind)
  {
    LlValue * srcstr = value->Generate(scope);
    CallAnyValueFunc("AnyValSetStr", {targetaddr, srcstr});
    if (!dynamic_cast<OLValueExpr *>(value))
    {
      LlValue * tmp = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "any.str.tmp");
      ll_builder.CreateStore(srcstr, tmp);
      GenerateStringDestroy(scope, tmp);
    }
    return true;
  }

  LlValue * descaddr = GenerateTextInfoAddress(scope, value);
  if (TK_STRVIEW == srctype->kind)
  {
    CallAnyValueFunc("AnyValSetText", {targetaddr, descaddr});
  }
  else
  {
    CallAnyValueFunc("AnyValSetCString", {targetaddr, descaddr});
  }
  return true;
}

bool GenerateAnyValueAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value)
{
  if (!value || !value->ptype)
  {
    return false;
  }

  OType * srctype = value->ResolvedType();
  if (!srctype)
  {
    return false;
  }

  if (TK_ANYVALUE == srctype->kind)
  {
    if (auto * lval = dynamic_cast<OLValueExpr *>(value))
    {
      GenerateAnyValueCopy(scope, targetaddr, lval->GenerateAddress(scope));
    }
    else
    {
      LlValue * tmp = CreateEntryBlockAlloca(g_builtins->type_anyvalue->GetLlType(), nullptr, "any.move.tmp");
      ll_builder.CreateStore(value->Generate(scope), tmp);
      GenerateAnyValueMove(scope, targetaddr, tmp);
    }
    return true;
  }

  if (TK_BOOL == srctype->kind)
  {
    CallAnyValueFunc("AnyValSetBool", {targetaddr, value->Generate(scope)});
    return true;
  }

  if (TK_INT == srctype->kind)
  {
    auto * inttype = static_cast<OTypeInt *>(srctype);
    if (inttype->issigned)
    {
      CallAnyValueFunc("AnyValSetInt", {targetaddr, ToNativeInt(value->Generate(scope))});
    }
    else
    {
      CallAnyValueFunc("AnyValSetUInt", {targetaddr, ToNativeUInt(value->Generate(scope))});
    }
    return true;
  }

  if (TK_FLOAT == srctype->kind)
  {
    auto * floattype = static_cast<OTypeFloat *>(srctype);
    if (floattype->bitlength <= 32)
    {
      CallAnyValueFunc("AnyValSetFloat32", {targetaddr, ToFloat32(value->Generate(scope))});
    }
    else
    {
      CallAnyValueFunc("AnyValSetFloat64", {targetaddr, ToFloat64(value->Generate(scope))});
    }
    return true;
  }

  if (TK_CSTRING == srctype->kind || TK_STRVIEW == srctype->kind || TK_DYNSTR == srctype->kind || IsCCharPointerType(srctype))
  {
    return GenerateAnyValueTextAssign(scope, targetaddr, value, srctype);
  }

  if (TK_POINTER == srctype->kind)
  {
    CallAnyValueFunc("AnyValSetPointer", {targetaddr, value->Generate(scope)});
    return true;
  }

  return false;
}

LlValue * GenerateAnyValueBoxExpr(OScope * scope, OType * anytype, OExpr * source)
{
  LlValue * tmp = CreateEntryBlockAlloca(anytype->GetLlType(), nullptr, "any.box.tmp");
  GenerateAnyValueCreate(scope, tmp);
  if (!GenerateAnyValueAssignExpr(scope, tmp, source))
  {
    throw logic_error("Unsupported anyvalue source type");
  }
  return ll_builder.CreateLoad(anytype->GetLlType(), tmp, "any.box");
}

LlValue * GenerateAnyValueMethodCall(OScope * scope, OLValueExpr * receiver, EAnyValueMethod method,
                                     const vector<OExpr *> & args)
{
  LlValue * addr = receiver->GenerateAddress(scope);
  switch (method)
  {
    case AVM_IS_NULL:    return CallAnyValueFunc("AnyValIsNull", {addr});
    case AVM_SET_NULL:   CallAnyValueFunc("AnyValSetNull", {addr}); return nullptr;
    case AVM_IS_NUMBER:  return CallAnyValueFunc("AnyValIsNumber", {addr});
    case AVM_IS_INT:     return CallAnyValueFunc("AnyValIsInt", {addr});
    case AVM_IS_SINT:    return CallAnyValueFunc("AnyValIsSInt", {addr});
    case AVM_IS_UINT:    return CallAnyValueFunc("AnyValIsUint", {addr});
    case AVM_AS_INT:     return CallAnyValueFunc("AnyValAsInt", {addr, ToNativeInt(args[0]->Generate(scope))});
    case AVM_AS_UINT:    return CallAnyValueFunc("AnyValAsUint", {addr, ToNativeUInt(args[0]->Generate(scope))});
    case AVM_SET_INT:    CallAnyValueFunc("AnyValSetInt", {addr, ToNativeInt(args[0]->Generate(scope))}); return nullptr;
    case AVM_SET_UINT:   CallAnyValueFunc("AnyValSetUInt", {addr, ToNativeUInt(args[0]->Generate(scope))}); return nullptr;
    case AVM_IS_BOOL:    return CallAnyValueFunc("AnyValIsBool", {addr});
    case AVM_AS_BOOL:    return CallAnyValueFunc("AnyValAsBool", {addr, args[0]->Generate(scope)});
    case AVM_SET_BOOL:   CallAnyValueFunc("AnyValSetBool", {addr, args[0]->Generate(scope)}); return nullptr;
    case AVM_IS_POINTER: return CallAnyValueFunc("AnyValIsPointer", {addr});
    case AVM_AS_POINTER: return CallAnyValueFunc("AnyValAsPointer", {addr, args[0]->Generate(scope)});
    case AVM_SET_POINTER: CallAnyValueFunc("AnyValSetPointer", {addr, args[0]->Generate(scope)}); return nullptr;
    case AVM_IS_FLOAT:   return CallAnyValueFunc("AnyValIsFloat", {addr});
    case AVM_IS_FLOAT32: return CallAnyValueFunc("AnyValIsFloat32", {addr});
    case AVM_IS_FLOAT64: return CallAnyValueFunc("AnyValIsFloat64", {addr});
    case AVM_AS_FLOAT:   return CallAnyValueFunc("AnyValAsFloat", {addr, ToFloat64(args[0]->Generate(scope))});
    case AVM_AS_FLOAT32: return CallAnyValueFunc("AnyValAsFloat32", {addr, ToFloat32(args[0]->Generate(scope))});
    case AVM_AS_FLOAT64: return CallAnyValueFunc("AnyValAsFloat64", {addr, ToFloat64(args[0]->Generate(scope))});
    case AVM_SET_FLOAT:  CallAnyValueFunc("AnyValSetFloat", {addr, ToFloat64(args[0]->Generate(scope))}); return nullptr;
    case AVM_SET_FLOAT32: CallAnyValueFunc("AnyValSetFloat32", {addr, ToFloat32(args[0]->Generate(scope))}); return nullptr;
    case AVM_SET_FLOAT64: CallAnyValueFunc("AnyValSetFloat64", {addr, ToFloat64(args[0]->Generate(scope))}); return nullptr;
    case AVM_IS_TEXT:    return CallAnyValueFunc("AnyValIsText", {addr});
    case AVM_AS_TEXT:
    {
      LlValue * defaddr = GenerateTextInfoAddress(scope, args[0]);
      LlValue * viewaddr = CreateEntryBlockAlloca(g_builtins->type_strview->GetLlType(), nullptr, "any.text.view");
      CallAnyValueFunc("AnyValAsText", {addr, defaddr, viewaddr});
      return ll_builder.CreateLoad(g_builtins->type_strview->GetLlType(), viewaddr, "any.text");
    }
    case AVM_SET_TEXT:   CallAnyValueFunc("AnyValSetText", {addr, GenerateTextInfoAddress(scope, args[0])}); return nullptr;
    case AVM_SET_CSTRING: CallAnyValueFunc("AnyValSetCString", {addr, GenerateTextInfoAddress(scope, args[0])}); return nullptr;
    case AVM_IS_STR:     return CallAnyValueFunc("AnyValIsStr", {addr});
    case AVM_AS_STR:     return CallAnyValueFunc("AnyValAsStr", {addr, GenerateTextInfoAddress(scope, args[0])});
    case AVM_SET_STR:
    {
      OType * srctype = args[0]->ResolvedType();
      if (srctype && TK_DYNSTR == srctype->kind)
      {
        LlValue * srcstr = args[0]->Generate(scope);
        CallAnyValueFunc("AnyValSetStr", {addr, srcstr});
        if (!dynamic_cast<OLValueExpr *>(args[0]))
        {
          LlValue * tmp = CreateEntryBlockAlloca(g_builtins->type_str->GetLlType(), nullptr, "any.setstr.tmp");
          ll_builder.CreateStore(srcstr, tmp);
          GenerateStringDestroy(scope, tmp);
        }
      }
      else
      {
        CallAnyValueFunc("AnyValSetStrText", {addr, GenerateTextInfoAddress(scope, args[0])});
      }
      return nullptr;
    }
  }
  throw logic_error("Unhandled anyvalue method");
}


bool OTypeAnyValue::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (TK_ANYVALUE != tks)
  {
    if (!IsAnyValueSourceType(resolved_src))
    {
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
      return false;
    }
    if (!EnsureAnyValueRtlUse()) return false;
    *rexpr = new OAnyValueBoxExpr(src, this);
    return true;
  }
  return true;
}

int OTypeAnyValue::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  if (TK_ANYVALUE != tks)
  {
    return IsAnyValueSourceType(resolved_src) ? 1 : -1;
  }
  return 0;
}
