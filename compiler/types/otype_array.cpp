/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_array.cpp
 * authors: nvitya
 * created: 2026-03-06
 * brief:   Array types: fixed-size array and array slice (descriptor)
 */

#include <vector>
#include "dqc_ast.h"
#include "otype_array.h"
#include "dqm_if.h"
#include "expressions.h"
#include "dqc.h"
#include "named_scopes.h"
#include "otype_func.h"
#include "otype_compound.h"
#include "scope_builtins.h"
#include <cctype>
#include <llvm/IR/GlobalVariable.h>
#include "otype_string.h"
#include "otype_anyvalue.h"

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

static LlValue * CallDynArrayFunc(OScope * scope, const string & name, vector<LlValue *> args = {})
{
  OValSymFunc * fn = DynArrayFunc(name);
  LlBasicBlock * bb_cleanup = nullptr;
  if (scope)
  {
    for (OScope * cur = scope; cur && !bb_cleanup; cur = cur->parent_scope)
    {
      bb_cleanup = cur->exception_cleanup_bb;
    }
  }

  if (bb_cleanup)
  {
    LlFunction * cur_func = ll_builder.GetInsertBlock()->getParent();
    if (!cur_func->hasPersonalityFn())
    {
      llvm::FunctionCallee pers_fn = ll_module->getOrInsertFunction("__gxx_personality_v0",
          LlFuncType::get(llvm::Type::getInt32Ty(ll_ctx), {}, true));
      cur_func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));
    }
    LlBasicBlock * bb_normal = LlBasicBlock::Create(ll_ctx, "invoke.cont", cur_func);
    LlValue * result = ll_builder.CreateInvoke(static_cast<LlFuncType *>(fn->ptype->GetLlType()), fn->ll_func, bb_normal, bb_cleanup, args);
    ll_builder.SetInsertPoint(bb_normal);
    return result;
  }

  return ll_builder.CreateCall(fn->ll_func, args);
}

static LlValue * IntExprValue(OScope * scope, OExpr * expr)
{
  return ToNativeUInt(expr->Generate(scope));
}

static OType * DynArrayElementStorageType(OTypeDynArray * dyntype)
{
  OType * elemtype = dyntype->elemtype->ResolveAlias();
  if (TK_OBJECT == elemtype->kind)
  {
    return elemtype->GetPointerType();
  }
  return elemtype;
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

static void GenerateElementDestructor(OType * elemtype, LlValue * elem_addr)
{
  if (auto * dyntype = dynamic_cast<OTypeDynArray *>(elemtype))
  {
    LlValue * ll_mgr = ll_builder.CreateLoad(dyntype->GetLlType(), elem_addr, "dynarr.mgr");
    GenerateDynArrayDestroy(nullptr, dyntype, ll_mgr);
  }
  else if (auto * strtype = dynamic_cast<OTypeDynString *>(elemtype))
  {
    LlValue * ll_str = ll_builder.CreateLoad(strtype->GetLlType(), elem_addr, "dynstr");
    GenerateStringDestroy(nullptr, ll_str);
  }
  else if (auto * anytype = dynamic_cast<OTypeAnyValue *>(elemtype))
  {
    LlValue * ll_any = ll_builder.CreateLoad(anytype->GetLlType(), elem_addr, "anyval");
    GenerateAnyValueDestroy(nullptr, ll_any);
  }
  else if (TK_OBJECT == elemtype->kind)
  {
    OTypeObject * objtype = static_cast<OTypeObject *>(elemtype);
    OValSymFunc * dtor = objtype->FindSpecialMethod(OSF_DESTROY);
    if (dtor && dtor->ll_func)
    {
      ll_builder.CreateCall(dtor->ll_func, {elem_addr});
    }
  }
  else if (TK_ARRAY == elemtype->kind)
  {
    OTypeArray * arrtype = static_cast<OTypeArray *>(elemtype);
    if (arrtype->elemtype->ContainsManagedStorage())
    {
      for (uint32_t i = 0; i < arrtype->arraylength; ++i)
      {
        LlValue * idx = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), i);
        LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
        LlValue * ll_field_addr = ll_builder.CreateGEP(arrtype->GetLlType(), elem_addr, {ll_zero, idx}, "arr.elem.addr");
        GenerateElementDestructor(arrtype->elemtype, ll_field_addr);
      }
    }
  }
  else if (elemtype->IsCompound())
  {
    OCompoundType * comptype = static_cast<OCompoundType *>(elemtype);
    comptype->GetLlType();
    for (OValSym * member : comptype->member_order)
    {
      if (member && member->ptype && member->ptype->ContainsManagedStorage())
      {
        LlValue * ll_field_addr = ll_builder.CreateStructGEP(comptype->GetLlType(), elem_addr,
            member->ll_field_index, member->name + ".addr");
        GenerateElementDestructor(member->GetStorageType(), ll_field_addr);
      }
    }
  }
}

