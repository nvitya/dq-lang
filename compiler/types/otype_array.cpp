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
#include <llvm/IR/Intrinsics.h>
#include "otype_string.h"
#include "otype_anyvalue.h"
#include "statements.h"

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
  if (scope)
  {
    return scope->GenerateCallOrInvoke(static_cast<LlFuncType *>(fn->ptype->GetLlType()), fn->ll_func, args);
  }
  return ll_builder.CreateCall(fn->ll_func, args);
}

static LlValue * IntExprValue(OScope * scope, OExpr * expr)
{
  return ToNativeUInt(expr->Generate(scope));
}

OType * OTypeDynArray::ElementStorageType() const
{
  if (AsAutoFreeType(elemtype))
  {
    return elemtype;
  }
  OType * etype = elemtype->ResolveAlias();
  if (TK_OBJECT == etype->kind)
  {
    return etype->GetPointerType();
  }
  return etype;
}

static LlValue * GetAutoFreeSourceAddress(OScope * scope, OExpr * value)
{
  auto * source = dynamic_cast<OLValueExpr *>(value);
  if (source && AsAutoFreeType(source->ptype))
  {
    return source->GenerateAddress(scope);
  }
  return nullptr;
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
  if (!elemtype)
  {
    return;
  }
  if (elemtype->RequiresCleanup())
  {
    elemtype->GenerateCleanup(nullptr, elem_addr);
    return;
  }
  if (TK_OBJECT == elemtype->kind)
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
        LlValue * idx = LlConstUInt(i);
        LlValue * ll_zero = LlZero();
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

void OTypeArray::GenerateCleanup(OScope * scope, LlValue * addr)
{
  if (!elemtype || !elemtype->RequiresCleanup() || !addr)
  {
    return;
  }
  for (uint32_t i = 0; i < arraylength; ++i)
  {
    LlValue * elemaddr = ll_builder.CreateInBoundsGEP(GetLlType(), addr, {LlZero(), LlConstUInt(i)}, "autofree.array.elem");
    elemtype->GenerateCleanup(scope, elemaddr);
  }
}

bool OTypeArray::GenerateAssignment(OScope * scope, LlValue * targetaddr, OExpr * value, bool volatile_store)
{
  if (!elemtype || !elemtype->RequiresCleanup())
  {
    return OType::GenerateAssignment(scope, targetaddr, value, volatile_store);
  }

  if (auto * arrlit = dynamic_cast<OArrayLit *>(value); arrlit && arrlit->elements.size() == arraylength)
  {
    for (uint32_t i = 0; i < arraylength; ++i)
    {
      LlValue * targetelem = ll_builder.CreateInBoundsGEP(GetLlType(), targetaddr, {LlZero(), LlConstUInt(i)}, "autofree.array.target");
      if (!elemtype->GenerateAssignment(scope, targetelem, arrlit->elements[i], volatile_store))
      {
        return false;
      }
    }
    return true;
  }

  auto * source = dynamic_cast<OLValueExpr *>(value);
  auto * source_array = dynamic_cast<OTypeArray *>(source && source->ptype ? source->ptype->ResolveAlias() : nullptr);
  if (!source || !source_array || source_array->arraylength != arraylength)
  {
    return OType::GenerateAssignment(scope, targetaddr, value, volatile_store);
  }

  class OLValueArrayElem : public OLValueExpr
  {
  public:
    LlValue * address;
    OLValueArrayElem(LlValue * aaddr, OType * atype) : address(aaddr) { ptype = atype; }
    LlValue * GenerateAddress(OScope *) override { return address; }
  };

  LlValue * sourceaddr = source->GenerateAddress(scope);
  for (uint32_t i = 0; i < arraylength; ++i)
  {
    LlValue * targetelem = ll_builder.CreateInBoundsGEP(GetLlType(), targetaddr, {LlZero(), LlConstUInt(i)}, "autofree.array.target");
    LlValue * sourceelem = ll_builder.CreateInBoundsGEP(GetLlType(), sourceaddr, {LlZero(), LlConstUInt(i)}, "autofree.array.source");
    OLValueArrayElem elem_lvalue(sourceelem, elemtype);
    if (!elemtype->GenerateAssignment(scope, targetelem, &elem_lvalue, volatile_store))
    {
      return false;
    }
  }
  return true;
}

static bool CanGenerateElementCopyRefs(OType * elemtype)
{
  if (TK_DYNSTR == elemtype->kind || TK_DYN_ARRAY == elemtype->kind)
  {
    return true;
  }
  if (TK_ANYVALUE == elemtype->kind)
  {
    return false;
  }
  if (TK_ARRAY == elemtype->kind)
  {
    OTypeArray * arrtype = static_cast<OTypeArray *>(elemtype);
    return CanGenerateElementCopyRefs(arrtype->elemtype);
  }
  if (elemtype->IsCompound())
  {
    OCompoundType * comptype = static_cast<OCompoundType *>(elemtype);
    for (OValSym * member : comptype->member_order)
    {
      if (member && member->ptype && CanGenerateElementCopyRefs(member->GetStorageType()))
      {
        return true;
      }
    }
  }
  return false;
}

static void GenerateElementCopyRefs(OType * elemtype, LlValue * elem_addr)
{
  if (auto * dyntype = dynamic_cast<OTypeDynArray *>(elemtype))
  {
    (void)dyntype;
    CallDynArrayFunc(nullptr, "DynArrIncRef", {elem_addr});
  }
  else if (auto * strtype = dynamic_cast<OTypeDynString *>(elemtype))
  {
    strtype->GenerateIncRef(nullptr, elem_addr);
  }
  else if (TK_ARRAY == elemtype->kind)
  {
    OTypeArray * arrtype = static_cast<OTypeArray *>(elemtype);
    if (CanGenerateElementCopyRefs(arrtype->elemtype))
    {
      for (uint32_t i = 0; i < arrtype->arraylength; ++i)
      {
        LlValue * idx = LlConstUInt(i);
        LlValue * ll_zero = LlZero();
        LlValue * ll_field_addr = ll_builder.CreateGEP(arrtype->GetLlType(), elem_addr, {ll_zero, idx}, "arr.elem.copy.addr");
        GenerateElementCopyRefs(arrtype->elemtype, ll_field_addr);
      }
    }
  }
  else if (elemtype->IsCompound())
  {
    OCompoundType * comptype = static_cast<OCompoundType *>(elemtype);
    comptype->GetLlType();
    for (OValSym * member : comptype->member_order)
    {
      if (member && member->ptype && CanGenerateElementCopyRefs(member->GetStorageType()))
      {
        LlValue * ll_field_addr = ll_builder.CreateStructGEP(comptype->GetLlType(), elem_addr,
            member->ll_field_index, member->name + ".copy.addr");
        GenerateElementCopyRefs(member->GetStorageType(), ll_field_addr);
      }
    }
  }
}

template <typename BodyFn>
static void EmitArrayElementLoop(llvm::Function * func, llvm::Value * dataptr, llvm::Value * count,
                                 OType * elemtype, BodyFn && body_fn)
{
  LlType * uint_type = LlNativeUIntType();
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
  LlValue * ll_cmp = ll_builder.CreateICmpULT(ll_idx, count, "cmp");
  ll_builder.CreateCondBr(ll_cmp, bb_body, bb_end);

  ll_builder.SetInsertPoint(bb_body);
  LlValue * ll_bytesize = llvm::ConstantInt::get(uint_type, elemtype->bytesize);
  LlValue * ll_offset = ll_builder.CreateMul(ll_idx, ll_bytesize, "offset");
  LlValue * ll_elem_addr = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), dataptr, ll_offset, "elem.addr");

  body_fn(ll_elem_addr);

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
}

