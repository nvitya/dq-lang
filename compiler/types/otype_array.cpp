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

static llvm::FunctionCallee LibcMalloc()
{
  auto * ftype = llvm::FunctionType::get(LlPtrType(), {LlNativeUIntType()}, false);
  return ll_module->getOrInsertFunction("malloc", ftype);
}

static llvm::FunctionCallee LibcFree()
{
  auto * ftype = llvm::FunctionType::get(LlType::getVoidTy(ll_ctx), {LlPtrType()}, false);
  return ll_module->getOrInsertFunction("free", ftype);
}

static llvm::FunctionCallee LibcRealloc()
{
  auto * ftype = llvm::FunctionType::get(LlPtrType(), {LlPtrType(), LlNativeUIntType()}, false);
  return ll_module->getOrInsertFunction("realloc", ftype);
}

static llvm::FunctionCallee LibcMemmove()
{
  auto * ftype = llvm::FunctionType::get(LlPtrType(), {LlPtrType(), LlPtrType(), LlNativeUIntType()}, false);
  return ll_module->getOrInsertFunction("memmove", ftype);
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

static void StoreDynField(OTypeDynArray * dyntype, LlValue * dynaddr, unsigned fieldidx, LlValue * value)
{
  LlValue * addr = DynFieldAddr(dyntype, dynaddr, fieldidx, "dyn.field.addr");
  ll_builder.CreateStore(value, addr);
}

static LlValue * DynByteSize(OTypeDynArray * dyntype, LlValue * count)
{
  return ll_builder.CreateMul(ToNativeUInt(count), LlConstUInt(dyntype->elemtype->bytesize), "dyn.bytes");
}

static LlValue * NormalizeDynIndex(OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * index)
{
  LlValue * idx = ToNativeUInt(index);
  LlValue * len = GenerateDynArrayLength(nullptr, dyntype, dynaddr);
  LlValue * is_neg = ll_builder.CreateICmpSLT(idx, LlZero(), "dyn.idx.neg");
  return ll_builder.CreateSelect(is_neg, ll_builder.CreateAdd(len, idx, "dyn.idx.from_end"), idx, "dyn.idx");
}

static LlValue * ClampMinMax(LlValue * value, LlValue * lo, LlValue * hi, const char * name)
{
  LlValue * lt_lo = ll_builder.CreateICmpSLT(value, lo);
  LlValue * at_least_lo = ll_builder.CreateSelect(lt_lo, lo, value);
  LlValue * gt_hi = ll_builder.CreateICmpSGT(at_least_lo, hi);
  return ll_builder.CreateSelect(gt_hi, hi, at_least_lo, name);
}

static LlValue * EvalOptionalBound(OScope * scope, OExpr * expr, LlValue * defval)
{
  if (!expr)
  {
    return defval;
  }
  return ToNativeUInt(expr->Generate(scope));
}

static LlValue * NormalizeSliceBound(OScope * scope, OExpr * expr, LlValue * defval, LlValue * len, const char * name)
{
  LlValue * val = EvalOptionalBound(scope, expr, defval);
  LlValue * is_neg = ll_builder.CreateICmpSLT(val, LlZero());
  val = ll_builder.CreateSelect(is_neg, ll_builder.CreateAdd(len, val), val);
  return ClampMinMax(val, LlZero(), len, name);
}

static void DynReallocToCapacity(OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * newcap)
{
  LlValue * oldptr = LoadDynField(dyntype, dynaddr, 0, "dyn.ptr");
  LlValue * newbytes = DynByteSize(dyntype, newcap);
  LlValue * newptr = ll_builder.CreateCall(LibcRealloc(), {oldptr, newbytes}, "dyn.realloc");
  StoreDynField(dyntype, dynaddr, 0, newptr);
  StoreDynField(dyntype, dynaddr, 2, newcap);
}

static void DynReserveValue(OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * mincap)
{
  mincap = ToNativeUInt(mincap);
  LlValue * cap = LoadDynField(dyntype, dynaddr, 2, "dyn.cap");
  LlValue * grow = ll_builder.CreateICmpUGT(mincap, cap);

  LlFunction * func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * grow_bb = LlBasicBlock::Create(ll_ctx, "dyn.reserve.grow", func);
  LlBasicBlock * done_bb = LlBasicBlock::Create(ll_ctx, "dyn.reserve.done", func);
  ll_builder.CreateCondBr(grow, grow_bb, done_bb);
  ll_builder.SetInsertPoint(grow_bb);
  DynReallocToCapacity(dyntype, dynaddr, mincap);
  ll_builder.CreateBr(done_bb);
  ll_builder.SetInsertPoint(done_bb);
}

static void DynEnsureAppendCapacity(OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * newlen)
{
  newlen = ToNativeUInt(newlen);
  LlValue * cap = LoadDynField(dyntype, dynaddr, 2, "dyn.cap");
  LlValue * grow = ll_builder.CreateICmpUGT(newlen, cap);

  LlFunction * func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * grow_bb = LlBasicBlock::Create(ll_ctx, "dyn.grow", func);
  LlBasicBlock * done_bb = LlBasicBlock::Create(ll_ctx, "dyn.grow.done", func);
  ll_builder.CreateCondBr(grow, grow_bb, done_bb);
  ll_builder.SetInsertPoint(grow_bb);
  LlValue * doubled = ll_builder.CreateMul(cap, LlConstUInt(2), "dyn.cap2");
  LlValue * doubled_small = ll_builder.CreateICmpULT(doubled, newlen);
  LlValue * newcap = ll_builder.CreateSelect(doubled_small, newlen, doubled, "dyn.newcap");
  DynReallocToCapacity(dyntype, dynaddr, newcap);
  ll_builder.CreateBr(done_bb);
  ll_builder.SetInsertPoint(done_bb);
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
  LlValue * idx = NormalizeDynIndex(dyntype, dynaddr, index);
  LlValue * ptr = GenerateDynArrayDataPtr(nullptr, dyntype, dynaddr);
  return ll_builder.CreateGEP(dyntype->elemtype->GetLlType(), ptr, {idx}, "dyn.elem");
}

LlValue * GenerateDynArraySlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr,
                                OExpr * start_expr, OExpr * end_expr, OType * slicetype)
{
  LlValue * len = GenerateDynArrayLength(scope, dyntype, dynaddr);
  LlValue * start = NormalizeSliceBound(scope, start_expr, LlZero(), len, "dyn.slice.start");
  LlValue * end = NormalizeSliceBound(scope, end_expr, len, len, "dyn.slice.end");
  LlValue * end_lt_start = ll_builder.CreateICmpSLT(end, start);
  end = ll_builder.CreateSelect(end_lt_start, start, end, "dyn.slice.end2");

  LlValue * ptr = GenerateDynArrayDataPtr(scope, dyntype, dynaddr);
  LlValue * slice_ptr = ll_builder.CreateGEP(dyntype->elemtype->GetLlType(), ptr, {start}, "dyn.slice.ptr");
  LlValue * slice_len = ll_builder.CreateSub(end, start, "dyn.slice.len");

  LlValue * ll_slice = llvm::UndefValue::get(slicetype->GetLlType());
  ll_slice = ll_builder.CreateInsertValue(ll_slice, slice_ptr, 0, "slice.ptr");
  ll_slice = ll_builder.CreateInsertValue(ll_slice, slice_len, 1, "slice.len");
  return ll_slice;
}