static llvm::Function * GetTypeDestroyFunc(OType * elemtype)
{
  if (!elemtype->ContainsManagedStorage()) return nullptr;
  if (elemtype->kind == TK_DYN_ARRAY || elemtype->kind == TK_DYNSTR)
  {
    return nullptr; // Handled natively by RTL without code bloat
  }

  string func_name = "__dq_destroy_" + SanitizeLlName(elemtype->name);
  if (auto * existing = ll_module->getFunction(func_name))
  {
    return existing;
  }

  LlType * ptr_type = LlPtrType();
  LlType * uint_type = LlNativeUIntType();
  llvm::FunctionType * func_type = llvm::FunctionType::get(LlType::getVoidTy(ll_ctx), {ptr_type, uint_type}, false);
  llvm::Function * func = llvm::Function::Create(func_type, llvm::GlobalValue::InternalLinkage, func_name, ll_module);

  auto arg_it = func->arg_begin();
  llvm::Argument * arg_ptr = &*arg_it++;
  arg_ptr->setName("dataptr");
  llvm::Argument * arg_count = &*arg_it++;
  arg_count->setName("count");

  LlBasicBlock * old_bb = ll_builder.GetInsertBlock();

  LlBasicBlock * bb_entry = LlBasicBlock::Create(ll_ctx, "entry", func);
  LlBasicBlock * bb_cond = LlBasicBlock::Create(ll_ctx, "loop.cond", func);
  LlBasicBlock * bb_body = LlBasicBlock::Create(ll_ctx, "loop.body", func);
  LlBasicBlock * bb_inc = LlBasicBlock::Create(ll_ctx, "loop.inc", func);
  LlBasicBlock * bb_end = LlBasicBlock::Create(ll_ctx, "loop.end", func);

  ll_builder.SetInsertPoint(bb_entry);
  LlValue * ll_idx_ptr = CreateEntryBlockAlloca(uint_type, nullptr, "idx");
  ll_builder.CreateStore(llvm::ConstantInt::get(uint_type, 0), ll_idx_ptr);
  ll_builder.CreateBr(bb_cond);

  ll_builder.SetInsertPoint(bb_cond);
  LlValue * ll_idx = ll_builder.CreateLoad(uint_type, ll_idx_ptr, "idx.val");
  LlValue * ll_cmp = ll_builder.CreateICmpULT(ll_idx, arg_count, "cmp");
  ll_builder.CreateCondBr(ll_cmp, bb_body, bb_end);

  ll_builder.SetInsertPoint(bb_body);
  LlValue * ll_bytesize = llvm::ConstantInt::get(uint_type, elemtype->bytesize);
  LlValue * ll_offset = ll_builder.CreateMul(ll_idx, ll_bytesize, "offset");
  LlValue * ll_elem_addr = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), arg_ptr, ll_offset, "elem.addr");
  
  GenerateElementDestructor(elemtype, ll_elem_addr);

  ll_builder.CreateBr(bb_inc);

  ll_builder.SetInsertPoint(bb_inc);
  LlValue * ll_next_idx = ll_builder.CreateAdd(ll_idx, llvm::ConstantInt::get(uint_type, 1), "idx.next");
  ll_builder.CreateStore(ll_next_idx, ll_idx_ptr);
  ll_builder.CreateBr(bb_cond);

  ll_builder.SetInsertPoint(bb_end);
  ll_builder.CreateRetVoid();

  if (old_bb)
  {
    ll_builder.SetInsertPoint(old_bb);
  }

  return func;
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
  OType * storage_type = DynArrayElementStorageType(dyntype);
  llvm::Function * ll_destroy_func = GetTypeDestroyFunc(storage_type);

  vector<LlConst *> values = {
    llvm::ConstantInt::get(i32type, storage_type->bytesize),
    llvm::ConstantInt::get(i16type, 0),
    llvm::ConstantInt::get(i8type, uint8_t(storage_type->kind)),
    llvm::ConstantInt::get(i8type, 0),
    llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)),
    (ll_destroy_func ? static_cast<llvm::Constant *>(ll_destroy_func) : static_cast<llvm::Constant *>(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)))),
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
  CallDynArrayFunc(scope, "DynArrGetFullSlice", {dynaddr, descaddr});
  LlValue * desc = ll_builder.CreateLoad(dyntype->elemtype->GetSliceType()->GetLlType(), descaddr, "dyn.data.slice");
  return ll_builder.CreateExtractValue(desc, {0}, "dyn.ptr");
}