static llvm::Function * GetTypeDestroyFunc(OType * elemtype)
{
  if (!elemtype->ContainsManagedStorage()) return nullptr;
  if (elemtype->kind == TK_DYN_ARRAY)
  {
    return nullptr; // Handled natively by RTL without code bloat
  }
  if (elemtype->kind == TK_ANYVALUE && !EnsureAnyValueRtlUse())
  {
    return nullptr;
  }

  string func_name = "__dq_destroy_" + SanitizeLlName(elemtype->name);
  if (auto * existing = ll_module->getFunction(func_name))
  {
    return existing;
  }

  llvm::FunctionType * func_type = llvm::FunctionType::get(
      LlType::getVoidTy(ll_ctx), {LlPtrType(), LlNativeUIntType()}, false);
  llvm::Function * func = llvm::Function::Create(
      func_type, llvm::GlobalValue::InternalLinkage, func_name, ll_module);

  auto arg_it = func->arg_begin();
  llvm::Argument * arg_ptr = &*arg_it++;
  arg_ptr->setName("dataptr");
  llvm::Argument * arg_count = &*arg_it++;
  arg_count->setName("count");

  EmitArrayElementLoop(func, arg_ptr, arg_count, elemtype, [&](LlValue * ll_elem_addr) {
    GenerateElementDestructor(elemtype, ll_elem_addr);
  });

  return func;
}

