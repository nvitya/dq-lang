/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_array.cpp
 * authors: nvitya
 * created: 2026-03-06
 * brief:   Array types: fixed-size array and array slice (descriptor)
 */

#include <vector>
#include "otype_array.h"
#include "dqm_if.h"
#include "expressions.h"
#include "dqc.h"
#include "named_scopes.h"
#include "otype_func.h"
#include "otype_object.h"
#include "scope_builtins.h"
#include <cctype>
#include <llvm/IR/GlobalVariable.h>

using namespace std;

static LlType * LlPtrType()
{
  return llvm::PointerType::get(ll_ctx, 0);
}

static LlType * LlNativeUIntType()
{
  return g_builtins->native_uint->GetLlType();
}

static LlValue * LlZero()
{
  return llvm::ConstantInt::get(LlNativeUIntType(), 0);
}

static LlValue * LlOne()
{
  return llvm::ConstantInt::get(LlNativeUIntType(), 1);
}

static LlValue * LlConstUInt(uint64_t value)
{
  return llvm::ConstantInt::get(LlNativeUIntType(), value);
}

static LlValue * ToNativeUInt(LlValue * value)
{
  LlType * dst = LlNativeUIntType();
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
    return ll_builder.CreateSExt(value, dst, "u.ext");
  }
  if (srcbits > dstbits)
  {
    return ll_builder.CreateTrunc(value, dst, "u.trunc");
  }
  return value;
}

static LlValue * LlBool(bool value)
{
  return llvm::ConstantInt::get(g_builtins->type_bool->GetLlType(), value);
}

static OValSymFunc * DynArrayFunc(const string & name)
{
  auto nsit = g_namespaces.find("__dq_dynarray");
  if (nsit == g_namespaces.end() || !nsit->second)
  {
    throw runtime_error("Dynamic array RTL module is not loaded");
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
    throw runtime_error("Dynamic array RTL function is not available: " + name);
  }
  return fn;
}

static LlValue * CallDynArrayFunc(const string & name, vector<LlValue *> args = {})
{
  OValSymFunc * fn = DynArrayFunc(name);
  return ll_builder.CreateCall(fn->ll_func, args);
}

static LlValue * IntExprValue(OScope * scope, OExpr * expr)
{
  return ToNativeUInt(expr->Generate(scope));
}

static string SanitizeLlName(const string & src)
{
  string result;
  result.reserve(src.size());
  for (unsigned char ch : src)
  {
    result.push_back(isalnum(ch) ? char(ch) : '_');
  }
  return result;
}

static LlValue * DynArrayTypeInfo(OTypeDynArray * dyntype)
{
  string gvname = "__dq_dynarr_typeinfo_" + SanitizeLlName(dyntype->elemtype->name);
  if (auto * existing = ll_module->getNamedGlobal(gvname))
  {
    return existing;
  }

  LlType * ptrtype = LlPtrType();
  LlType * i32type = LlType::getInt32Ty(ll_ctx);
  LlType * i16type = LlType::getInt16Ty(ll_ctx);
  LlType * i8type = LlType::getInt8Ty(ll_ctx);
  vector<LlType *> fields = {
    i32type, i16type, i8type, i8type,
    ptrtype, ptrtype, ptrtype, ptrtype
  };
  auto * ti_type = llvm::StructType::get(ll_ctx, fields);
  OType * elemtype = dyntype->elemtype->ResolveAlias();
  vector<LlConst *> values = {
    llvm::ConstantInt::get(i32type, elemtype->bytesize),
    llvm::ConstantInt::get(i16type, 0),
    llvm::ConstantInt::get(i8type, uint8_t(elemtype->kind)),
    llvm::ConstantInt::get(i8type, 0),
    llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)),
    llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)),
    llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)),
    llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0))
  };
  auto * init = llvm::ConstantStruct::get(ti_type, values);
  return new llvm::GlobalVariable(*ll_module, ti_type, true, llvm::GlobalValue::PrivateLinkage, init, gvname);
}