LlValue * GenerateDynArrayLength(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  return CallDynArrayFunc(scope, "DynArrGetLength", {dynaddr});
}

LlValue * GenerateDynArrayCapacity(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  return CallDynArrayFunc(scope, "DynArrGetCapacity", {dynaddr});
}

LlValue * GenerateDynArrayRefCount(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  return CallDynArrayFunc(scope, "DynArrGetRefCount", {dynaddr});
}

LlValue * GenerateDynArrayElementAddress(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * index)
{
  (void)dyntype;
  LlValue * result = CallDynArrayFunc(scope, "DynArrGetElemPtr", {dynaddr, ToNativeUInt(index)});
  EmitExpressionExceptionCheck(scope);
  return result;
}

LlValue * GenerateDynArraySlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr,
                                OExpr * start_expr, OExpr * end_expr, OType * slicetype)
{
  (void)dyntype;
  LlValue * descaddr = CreateEntryBlockAlloca(slicetype->GetLlType(), nullptr, "dyn.slice.desc");
  if (!start_expr && !end_expr)
  {
    CallDynArrayFunc(scope, "DynArrGetFullSlice", {dynaddr, descaddr});
    EmitExpressionExceptionCheck(scope);
  }
  else
  {
    LlValue * start = start_expr ? IntExprValue(scope, start_expr) : LlZero();
    LlValue * end = end_expr ? IntExprValue(scope, end_expr) : GenerateDynArrayLength(scope, dyntype, dynaddr);
    CallDynArrayFunc(scope, "DynArrGetSlice", {dynaddr, descaddr, start, end});
    EmitExpressionExceptionCheck(scope);
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
  CallDynArrayFunc(scope, "DynArrDecRef", {dynaddr});
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
  CallDynArrayFunc(scope, "DynArrAssignOther", {dynaddr, srcmgr});
}

void GenerateDynArrayAssignData(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * srcptr, LlValue * count)
{
  (void)scope;
  CallDynArrayFunc(scope, "DynArrAssignData", {dynaddr, DynArrayTypeInfo(dyntype), srcptr, ToNativeUInt(count)});
}

void GenerateDynArrayClear(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  CallDynArrayFunc(scope, "DynArrClear", {dynaddr, LlBool(false)});
}

void GenerateDynArrayClear(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * free_storage)
{
  (void)dyntype;
  CallDynArrayFunc(scope, "DynArrClear", {dynaddr, free_storage ? free_storage->Generate(scope) : LlBool(false)});
}

void GenerateDynArrayReserve(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * min_capacity)
{
  CallDynArrayFunc(scope, "DynArrReserve", {dynaddr, DynArrayTypeInfo(dyntype), IntExprValue(scope, min_capacity)});
}

void GenerateDynArrayCompact(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  (void)dyntype;
  CallDynArrayFunc(scope, "DynArrCompact", {dynaddr});
}

void GenerateDynArraySetLength(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * new_length)
{
  CallDynArrayFunc(scope, "DynArrSetLength", {dynaddr, DynArrayTypeInfo(dyntype), IntExprValue(scope, new_length)});
}

void GenerateDynArraySetCapacity(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * new_capacity)
{
  CallDynArrayFunc(scope, "DynArrSetCapacity", {dynaddr, DynArrayTypeInfo(dyntype), IntExprValue(scope, new_capacity)});
}

void GenerateDynArrayAppend(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * value)
{
  LlValue * tmp = CreateEntryBlockAlloca(DynArrayElementStorageType(dyntype)->GetLlType(), nullptr, "dyn.append.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallDynArrayFunc(scope, "DynArrAppend", {dynaddr, DynArrayTypeInfo(dyntype), tmp, LlOne()});
}

void GenerateDynArrayAppendSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * values)
{
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "append.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "append.count");
  CallDynArrayFunc(scope, "DynArrAppend", {dynaddr, DynArrayTypeInfo(dyntype), src, count});
}

void GenerateDynArrayPrepend(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * value)
{
  LlValue * tmp = CreateEntryBlockAlloca(DynArrayElementStorageType(dyntype)->GetLlType(), nullptr, "dyn.prepend.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallDynArrayFunc(scope, "DynArrInsert", {dynaddr, DynArrayTypeInfo(dyntype), LlZero(), tmp, LlOne()});
}

void GenerateDynArrayPrependSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * values)
{
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "prepend.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "prepend.count");
  CallDynArrayFunc(scope, "DynArrInsert", {dynaddr, DynArrayTypeInfo(dyntype), LlZero(), src, count});
}

void GenerateDynArrayInsert(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * value)
{
  LlValue * idx = IntExprValue(scope, index);
  LlValue * tmp = CreateEntryBlockAlloca(DynArrayElementStorageType(dyntype)->GetLlType(), nullptr, "dyn.insert.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallDynArrayFunc(scope, "DynArrInsert", {dynaddr, DynArrayTypeInfo(dyntype), idx, tmp, LlOne()});
}

void GenerateDynArrayInsertSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * values)
{
  LlValue * idx = IntExprValue(scope, index);
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "insert.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "insert.count");
  CallDynArrayFunc(scope, "DynArrInsert", {dynaddr, DynArrayTypeInfo(dyntype), idx, src, count});
}

void GenerateDynArrayDelete(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * count)
{
  (void)dyntype;
  LlValue * idx = IntExprValue(scope, index);
  LlValue * cnt = count ? IntExprValue(scope, count) : LlOne();
  CallDynArrayFunc(scope, "DynArrDelete", {dynaddr, idx, cnt});
}

LlValue * GenerateDynArrayClone(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr)
{
  (void)scope;
  LlValue * tmp = CreateEntryBlockAlloca(dyntype->GetLlType(), nullptr, "dyn.clone.tmp");
  ll_builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)), tmp);
  LlValue * srcmgr = GenerateDynArrayManagerValue(scope, dyntype, dynaddr);
  CallDynArrayFunc(scope, "DynArrClone", {tmp, DynArrayTypeInfo(dyntype), srcmgr});
  return ll_builder.CreateLoad(dyntype->GetLlType(), tmp, "dyn.clone");
}