static llvm::Function * GetTypeCopyFunc(OType * elemtype)
{
  if (!CanGenerateElementCopyRefs(elemtype)) return nullptr;

  string func_name = "__dq_copyrefs_" + SanitizeLlName(elemtype->name);
  if (auto * existing = ll_module->getFunction(func_name))
  {
    return existing;
  }

  llvm::FunctionType * func_type = llvm::FunctionType::get(
      LlType::getVoidTy(ll_ctx), {LlPtrType(), LlPtrType(), LlNativeUIntType()}, false);
  llvm::Function * func = llvm::Function::Create(
      func_type, llvm::GlobalValue::InternalLinkage, func_name, ll_module);

  auto arg_it = func->arg_begin();
  llvm::Argument * arg_dst = &*arg_it++;
  arg_dst->setName("dstptr");
  llvm::Argument * arg_src = &*arg_it++;
  arg_src->setName("srcptr");
  llvm::Argument * arg_count = &*arg_it++;
  arg_count->setName("count");
  (void)arg_src;

  EmitArrayElementLoop(func, arg_dst, arg_count, elemtype, [&](LlValue * ll_elem_addr) {
    GenerateElementCopyRefs(elemtype, ll_elem_addr);
  });

  return func;
}

LlValue * OTypeDynArray::GetTypeInfo()
{
  string gvname = "__dq_dynarr_typeinfo_" + SanitizeLlName(elemtype->name);
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
  OType * storage_type = ElementStorageType();
  llvm::Function * ll_destroy_func = GetTypeDestroyFunc(storage_type);
  llvm::Function * ll_copy_func = GetTypeCopyFunc(storage_type);

  vector<LlConst *> values = {
    llvm::ConstantInt::get(i32type, storage_type->bytesize),
    llvm::ConstantInt::get(i16type, 0),
    llvm::ConstantInt::get(i8type, uint8_t(storage_type->kind)),
    llvm::ConstantInt::get(i8type, 0),
    llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)),
    (ll_destroy_func ? static_cast<llvm::Constant *>(ll_destroy_func) : static_cast<llvm::Constant *>(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)))),
    (ll_copy_func ? static_cast<llvm::Constant *>(ll_copy_func) : static_cast<llvm::Constant *>(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)))),
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

  if (auto * aggregate = dynamic_cast<OStructLit *>(expr);
      aggregate && aggregate->entries.empty() && !aggregate->fill_missing)
  {
    if (!aggregate->ptype
        && !g_compiler->ConvertExprToType(ptype, &expr, emit_errors ? EXPCF_GENERATE_ERRORS : 0))
    {
      return false;
    }
    return true;
  }

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

// OTypeArraySlice -- Array descriptor {ptr, target-native uint}

bool OTypeArraySlice::EnsureRuntimeUse()
{
  if (g_namespaces.contains("__dq_rawarray"))
  {
    return true;
  }
  return g_compiler->AddImplicitUse("rtl/rtlint", "__dq_rawarray", nullptr, true, MUM_NONE);
}

LlValue * OTypeArraySlice::CallRuntimeFunc(OScope * scope, const string & name, vector<LlValue *> args)
{
  auto nsit = g_namespaces.find("__dq_rawarray");
  if (nsit == g_namespaces.end() || !nsit->second)
  {
    throw runtime_error("Raw array RTL module is not loaded");
  }
  auto * fn = dynamic_cast<OValSymFunc *>(nsit->second->FindValSym(name, nullptr, false));
  if (!fn || !fn->ll_func)
  {
    throw runtime_error("rtl/rtlint." + name + " function is not available");
  }
  return scope->GenerateCallOrInvoke(
      static_cast<LlFuncType *>(fn->ptype->GetLlType()), fn->ll_func, args);
}

LlValue * OTypeArraySlice::GenerateElementAddress(LlValue * sliceaddr, LlValue * index)
{
  LlValue * ptraddr = ll_builder.CreateStructGEP(GetLlType(), sliceaddr, 0, "slice.ptr.addr");
  LlValue * dataptr = ll_builder.CreateLoad(LlPtrType(), ptraddr, "slice.ptr");
  index = ll_builder.CreateSExtOrTrunc(index, g_builtins->native_int->GetLlType());
  return ll_builder.CreateGEP(elemtype->GetLlType(), dataptr, {index}, "slice.elem");
}

LlType * OTypeArraySlice::CreateLlType()
{
  vector<LlType *> fields = {
    llvm::PointerType::get(ll_ctx, 0),
    LlNativeUIntType()
  };
  return llvm::StructType::get(ll_ctx, fields);
}