OValueArray::OValueArray(OTypeArray * atype)
:
  super(atype)
{
  elements.reserve(atype->arraylength);
  for (uint32_t i = 0; i < atype->arraylength; ++i)
  {
    OValue * elemvalue = atype->elemtype->CreateValue();
    if (!elemvalue)
    {
      throw logic_error(format("Array element type \"{}\" does not support constant values", atype->elemtype->name));
    }
    elements.push_back(elemvalue);
  }
}

OValueArray::~OValueArray()
{
  for (OValue * elem : elements)
  {
    delete elem;
  }
}

LlConst * OValueArray::CreateLlConst()
{
  vector<LlConst *> ll_elems;
  ll_elems.reserve(elements.size());
  for (OValue * elem : elements)
  {
    ll_elems.push_back(elem->GetLlConst());
  }

  auto * ll_arrtype = static_cast<llvm::ArrayType *>(ptype->GetLlType());
  return llvm::ConstantArray::get(ll_arrtype, ll_elems);
}

bool OValueArray::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecEmpty(DQMIF_VALUE_LINKED);
}

bool OValueArray::CalculateConstant(OExpr * expr, bool emit_errors)
{
  auto * arrtype = static_cast<OTypeArray *>(ptype);

  if (auto * arrlit = dynamic_cast<OArrayLit *>(expr))
  {
    if (arrlit->elements.size() != arrtype->arraylength)
    {
      g_compiler->Error(DQERR_ARR_ELEMCOUNT_MISM, to_string(arrtype->arraylength), to_string(arrlit->elements.size()));
      return false;
    }

    for (size_t i = 0; i < arrlit->elements.size(); ++i)
    {
      if (!elements[i]->CalculateConstant(arrlit->elements[i], emit_errors))
      {
        return false;
      }
    }

    return true;
  }

  if (emit_errors)
  {
    g_compiler->Error(DQERR_ARRAY_CONSTEXPR);
  }
  return false;
}

// OTypeArray -- Fixed-size array

OValue * OTypeArray::CreateValue()
{
  return new OValueArray(this);
}

LlType * OTypeArray::CreateLlType()
{
  return llvm::ArrayType::get(elemtype->GetLlType(), arraylength);
}

LlDiType * OTypeArray::CreateDiType()
{
  llvm::Metadata * subscripts[] = {
    di_builder->getOrCreateSubrange(0, arraylength)
  };
  return di_builder->createArrayType(
      bytesize * 8,
      0,
      elemtype->GetDiType(),
      di_builder->getOrCreateArray(subscripts)
  );
}

// OTypeArraySlice -- Array descriptor {ptr, i64}

LlType * OTypeArraySlice::CreateLlType()
{
  vector<LlType *> fields = {
    llvm::PointerType::get(ll_ctx, 0),
    LlType::getInt64Ty(ll_ctx)
  };
  return llvm::StructType::get(ll_ctx, fields);
}

LlDiType * OTypeArraySlice::CreateDiType()
{
  // For debug info, represent as a struct with pointer and length fields
  LlDiType * ptr_di = di_builder->createPointerType(elemtype->GetDiType(), TARGET_PTRSIZE * 8);
  LlDiType * len_di = di_builder->createBasicType("uint64", 64, llvm::dwarf::DW_ATE_unsigned);

  llvm::Metadata * elements[] = {
    di_builder->createMemberType(
        nullptr, "ptr", nullptr, 0, TARGET_PTRSIZE * 8, 0,
        0, llvm::DINode::FlagZero, ptr_di),
    di_builder->createMemberType(
        nullptr, "len", nullptr, 0, 64, 0,
        TARGET_PTRSIZE * 8, llvm::DINode::FlagZero, len_di)
  };

  return di_builder->createStructType(
      nullptr, name, nullptr, 0, bytesize * 8, 0,
      llvm::DINode::FlagZero, nullptr,
      di_builder->getOrCreateArray(elements)
  );
}