LlValue * GenerateDynArrayPop(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, bool first)
{
  (void)scope;
  LlValue * tmp = CreateEntryBlockAlloca(DynArrayElementStorageType(dyntype)->GetLlType(), nullptr, first ? "dyn.popfirst.tmp" : "dyn.pop.tmp");
  CallDynArrayFunc(scope, first ? "DynArrPopFirst" : "DynArrPop", {dynaddr, tmp});
  return ll_builder.CreateLoad(DynArrayElementStorageType(dyntype)->GetLlType(), tmp, first ? "dyn.popfirst" : "dyn.pop");
}

static bool ConvertArrayLiteralElements(OArrayLit * arrlit, OType * elemtype, uint32_t arraylength, uint32_t aflags)
{
  if (arrlit->elements.size() != arraylength)
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      g_compiler->Error(DQERR_ARR_SIZE_MISM, to_string(arraylength), to_string(arrlit->elements.size()));
    }
    return false;
  }

  for (OExpr *& elem : arrlit->elements)
  {
    if (!g_compiler->ConvertExprToType(elemtype, &elem, aflags | EXPCF_ALLOW_LAZY_CSTRING))
    {
      return false;
    }
  }
  arrlit->ptype = elemtype->GetArrayType(uint32_t(arrlit->elements.size()));
  return true;
}