LlDiType * OTypeArraySlice::CreateDiType()
{
  // For debug info, represent as a struct with pointer and length fields
  LlDiType * ptr_di = di_builder->createPointerType(elemtype->GetDiType(), TARGET_PTRSIZE * 8);
  unsigned native_bits = TARGET_PTRSIZE * 8;
  LlDiType * len_di = di_builder->createBasicType(
      format("uint{}", native_bits), native_bits, llvm::dwarf::DW_ATE_unsigned);

  llvm::Metadata * elements[] = {
    di_builder->createMemberType(
        nullptr, "ptr", nullptr, 0, TARGET_PTRSIZE * 8, 0,
        0, llvm::DINode::FlagZero, ptr_di),
    di_builder->createMemberType(
        nullptr, "len", nullptr, 0, native_bits, 0,
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

LlValue * OTypeDynArray::GenerateManagerFieldAddress(LlValue * mgr, const string & fieldname)
{
  // Use the RTL parameter type so field offsets follow its actual target layout.
  auto * functype = static_cast<OTypeFunc *>(DynArrayFunc("DynArrGetLength")->ptype);
  auto * mgrtype = dynamic_cast<OTypeObject *>(functype->params[0]->ptype->ResolveAlias());
  if (!mgrtype)
  {
    throw runtime_error("Dynamic array RTL manager type is not available");
  }
  LlType * ll_mgrtype = mgrtype->GetLlType();
  OValSym * field = mgrtype->FindMemberSymbol(fieldname);
  if (!field)
  {
    throw runtime_error("Dynamic array RTL manager field is not available: " + fieldname);
  }
  return ll_builder.CreateStructGEP(ll_mgrtype, mgr, field->ll_field_index, "dyn." + fieldname + ".addr");
}

LlValue * OTypeDynArray::GenerateDataPtr(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  LlValue * descaddr = CreateEntryBlockAlloca(elemtype->GetSliceType()->GetLlType(), nullptr, "dyn.data.desc");
  CallDynArrayFunc(scope, "DynArrGetFullSlice", {dynaddr, descaddr});
  LlValue * desc = ll_builder.CreateLoad(elemtype->GetSliceType()->GetLlType(), descaddr, "dyn.data.slice");
  return ll_builder.CreateExtractValue(desc, {0}, "dyn.ptr");
}

LlValue * OTypeDynArray::GenerateLength(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  return CallDynArrayFunc(scope, "DynArrGetLength", {dynaddr});
}

LlValue * OTypeDynArray::GenerateCapacity(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  return CallDynArrayFunc(scope, "DynArrGetCapacity", {dynaddr});
}

LlValue * OTypeDynArray::GenerateRefCount(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  return CallDynArrayFunc(scope, "DynArrGetRefCount", {dynaddr});
}

LlValue * OTypeDynArray::GenerateElementAddress(OScope * scope, LlValue * dynaddr, LlValue * index)
{
  index = ToNativeInt(index);
  LlFunction * func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * bb_bounds = LlBasicBlock::Create(ll_ctx, "dyn.index.bounds", func);
  LlBasicBlock * bb_element = LlBasicBlock::Create(ll_ctx, "dyn.index.element", func);
  LlBasicBlock * bb_error = LlBasicBlock::Create(ll_ctx, "dyn.index.error", func);
  LlBasicBlock * bb_end = LlBasicBlock::Create(ll_ctx, "dyn.index.end", func);

  LlValue * mgr = GenerateManagerValue(scope, dynaddr);
  ll_builder.CreateCondBr(ll_builder.CreateIsNotNull(mgr), bb_bounds, bb_error);

  ll_builder.SetInsertPoint(bb_bounds);
  LlValue * length = ll_builder.CreateLoad(LlNativeUIntType(), GenerateManagerFieldAddress(mgr, "length"), "dyn.length");
  // Match the RTL's native signed index comparisons, including negative indices.
  LlValue * valid = ll_builder.CreateAnd(ll_builder.CreateICmpSGE(index, LlZero()),
      ll_builder.CreateICmpSLT(index, length));
  ll_builder.CreateCondBr(valid, bb_element, bb_error);

  ll_builder.SetInsertPoint(bb_element);
  LlValue * data = ll_builder.CreateLoad(LlPtrType(), GenerateManagerFieldAddress(mgr, "dataptr"), "dyn.data");
  LlValue * element = ll_builder.CreateGEP(ElementStorageType()->GetLlType(), data, {index}, "dyn.element");
  ll_builder.CreateBr(bb_end);

  ll_builder.SetInsertPoint(bb_error);
  // Retain the runtime's error handling, including handlers that return normally.
  CallDynArrayFunc(scope, "DynArrBoundsError");
  bb_error = ll_builder.GetInsertBlock();
  ll_builder.CreateBr(bb_end);

  ll_builder.SetInsertPoint(bb_end);
  auto * result = ll_builder.CreatePHI(LlPtrType(), 2, "dyn.element.addr");
  result->addIncoming(element, bb_element);
  result->addIncoming(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)), bb_error);
  return result;
}

LlValue * OTypeDynArray::GenerateSlice(OScope * scope, LlValue * dynaddr,
                                       OExpr * start_expr, OExpr * end_expr, OType * slicetype, bool end_inclusive)
{
  LlValue * descaddr = CreateEntryBlockAlloca(slicetype->GetLlType(), nullptr, "dyn.slice.desc");
  if (!start_expr && !end_expr)
  {
    CallDynArrayFunc(scope, "DynArrGetFullSlice", {dynaddr, descaddr});
  }
  else
  {
    LlValue * start = start_expr ? IntExprValue(scope, start_expr) : LlZero();
    LlValue * end = end_expr ? IntExprValue(scope, end_expr) : GenerateLength(scope, dynaddr);
    if (end_inclusive && end_expr)
    {
      end = ll_builder.CreateBinaryIntrinsic(llvm::Intrinsic::sadd_sat, end, LlOne(), nullptr, "dyn.slice.end.inclusive");
    }
    CallDynArrayFunc(scope, "DynArrGetSlice", {dynaddr, descaddr, start, end});
  }
  return ll_builder.CreateLoad(slicetype->GetLlType(), descaddr, "dyn.slice");
}

void OTypeDynArray::GenerateCreate(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  ll_builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)), dynaddr);
}