void GenerateDynArrayCreate(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  LlValue * cap = LlOne();
  LlValue * ptr = ll_builder.CreateCall(LibcMalloc(), {DynByteSize(dyntype, cap)}, "dyn.alloc");
  StoreDynField(dyntype, dynaddr, 0, ptr);
  StoreDynField(dyntype, dynaddr, 1, LlZero());
  StoreDynField(dyntype, dynaddr, 2, cap);
  StoreDynField(dyntype, dynaddr, 3, LlConstUInt(dyntype->elemtype->bytesize));
}

void GenerateDynArrayDestroy(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  LlValue * ptr = GenerateDynArrayDataPtr(scope, dyntype, dynaddr);
  ll_builder.CreateCall(LibcFree(), {ptr});
  StoreDynField(dyntype, dynaddr, 0, llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)));
  StoreDynField(dyntype, dynaddr, 1, LlZero());
  StoreDynField(dyntype, dynaddr, 2, LlZero());
}

void GenerateDynArrayClear(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  StoreDynField(dyntype, dynaddr, 1, LlZero());
}

void GenerateDynArrayReserve(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * min_capacity)
{
  DynReserveValue(dyntype, dynaddr, min_capacity->Generate(scope));
}

void GenerateDynArrayCompact(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  LlValue * len = GenerateDynArrayLength(scope, dyntype, dynaddr);
  DynReserveValue(dyntype, dynaddr, len);
  DynReallocToCapacity(dyntype, dynaddr, len);
}

void GenerateDynArraySetLength(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * new_length)
{
  LlValue * newlen = ToNativeUInt(new_length->Generate(scope));
  DynReserveValue(dyntype, dynaddr, newlen);
  StoreDynField(dyntype, dynaddr, 1, newlen);
}

void GenerateDynArrayAppend(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * value)
{
  LlValue * oldlen = GenerateDynArrayLength(scope, dyntype, dynaddr);
  LlValue * newlen = ll_builder.CreateAdd(oldlen, LlOne(), "dyn.append.len");
  DynEnsureAppendCapacity(dyntype, dynaddr, newlen);
  LlValue * dest = GenerateDynArrayElementAddress(scope, dyntype, dynaddr, oldlen);
  LlValue * val = value->Generate(scope);
  ll_builder.CreateStore(val, dest);
  StoreDynField(dyntype, dynaddr, 1, newlen);
}

void GenerateDynArrayAppendSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * values)
{
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "append.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "append.count");
  LlValue * oldlen = GenerateDynArrayLength(scope, dyntype, dynaddr);
  LlValue * newlen = ll_builder.CreateAdd(oldlen, count, "dyn.append.len");
  DynEnsureAppendCapacity(dyntype, dynaddr, newlen);
  LlValue * dest = GenerateDynArrayElementAddress(scope, dyntype, dynaddr, oldlen);
  ll_builder.CreateCall(LibcMemmove(), {dest, src, DynByteSize(dyntype, count)});
  StoreDynField(dyntype, dynaddr, 1, newlen);
}

static LlValue * NormalizeInsertIndex(OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * index)
{
  LlValue * len = GenerateDynArrayLength(nullptr, dyntype, dynaddr);
  LlValue * idx = ToNativeUInt(index);
  LlValue * is_neg = ll_builder.CreateICmpSLT(idx, LlZero());
  idx = ll_builder.CreateSelect(is_neg, ll_builder.CreateAdd(len, idx), idx);
  return ClampMinMax(idx, LlZero(), len, "dyn.insert.idx");
}

static void DynInsertBytes(OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * index, LlValue * src, LlValue * count)
{
  LlValue * len = GenerateDynArrayLength(nullptr, dyntype, dynaddr);
  LlValue * newlen = ll_builder.CreateAdd(len, count, "dyn.insert.len");
  DynEnsureAppendCapacity(dyntype, dynaddr, newlen);

  LlValue * ptr = GenerateDynArrayDataPtr(nullptr, dyntype, dynaddr);
  LlValue * move_src = ll_builder.CreateGEP(dyntype->elemtype->GetLlType(), ptr, {index}, "dyn.ins.src");
  LlValue * move_dst_idx = ll_builder.CreateAdd(index, count);
  LlValue * move_dst = ll_builder.CreateGEP(dyntype->elemtype->GetLlType(), ptr, {move_dst_idx}, "dyn.ins.dst");
  LlValue * move_count = ll_builder.CreateSub(len, index, "dyn.ins.movecount");
  ll_builder.CreateCall(LibcMemmove(), {move_dst, move_src, DynByteSize(dyntype, move_count)});
  ll_builder.CreateCall(LibcMemmove(), {move_src, src, DynByteSize(dyntype, count)});
  StoreDynField(dyntype, dynaddr, 1, newlen);
}

void GenerateDynArrayInsert(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * value)
{
  LlValue * idx = NormalizeInsertIndex(dyntype, dynaddr, index->Generate(scope));
  LlValue * tmp = ll_builder.CreateAlloca(dyntype->elemtype->GetLlType(), nullptr, "dyn.insert.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  DynInsertBytes(dyntype, dynaddr, idx, tmp, LlOne());
}

void GenerateDynArrayInsertSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * values)
{
  LlValue * idx = NormalizeInsertIndex(dyntype, dynaddr, index->Generate(scope));
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "insert.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "insert.count");
  DynInsertBytes(dyntype, dynaddr, idx, src, count);
}

void GenerateDynArrayDelete(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * count)
{
  LlValue * len = GenerateDynArrayLength(scope, dyntype, dynaddr);
  LlValue * idx = ToNativeUInt(index->Generate(scope));
  LlValue * is_neg = ll_builder.CreateICmpSLT(idx, LlZero());
  idx = ll_builder.CreateSelect(is_neg, ll_builder.CreateAdd(len, idx), idx, "dyn.del.idx");
  LlValue * cnt = count ? ToNativeUInt(count->Generate(scope)) : LlOne();
  LlValue * invalid = ll_builder.CreateOr(ll_builder.CreateICmpSLT(idx, LlZero()),
      ll_builder.CreateOr(ll_builder.CreateICmpUGE(idx, len), ll_builder.CreateICmpSLE(cnt, LlZero())));

  LlFunction * func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * run_bb = LlBasicBlock::Create(ll_ctx, "dyn.delete.run", func);
  LlBasicBlock * done_bb = LlBasicBlock::Create(ll_ctx, "dyn.delete.done", func);
  ll_builder.CreateCondBr(invalid, done_bb, run_bb);
  ll_builder.SetInsertPoint(run_bb);

  LlValue * avail = ll_builder.CreateSub(len, idx);
  LlValue * too_many = ll_builder.CreateICmpUGT(cnt, avail);
  cnt = ll_builder.CreateSelect(too_many, avail, cnt);
  LlValue * ptr = GenerateDynArrayDataPtr(scope, dyntype, dynaddr);
  LlValue * dst = ll_builder.CreateGEP(dyntype->elemtype->GetLlType(), ptr, {idx}, "dyn.del.dst");
  LlValue * src_idx = ll_builder.CreateAdd(idx, cnt);
  LlValue * src = ll_builder.CreateGEP(dyntype->elemtype->GetLlType(), ptr, {src_idx}, "dyn.del.src");
  LlValue * move_count = ll_builder.CreateSub(len, src_idx);
  ll_builder.CreateCall(LibcMemmove(), {dst, src, DynByteSize(dyntype, move_count)});
  StoreDynField(dyntype, dynaddr, 1, ll_builder.CreateSub(len, cnt));
  ll_builder.CreateBr(done_bb);
  ll_builder.SetInsertPoint(done_bb);
}