static int ArrayLiteralElementConversionCost(OArrayLit * arrlit, OType * elemtype, uint32_t arraylength, uint32_t aflags)
{
  if (arrlit->elements.size() != arraylength)
  {
    return -1;
  }

  int result = 0;
  for (OExpr * elem : arrlit->elements)
  {
    int cost = g_compiler->GetAssignTypeConversionCost(elemtype, elem, aflags | EXPCF_ALLOW_LAZY_CSTRING);
    if (cost < 0)
    {
      return -1;
    }
    result = max(result, cost);
  }
  return result;
}


bool OTypeArray::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ARRAY != tks)
  {
    return OType::ConvertFromExpr(rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }

  OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
  if (auto * arrlit = dynamic_cast<OArrayLit *>(src))
  {
    return ConvertArrayLiteralElements(arrlit, this->elemtype, this->arraylength, aflags);
  }
  if (this->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias())
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, arrsrc->elemtype->ResolveAlias()->name);
    return false;
  }
  if (this->arraylength != arrsrc->arraylength)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_SIZE_MISM, to_string(this->arraylength), to_string(arrsrc->arraylength));
    return false;
  }

  return true;
}

int OTypeArray::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ARRAY != tks)
  {
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  if (is_explicit_cast) return -1;

  OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
  if (auto * arrlit = dynamic_cast<OArrayLit *>(expr))
  {
    return ArrayLiteralElementConversionCost(arrlit, this->elemtype, this->arraylength, aflags);
  }
  if ((this->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias()) || (this->arraylength != arrsrc->arraylength))
  {
    return -1;
  }

  return 0;
}

bool OTypeArraySlice::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ARRAY_SLICE != tks)
  {
    if (TK_ARRAY == tks)
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
      if (auto * arrlit = dynamic_cast<OArrayLit *>(src))
      {
        bool allow_literal_slice = (aflags & EXPCF_ALLOW_ARRAY_LITERAL_SLICE)
            || (TK_ANYVALUE == this->elemtype->ResolveAlias()->kind);
        if (allow_literal_slice)
        {
          if (!ConvertArrayLiteralElements(arrlit, this->elemtype, uint32_t(arrlit->elements.size()), aflags))
          {
            return false;
          }
          *rexpr = new OArrayLitToSliceExpr(arrlit, this);
          return true;
        }
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
        return false;
      }
      if (this->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias())
      {
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, arrsrc->elemtype->ResolveAlias()->name);
        return false;
      }
      OLValueExpr * lval = dynamic_cast<OLValueExpr *>(src);
      if (!lval)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_SLICE_CONVERSION);
        return false;
      }
      *rexpr = new OArrayToSliceExpr(lval, this);
      return true;
    }
    if (TK_DYN_ARRAY == tks)
    {
      if (is_explicit_cast)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
        return false;
      }
      OTypeDynArray * dynsrc = static_cast<OTypeDynArray *>(resolved_src);
      if (this->elemtype->ResolveAlias() != dynsrc->elemtype->ResolveAlias())
      {
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, dynsrc->elemtype->ResolveAlias()->name);
        return false;
      }
      OLValueExpr * lval = dynamic_cast<OLValueExpr *>(src);
      if (!lval)
      {
        if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_SLICE_CONVERSION);
        return false;
      }
      *rexpr = new ODynArrayToSliceExpr(lval, this);
      return true;
    }
    return OType::ConvertFromExpr(rexpr, aflags);
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }
  OTypeArraySlice * slicesrc = static_cast<OTypeArraySlice *>(resolved_src);
  if (this->elemtype->ResolveAlias() != slicesrc->elemtype->ResolveAlias())
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, slicesrc->elemtype->ResolveAlias()->name);
    return false;
  }
  return true;
}