void OTypeDynArray::GenerateDestroy(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  CallDynArrayFunc(scope, "DynArrDecRef", {dynaddr});
}

LlValue * OTypeDynArray::GenerateManagerValue(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  return ll_builder.CreateLoad(GetLlType(), dynaddr, "dyn.mgr");
}

void OTypeDynArray::GenerateAssignOther(OScope * scope, LlValue * dynaddr, LlValue * srcmgr)
{
  (void)scope;
  CallDynArrayFunc(scope, "DynArrAssignOther", {dynaddr, srcmgr});
}

bool OTypeDynArray::GenerateAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value)
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

  if (TK_DYN_ARRAY == srctype->kind)
  {
    LlValue * srcmgr = value->Generate(scope);
    if (dynamic_cast<OLValueExpr *>(value))
    {
      GenerateAssignOther(scope, targetaddr, srcmgr);
    }
    else
    {
      GenerateDestroy(scope, targetaddr);
      ll_builder.CreateStore(srcmgr, targetaddr);
    }
    return true;
  }

  return false;
}

bool OTypeDynArray::GenerateAssignment(OScope * scope, LlValue * targetaddr, OExpr * value, bool volatile_store)
{
  (void)volatile_store;
  return GenerateAssignExpr(scope, targetaddr, value);
}

void OTypeDynArray::GenerateAssignData(OScope * scope, LlValue * dynaddr, LlValue * srcptr, LlValue * count)
{
  (void)scope;
  CallDynArrayFunc(scope, "DynArrAssignData", {dynaddr, GetTypeInfo(), srcptr, ToNativeUInt(count)});
}

void OTypeDynArray::GenerateClear(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  CallDynArrayFunc(scope, "DynArrClear", {dynaddr, LlBool(false)});
}

void OTypeDynArray::GenerateClear(OScope * scope, LlValue * dynaddr, OExpr * free_storage)
{
  CallDynArrayFunc(scope, "DynArrClear", {dynaddr, free_storage ? free_storage->Generate(scope) : LlBool(false)});
}

void OTypeDynArray::GenerateReserve(OScope * scope, LlValue * dynaddr, OExpr * min_capacity)
{
  CallDynArrayFunc(scope, "DynArrReserve", {dynaddr, GetTypeInfo(), IntExprValue(scope, min_capacity)});
}

void OTypeDynArray::GenerateCompact(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  CallDynArrayFunc(scope, "DynArrCompact", {dynaddr});
}

void OTypeDynArray::GenerateSetLength(OScope * scope, LlValue * dynaddr, OExpr * new_length)
{
  CallDynArrayFunc(scope, "DynArrSetLength", {dynaddr, GetTypeInfo(), IntExprValue(scope, new_length)});
}

void OTypeDynArray::GenerateSetCapacity(OScope * scope, LlValue * dynaddr, OExpr * new_capacity)
{
  CallDynArrayFunc(scope, "DynArrSetCapacity", {dynaddr, GetTypeInfo(), IntExprValue(scope, new_capacity)});
}