// OTypeDynArray -- nullable ODynArrMgr reference

LlType * OTypeDynArray::CreateLlType()
{
  return LlPtrType();
}

LlDiType * OTypeDynArray::CreateDiType()
{
  LlDiType * mgr_di = di_builder->createBasicType("ODynArrMgr", TARGET_PTRSIZE * 8, llvm::dwarf::DW_ATE_address);
  return di_builder->createPointerType(mgr_di, TARGET_PTRSIZE * 8);
}

LlValue * GenerateDynArrayDataPtr(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  LlValue * descaddr = CreateEntryBlockAlloca(dyntype->elemtype->GetSliceType()->GetLlType(), nullptr, "dyn.data.desc");
  CallDynArrayFunc("DynArrGetFullSlice", {dynaddr, descaddr});
  LlValue * desc = ll_builder.CreateLoad(dyntype->elemtype->GetSliceType()->GetLlType(), descaddr, "dyn.data.slice");
  return ll_builder.CreateExtractValue(desc, {0}, "dyn.ptr");
}

LlValue * GenerateDynArrayLength(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  return CallDynArrayFunc("DynArrGetLength", {dynaddr});
}

LlValue * GenerateDynArrayCapacity(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  return CallDynArrayFunc("DynArrGetCapacity", {dynaddr});
}

LlValue * GenerateDynArrayElementAddress(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * index)
{
  (void)scope;
  (void)dyntype;
  return CallDynArrayFunc("DynArrGetElemPtr", {dynaddr, ToNativeUInt(index)});
}

LlValue * GenerateDynArraySlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr,
                                OExpr * start_expr, OExpr * end_expr, OType * slicetype)
{
  (void)dyntype;
  LlValue * descaddr = CreateEntryBlockAlloca(slicetype->GetLlType(), nullptr, "dyn.slice.desc");
  if (!start_expr && !end_expr)
  {
    CallDynArrayFunc("DynArrGetFullSlice", {dynaddr, descaddr});
  }
  else
  {
    LlValue * start = start_expr ? IntExprValue(scope, start_expr) : LlZero();
    LlValue * end = end_expr ? IntExprValue(scope, end_expr) : GenerateDynArrayLength(scope, dyntype, dynaddr);
    CallDynArrayFunc("DynArrGetSlice", {dynaddr, descaddr, start, end});
  }
  return ll_builder.CreateLoad(slicetype->GetLlType(), descaddr, "dyn.slice");
}

void GenerateDynArrayCreate(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  ll_builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)), dynaddr);
}

void GenerateDynArrayDestroy(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  CallDynArrayFunc("DynArrDecRef", {dynaddr});
}

LlValue * GenerateDynArrayManagerValue(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  return ll_builder.CreateLoad(dyntype->GetLlType(), dynaddr, "dyn.mgr");
}

void GenerateDynArrayAssignOther(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * srcmgr)
{
  (void)scope;
  (void)dyntype;
  CallDynArrayFunc("DynArrAssignOther", {dynaddr, srcmgr});
}

void GenerateDynArrayAssignData(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * srcptr, LlValue * count)
{
  (void)scope;
  CallDynArrayFunc("DynArrAssignData", {dynaddr, DynArrayTypeInfo(dyntype), srcptr, ToNativeUInt(count)});
}

void GenerateDynArrayClear(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  CallDynArrayFunc("DynArrClear", {dynaddr, LlBool(false)});
}

void GenerateDynArrayClear(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * free_storage)
{
  (void)dyntype;
  CallDynArrayFunc("DynArrClear", {dynaddr, free_storage ? free_storage->Generate(scope) : LlBool(false)});
}