int OTypeArraySlice::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_ARRAY_SLICE != tks)
  {
    if (TK_ARRAY == tks)
    {
      OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
      if (auto * arrlit = dynamic_cast<OArrayLit *>(expr))
      {
        bool allow_literal_slice = (aflags & EXPCF_ALLOW_ARRAY_LITERAL_SLICE)
            || (TK_ANYVALUE == this->elemtype->ResolveAlias()->kind);
        if (!allow_literal_slice) return -1;
        return ArrayLiteralElementConversionCost(arrlit, this->elemtype, uint32_t(arrlit->elements.size()), aflags);
      }
      if (is_explicit_cast || (this->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias())) return -1;
      if (dynamic_cast<OLValueExpr *>(expr)) return 1;
      return -1;
    }
    if (TK_DYN_ARRAY == tks)
    {
      OTypeDynArray * dynsrc = static_cast<OTypeDynArray *>(resolved_src);
      if (is_explicit_cast || (this->elemtype->ResolveAlias() != dynsrc->elemtype->ResolveAlias())) return -1;
      return (dynamic_cast<OLValueExpr *>(expr) ? 1 : -1);
    }
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  if (is_explicit_cast) return -1;
  OTypeArraySlice * slicesrc = static_cast<OTypeArraySlice *>(resolved_src);
  return ((this->elemtype->ResolveAlias() == slicesrc->elemtype->ResolveAlias()) ? 0 : -1);
}

bool OTypeDynArray::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_DYN_ARRAY != tks)
  {
    if (is_explicit_cast)
    {
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
      return false;
    }
    bool ok = false;
    if (TK_ARRAY == tks)
    {
      OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
      if (auto * arrlit = dynamic_cast<OArrayLit *>(src))
      {
        ok = ConvertArrayLiteralElements(arrlit, this->elemtype, uint32_t(arrlit->elements.size()), aflags);
        if (ok)
        {
          *rexpr = new OArrayLitToDynArrayExpr(arrlit, this);
        }
      }
      else
      {
        ok = (arrsrc->arraylength == 0) || (this->elemtype->ResolveAlias() == arrsrc->elemtype->ResolveAlias());
        if (ok)
        {
          if (auto * lval = dynamic_cast<OLValueExpr *>(src))
          {
            *rexpr = new OArrayToDynArrayExpr(lval, this);
          }
        }
      }
    }
    else if (TK_ARRAY_SLICE == tks)
    {
      ok = (this->elemtype->ResolveAlias() == static_cast<OTypeArraySlice *>(resolved_src)->elemtype->ResolveAlias());
      if (ok)
      {
        *rexpr = new OSliceToDynArrayExpr(src, this);
      }
    }
    if (!ok && (aflags & EXPCF_GENERATE_ERRORS)) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
    return ok;
  }

  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    return false;
  }
  OTypeDynArray * dynsrc = static_cast<OTypeDynArray *>(resolved_src);
  bool ok = (this->elemtype->ResolveAlias() == dynsrc->elemtype->ResolveAlias());
  if (!ok && (aflags & EXPCF_GENERATE_ERRORS)) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
  return ok;
}

int OTypeDynArray::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_DYN_ARRAY != tks)
  {
    if (is_explicit_cast) return -1;
    if (TK_ARRAY == tks)
    {
      OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
      if (auto * arrlit = dynamic_cast<OArrayLit *>(expr))
      {
        return ArrayLiteralElementConversionCost(arrlit, this->elemtype, uint32_t(arrlit->elements.size()), aflags);
      }
      return ((arrsrc->arraylength == 0) || (this->elemtype->ResolveAlias() == arrsrc->elemtype->ResolveAlias())) ? 1 : -1;
    }
    if (TK_ARRAY_SLICE == tks)
    {
      return (this->elemtype->ResolveAlias() == static_cast<OTypeArraySlice *>(resolved_src)->elemtype->ResolveAlias()) ? 1 : -1;
    }
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  if (is_explicit_cast) return -1;
  OTypeDynArray * dynsrc = static_cast<OTypeDynArray *>(resolved_src);
  return ((this->elemtype->ResolveAlias() == dynsrc->elemtype->ResolveAlias()) ? 0 : -1);
}