void OTypeDynArray::GenerateAppend(OScope * scope, LlValue * dynaddr, OExpr * value)
{
  if (AsAutoFreeType(elemtype))
  {
    if (LlValue * sourceaddr = GetAutoFreeSourceAddress(scope, value))
    {
      CallDynArrayFunc(scope, "DynArrAppendMove", {dynaddr, GetTypeInfo(), sourceaddr});
      return;
    }
  }
  OType * storage_type = ElementStorageType();
  LlValue * tmp = CreateEntryBlockAlloca(storage_type->GetLlType(), nullptr, "dyn.append.value");
  LlValue * elemvalue = value->Generate(scope);
  ll_builder.CreateStore(elemvalue, tmp);

  LlBasicBlock * bb_end = nullptr;
  // Ordinary objects store borrowed pointers; autofree retains its owning type.
  if (!storage_type->ContainsManagedStorage() && !storage_type->RequiresCleanup())
  {
    LlFunction * func = ll_builder.GetInsertBlock()->getParent();
    LlBasicBlock * bb_capacity = LlBasicBlock::Create(ll_ctx, "dyn.append.capacity", func);
    LlBasicBlock * bb_store = LlBasicBlock::Create(ll_ctx, "dyn.append.store", func);
    LlBasicBlock * bb_slow = LlBasicBlock::Create(ll_ctx, "dyn.append.slow", func);
    bb_end = LlBasicBlock::Create(ll_ctx, "dyn.append.end", func);

    // Evaluate the value before loading the manager: it may mutate/rebind the array.
    LlValue * mgr = GenerateManagerValue(scope, dynaddr);
    ll_builder.CreateCondBr(ll_builder.CreateIsNotNull(mgr), bb_capacity, bb_slow);

    ll_builder.SetInsertPoint(bb_capacity);
    LlValue * lenaddr = GenerateManagerFieldAddress(mgr, "length");
    LlValue * length = ll_builder.CreateLoad(LlNativeUIntType(), lenaddr, "dyn.length");
    LlValue * capacity = ll_builder.CreateLoad(LlNativeUIntType(), GenerateManagerFieldAddress(mgr, "capacity"), "dyn.capacity");
    ll_builder.CreateCondBr(ll_builder.CreateICmpULT(length, capacity), bb_store, bb_slow);

    ll_builder.SetInsertPoint(bb_store);
    LlValue * data = ll_builder.CreateLoad(LlPtrType(), GenerateManagerFieldAddress(mgr, "dataptr"), "dyn.data");
    LlValue * dst = ll_builder.CreateGEP(storage_type->GetLlType(), data, {length}, "dyn.append.element");
    ll_builder.CreateStore(elemvalue, dst);
    ll_builder.CreateStore(ll_builder.CreateAdd(length, LlOne()), lenaddr);
    ll_builder.CreateBr(bb_end);

    ll_builder.SetInsertPoint(bb_slow);
  }
  CallDynArrayFunc(scope, "DynArrAppend", {dynaddr, GetTypeInfo(), tmp, LlOne()});
  if (bb_end)
  {
    ll_builder.CreateBr(bb_end);
    ll_builder.SetInsertPoint(bb_end);
  }
}

void OTypeDynArray::GenerateAppendSlice(OScope * scope, LlValue * dynaddr, OExpr * values)
{
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "append.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "append.count");
  CallDynArrayFunc(scope, "DynArrAppend", {dynaddr, GetTypeInfo(), src, count});
}

void OTypeDynArray::GeneratePrepend(OScope * scope, LlValue * dynaddr, OExpr * value)
{
  if (AsAutoFreeType(elemtype))
  {
    if (LlValue * sourceaddr = GetAutoFreeSourceAddress(scope, value))
    {
      CallDynArrayFunc(scope, "DynArrInsertMove", {dynaddr, GetTypeInfo(), LlZero(), sourceaddr});
      return;
    }
  }
  LlValue * tmp = CreateEntryBlockAlloca(ElementStorageType()->GetLlType(), nullptr, "dyn.prepend.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallDynArrayFunc(scope, "DynArrInsert", {dynaddr, GetTypeInfo(), LlZero(), tmp, LlOne()});
}

void OTypeDynArray::GeneratePrependSlice(OScope * scope, LlValue * dynaddr, OExpr * values)
{
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "prepend.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "prepend.count");
  CallDynArrayFunc(scope, "DynArrInsert", {dynaddr, GetTypeInfo(), LlZero(), src, count});
}

void OTypeDynArray::GenerateInsert(OScope * scope, LlValue * dynaddr, OExpr * index, OExpr * value)
{
  LlValue * idx = IntExprValue(scope, index);
  if (AsAutoFreeType(elemtype))
  {
    if (LlValue * sourceaddr = GetAutoFreeSourceAddress(scope, value))
    {
      CallDynArrayFunc(scope, "DynArrInsertMove", {dynaddr, GetTypeInfo(), idx, sourceaddr});
      return;
    }
  }
  LlValue * tmp = CreateEntryBlockAlloca(ElementStorageType()->GetLlType(), nullptr, "dyn.insert.value");
  ll_builder.CreateStore(value->Generate(scope), tmp);
  CallDynArrayFunc(scope, "DynArrInsert", {dynaddr, GetTypeInfo(), idx, tmp, LlOne()});
}

void OTypeDynArray::GenerateInsertSlice(OScope * scope, LlValue * dynaddr, OExpr * index, OExpr * values)
{
  LlValue * idx = IntExprValue(scope, index);
  LlValue * slice = values->Generate(scope);
  LlValue * src = ll_builder.CreateExtractValue(slice, {0}, "insert.src");
  LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "insert.count");
  CallDynArrayFunc(scope, "DynArrInsert", {dynaddr, GetTypeInfo(), idx, src, count});
}

