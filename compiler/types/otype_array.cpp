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

static LlValue * DynFieldAddr(OTypeDynArray * dyntype, LlValue * dynaddr, unsigned fieldidx, const char * name)
{
  return ll_builder.CreateStructGEP(dyntype->GetLlType(), dynaddr, fieldidx, name);
}

static LlValue * LoadDynField(OTypeDynArray * dyntype, LlValue * dynaddr, unsigned fieldidx, const char * name)
{
  LlType * ftype = (0 == fieldidx ? LlPtrType() : LlNativeUIntType());
  return ll_builder.CreateLoad(ftype, DynFieldAddr(dyntype, dynaddr, fieldidx, name), name);
}

static OValSymFunc * RawDynArrayMethod(const string & name)
{
  auto nsit = g_namespaces.find("__dq_dynarray");
  if (nsit == g_namespaces.end() || !nsit->second)
  {
    throw runtime_error("Dynamic array RTL module is not loaded");
  }

  OType * rawtype = nsit->second->FindType("ORawDynArray", nullptr, false);
  auto * rawobj = dynamic_cast<OTypeObject *>(rawtype ? rawtype->ResolveAlias() : nullptr);
  if (!rawobj)
  {
    throw runtime_error("Dynamic array RTL type ORawDynArray is not available");
  }

  OValSym * vs = rawobj->Members()->FindValSym(name, nullptr, false);
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
    throw runtime_error("Dynamic array RTL method is not available: ORawDynArray." + name);
  }
  return fn;
}

static LlValue * CallRawDynArrayMethod(const string & name, LlValue * dynaddr, vector<LlValue *> args = {})
{
  OValSymFunc * fn = RawDynArrayMethod(name);
  vector<LlValue *> ll_args;
  ll_args.reserve(args.size() + 1);
  ll_args.push_back(dynaddr);
  ll_args.insert(ll_args.end(), args.begin(), args.end());
  return ll_builder.CreateCall(fn->ll_func, ll_args);
}

static LlValue * IntExprValue(OScope * scope, OExpr * expr)
{
  return ToNativeUInt(expr->Generate(scope));
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

// OTypeDynArray -- ORawDynArray-compatible descriptor {ptr, len, cap, elemsize}

LlType * OTypeDynArray::CreateLlType()
{
  vector<LlType *> fields = {
    LlPtrType(),
    LlNativeUIntType(),
    LlNativeUIntType(),
    LlNativeUIntType()
  };
  return llvm::StructType::get(ll_ctx, fields);
}

LlDiType * OTypeDynArray::CreateDiType()
{
  LlDiType * ptr_di = di_builder->createPointerType(elemtype->GetDiType(), TARGET_PTRSIZE * 8);
  LlDiType * uint_di = di_builder->createBasicType("uint", TARGET_PTRSIZE * 8, llvm::dwarf::DW_ATE_unsigned);
  llvm::Metadata * elements[] = {
    di_builder->createMemberType(nullptr, "dataptr", nullptr, 0, TARGET_PTRSIZE * 8, 0,
        0, llvm::DINode::FlagZero, ptr_di),
    di_builder->createMemberType(nullptr, "length", nullptr, 0, TARGET_PTRSIZE * 8, 0,
        TARGET_PTRSIZE * 8, llvm::DINode::FlagZero, uint_di),
    di_builder->createMemberType(nullptr, "capacity", nullptr, 0, TARGET_PTRSIZE * 8, 0,
        TARGET_PTRSIZE * 16, llvm::DINode::FlagZero, uint_di),
    di_builder->createMemberType(nullptr, "elemsize", nullptr, 0, TARGET_PTRSIZE * 8, 0,
        TARGET_PTRSIZE * 24, llvm::DINode::FlagZero, uint_di)
  };
  return di_builder->createStructType(
      nullptr, name, nullptr, 0, bytesize * 8, 0,
      llvm::DINode::FlagZero, nullptr,
      di_builder->getOrCreateArray(elements)
  );
}

LlValue * GenerateDynArrayDataPtr(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  return LoadDynField(dyntype, dynaddr, 0, "dyn.ptr");
}

LlValue * GenerateDynArrayLength(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  return LoadDynField(dyntype, dynaddr, 1, "dyn.len");
}

LlValue * GenerateDynArrayElementAddress(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * index)
{
  (void)scope;
  (void)dyntype;
  return CallRawDynArrayMethod("GetElemPtrSigned", dynaddr, {ToNativeUInt(index)});
}

LlValue * GenerateDynArraySlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr,
                                OExpr * start_expr, OExpr * end_expr, OType * slicetype)
{
  (void)dyntype;
  LlValue * descaddr = ll_builder.CreateAlloca(slicetype->GetLlType(), nullptr, "dyn.slice.desc");
  if (!start_expr && !end_expr)
  {
    CallRawDynArrayMethod("GetFullSliceDesc", dynaddr, {descaddr});
  }
  else
  {
    LlValue * start = start_expr ? IntExprValue(scope, start_expr) : LlZero();
    LlValue * end = end_expr ? IntExprValue(scope, end_expr) : GenerateDynArrayLength(scope, dyntype, dynaddr);
    CallRawDynArrayMethod("GetSliceDesc", dynaddr, {descaddr, start, end});
  }
  return ll_builder.CreateLoad(slicetype->GetLlType(), descaddr, "dyn.slice");
}

void GenerateDynArrayCreate(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  CallRawDynArrayMethod("Create", dynaddr, {LlConstUInt(dyntype->elemtype->bytesize), LlOne()});
}

void GenerateDynArrayDestroy(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  CallRawDynArrayMethod("Destroy", dynaddr);
}

void GenerateDynArrayClear(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  CallRawDynArrayMethod("Clear", dynaddr);
}

void GenerateDynArrayReserve(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * min_capacity)
{
  (void)dyntype;
  CallRawDynArrayMethod("Reserve", dynaddr, {IntExprValue(scope, min_capacity)});
}

void GenerateDynArrayCompact(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  CallRawDynArrayMethod("Compact", dynaddr);
}

void GenerateDynArraySetLength(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * new_length)
{
  (void)dyntype;
  CallRawDynArrayMethod("SetLength", dynaddr, {IntExprValue(scope, new_length)});
}

void GenerateDynArrayAppend(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * value)
{
  LlValue * tmp = ll_builder.CreateAlloca(dyntype->elemtype->GetLlType(), nullptr, "dyn.append.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallRawDynArrayMethod("Append", dynaddr, {tmp, LlOne()});
}

void GenerateDynArrayAppendSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * values)
{
  (void)dyntype;
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "append.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "append.count");
  CallRawDynArrayMethod("Append", dynaddr, {src, count});
}

void GenerateDynArrayInsert(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * value)
{
  LlValue * idx = IntExprValue(scope, index);
  LlValue * tmp = ll_builder.CreateAlloca(dyntype->elemtype->GetLlType(), nullptr, "dyn.insert.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallRawDynArrayMethod("InsertAt", dynaddr, {idx, tmp, LlOne()});
}

void GenerateDynArrayInsertSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * values)
{
  (void)dyntype;
  LlValue * idx = IntExprValue(scope, index);
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "insert.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "insert.count");
  CallRawDynArrayMethod("InsertAt", dynaddr, {idx, src, count});
}

void GenerateDynArrayDelete(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * count)
{
  (void)dyntype;
  LlValue * idx = IntExprValue(scope, index);
  LlValue * cnt = count ? IntExprValue(scope, count) : LlOne();
  CallRawDynArrayMethod("DeleteAt", dynaddr, {idx, cnt});
}