void GenerateDynArrayReserve(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * min_capacity)
{
  CallDynArrayFunc("DynArrReserve", {dynaddr, DynArrayTypeInfo(dyntype), IntExprValue(scope, min_capacity)});
}

void GenerateDynArrayCompact(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  CallDynArrayFunc("DynArrCompact", {dynaddr});
}

void GenerateDynArraySetLength(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * new_length)
{
  CallDynArrayFunc("DynArrSetLength", {dynaddr, DynArrayTypeInfo(dyntype), IntExprValue(scope, new_length)});
}

void GenerateDynArraySetCapacity(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * new_capacity)
{
  CallDynArrayFunc("DynArrSetCapacity", {dynaddr, DynArrayTypeInfo(dyntype), IntExprValue(scope, new_capacity)});
}

void GenerateDynArrayAppend(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * value)
{
  LlValue * tmp = CreateEntryBlockAlloca(dyntype->elemtype->GetLlType(), nullptr, "dyn.append.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallDynArrayFunc("DynArrAppend", {dynaddr, DynArrayTypeInfo(dyntype), tmp, LlOne()});
}

void GenerateDynArrayAppendSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * values)
{
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "append.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "append.count");
  CallDynArrayFunc("DynArrAppend", {dynaddr, DynArrayTypeInfo(dyntype), src, count});
}

void GenerateDynArrayPrepend(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * value)
{
  LlValue * tmp = CreateEntryBlockAlloca(dyntype->elemtype->GetLlType(), nullptr, "dyn.prepend.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallDynArrayFunc("DynArrInsert", {dynaddr, DynArrayTypeInfo(dyntype), LlZero(), tmp, LlOne()});
}

void GenerateDynArrayPrependSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * values)
{
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "prepend.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "prepend.count");
  CallDynArrayFunc("DynArrInsert", {dynaddr, DynArrayTypeInfo(dyntype), LlZero(), src, count});
}

void GenerateDynArrayInsert(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * value)
{
  LlValue * idx = IntExprValue(scope, index);
  LlValue * tmp = CreateEntryBlockAlloca(dyntype->elemtype->GetLlType(), nullptr, "dyn.insert.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallDynArrayFunc("DynArrInsert", {dynaddr, DynArrayTypeInfo(dyntype), idx, tmp, LlOne()});
}

void GenerateDynArrayInsertSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * values)
{
  LlValue * idx = IntExprValue(scope, index);
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "insert.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "insert.count");
  CallDynArrayFunc("DynArrInsert", {dynaddr, DynArrayTypeInfo(dyntype), idx, src, count});
}

void GenerateDynArrayDelete(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * count)
{
  (void)dyntype;
  LlValue * idx = IntExprValue(scope, index);
  LlValue * cnt = count ? IntExprValue(scope, count) : LlOne();
  CallDynArrayFunc("DynArrDelete", {dynaddr, idx, cnt});
}

LlValue * GenerateDynArrayClone(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  LlValue * tmp = CreateEntryBlockAlloca(dyntype->GetLlType(), nullptr, "dyn.clone.tmp");
  ll_builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)), tmp);
  LlValue * srcmgr = GenerateDynArrayManagerValue(scope, dyntype, dynaddr);
  CallDynArrayFunc("DynArrClone", {tmp, DynArrayTypeInfo(dyntype), srcmgr});
  return ll_builder.CreateLoad(dyntype->GetLlType(), tmp, "dyn.clone");
}

LlValue * GenerateDynArrayPop(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, bool first)
{
  (void)scope;
  LlValue * tmp = CreateEntryBlockAlloca(dyntype->elemtype->GetLlType(), nullptr, first ? "dyn.popfirst.tmp" : "dyn.pop.tmp");
  CallDynArrayFunc(first ? "DynArrPopFirst" : "DynArrPop", {dynaddr, tmp});
  return ll_builder.CreateLoad(dyntype->elemtype->GetLlType(), tmp, first ? "dyn.popfirst" : "dyn.pop");
}