void OTypeDynArray::GenerateDelete(OScope * scope, LlValue * dynaddr, OExpr * index, OExpr * count)
{
  LlValue * idx = IntExprValue(scope, index);
  LlValue * cnt = count ? IntExprValue(scope, count) : LlOne();
  CallDynArrayFunc(scope, "DynArrDelete", {dynaddr, idx, cnt});
}

LlValue * OTypeDynArray::GenerateClone(OScope * scope, LlValue * dynaddr)
{
  (void)scope;
  LlValue * tmp = CreateEntryBlockAlloca(GetLlType(), nullptr, "dyn.clone.tmp");
  ll_builder.CreateStore(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)), tmp);
  LlValue * srcmgr = GenerateManagerValue(scope, dynaddr);
  CallDynArrayFunc(scope, "DynArrClone", {tmp, GetTypeInfo(), srcmgr});
  return ll_builder.CreateLoad(GetLlType(), tmp, "dyn.clone");
}

LlValue * OTypeDynArray::GeneratePop(OScope * scope, LlValue * dynaddr, bool first)
{
  (void)scope;
  LlValue * tmp = CreateEntryBlockAlloca(ElementStorageType()->GetLlType(), nullptr, first ? "dyn.popfirst.tmp" : "dyn.pop.tmp");
  CallDynArrayFunc(scope, first ? "DynArrPopFirst" : "DynArrPop", {dynaddr, tmp});
  return ll_builder.CreateLoad(ElementStorageType()->GetLlType(), tmp, first ? "dyn.popfirst" : "dyn.pop");
}

// Element type of fixed, dynamic and slice array types, nullptr for other types
static OType * ArrayLikeElemType(OType * type)
{
  switch (type->kind)
  {
    case TK_ARRAY:        return static_cast<OTypeArray *>(type)->elemtype;
    case TK_DYN_ARRAY:    return static_cast<OTypeDynArray *>(type)->elemtype;
    case TK_ARRAY_SLICE:  return static_cast<OTypeArraySlice *>(type)->elemtype;
    default:              return nullptr;
  }
}

// Converts (or rates) the elements of an untyped array literal to elemtype; the literal cost is the highest element cost
static int AnalyzeArrayLiteralElements(OArrayLit * arrlit, OType * elemtype, uint32_t arraylength, uint32_t aflags, bool convert)
{
  if (arrlit->elements.size() != arraylength)
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      g_compiler->Error(DQERR_ARR_SIZE_MISM, to_string(arraylength), to_string(arrlit->elements.size()));
    }
    return -1;
  }

  int result = 0;
  for (OExpr *& elem : arrlit->elements)
  {
    int cost = g_compiler->AnalyzeExprConversion(elemtype, &elem, aflags | EXPCF_ALLOW_LAZY_EMBSTR, convert);
    if (cost < 0)
    {
      return -1;
    }
    result = max(result, cost);
  }
  if (convert)
  {
    arrlit->ptype = elemtype->GetArrayType(uint32_t(arrlit->elements.size()));
  }
  return result;
}


int OTypeArray::AnalyzeConversion(OExpr ** rexpr, uint32_t aflags, bool convert)
{
  OExpr * src = *rexpr;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);
  if (auto * arrlit = dynamic_cast<OArrayLit *>(src))
  {
    if (is_explicit_cast)
    {
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_INVALID, arrlit->ptype ? arrlit->ptype->name : "array literal", name);
      return -1;
    }
    return AnalyzeArrayLiteralElements(arrlit, this->elemtype, this->arraylength, aflags, convert);
  }
  OType * resolved_src = src->ResolvedType();

  if (TK_ARRAY != resolved_src->kind)
  {
    return OType::AnalyzeConversion(rexpr, aflags, convert);
  }
  if (is_explicit_cast)
  {
    return ConversionError(resolved_src, aflags);
  }

  OTypeArray * arrsrc = static_cast<OTypeArray *>(resolved_src);
  if (this->elemtype->ResolveAlias() != arrsrc->elemtype->ResolveAlias())
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, arrsrc->elemtype->ResolveAlias()->name);
    return -1;
  }
  if (this->arraylength != arrsrc->arraylength)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_SIZE_MISM, to_string(this->arraylength), to_string(arrsrc->arraylength));
    return -1;
  }

  return 0;
}

int OTypeArraySlice::AnalyzeConversion(OExpr ** rexpr, uint32_t aflags, bool convert)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (aflags & EXPCF_EXPLICIT_CAST)
  {
    return ConversionError(resolved_src, aflags);
  }
  OType * src_elemtype = ArrayLikeElemType(resolved_src);
  if (!src_elemtype)
  {
    return OType::AnalyzeConversion(rexpr, aflags, convert);
  }

  if (auto * arrlit = dynamic_cast<OArrayLit *>(src); arrlit && (TK_ARRAY == tks))
  {
    bool allow_temporary_slice = (aflags & EXPCF_ALLOW_TEMP_ARRAY_SLICE)
        || (TK_ANYVALUE == this->elemtype->ResolveAlias()->kind);
    if (!allow_temporary_slice)
    {
      return ConversionError(resolved_src, aflags);
    }
    int cost = AnalyzeArrayLiteralElements(arrlit, this->elemtype, uint32_t(arrlit->elements.size()), aflags, convert);
    if (convert && (cost >= 0))
    {
      *rexpr = new OTempArrayToSliceExpr(arrlit, this);
    }
    return cost;
  }

  if (this->elemtype->ResolveAlias() != src_elemtype->ResolveAlias())
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_ELEM_TYPE_MISM, this->elemtype->ResolveAlias()->name, src_elemtype->ResolveAlias()->name);
    return -1;
  }
  if (TK_ARRAY_SLICE == tks)
  {
    return 0;
  }

  // Stable arrays are borrowed directly; temporary arrays live through the consuming call.
  OLValueExpr * lval = dynamic_cast<OLValueExpr *>(src);
  if (!lval)
  {
    if (!(aflags & EXPCF_ALLOW_TEMP_ARRAY_SLICE))
    {
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_ARR_SLICE_CONVERSION);
      return -1;
    }
    if (convert)
    {
      *rexpr = new OTempArrayToSliceExpr(src, this);
    }
    return 1;
  }
  if (convert)
  {
    *rexpr = (TK_ARRAY == tks) ? static_cast<OExpr *>(new OArrayToSliceExpr(lval, this))
                               : static_cast<OExpr *>(new ODynArrayToSliceExpr(lval, this));
  }
  return 1;
}

int OTypeDynArray::AnalyzeConversion(OExpr ** rexpr, uint32_t aflags, bool convert)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (aflags & EXPCF_EXPLICIT_CAST)
  {
    return ConversionError(resolved_src, aflags);
  }
  OType * src_elemtype = ArrayLikeElemType(resolved_src);
  if (!src_elemtype)
  {
    return OType::AnalyzeConversion(rexpr, aflags, convert);
  }

  if (auto * arrlit = dynamic_cast<OArrayLit *>(src); arrlit && (TK_ARRAY == tks))
  {
    int cost = AnalyzeArrayLiteralElements(arrlit, this->elemtype, uint32_t(arrlit->elements.size()), aflags, convert);
    if (cost < 0)
    {
      return ConversionError(resolved_src, aflags);
    }
    if (convert)
    {
      *rexpr = new OArrayLitToDynArrayExpr(arrlit, this);
    }
    return cost;
  }

  // A zero length fixed array is accepted regardless of its element type
  bool is_empty_array = (TK_ARRAY == tks) && (0 == static_cast<OTypeArray *>(resolved_src)->arraylength);
  if (!is_empty_array && (this->elemtype->ResolveAlias() != src_elemtype->ResolveAlias()))
  {
    return ConversionError(resolved_src, aflags);
  }
  if (TK_DYN_ARRAY == tks)
  {
    return 0;
  }

  if (convert)
  {
    if (TK_ARRAY_SLICE == tks)
    {
      *rexpr = new OSliceToDynArrayExpr(src, this);
    }
    else if (auto * lval = dynamic_cast<OLValueExpr *>(src))
    {
      *rexpr = new OArrayToDynArrayExpr(lval, this);
    }
  }
  return 1;
}
