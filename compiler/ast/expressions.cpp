/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    expressions.h
 * authors: nvitya
 * created: 2026-02-28
 * brief:   expressions
 */

#include "expressions.h"
#include "scope_builtins.h"
#include "otype_array.h"

/* ctor */ OExprTypeConv::OExprTypeConv(OType * dsttype, OExpr * asrc)
{
  ptype = dsttype;
  src   = asrc;
}

LlValue * OExprTypeConv::Generate(OScope * scope)
{
  return ptype->GenerateConversion(scope, src);
}

/* ctor */ OIntLit::OIntLit(int64_t v)
{
  ptype = g_builtins->type_int;
  value = v;
}

LlValue * OIntLit::Generate(OScope * scope)
{
  return llvm::ConstantInt::get(g_builtins->type_int->GetLlType(), value);
}

/* ctor */ OFloatLit::OFloatLit(double v)
{
  ptype = g_builtins->type_float;
  value = v;
}

LlValue * OFloatLit::Generate(OScope *scope)
{
  return llvm::ConstantInt::get(g_builtins->type_float->GetLlType(), value);
}

/* ctor */ OBoolLit::OBoolLit(bool v)
{
  ptype = g_builtins->type_bool;
  value = v;
}

LlValue * OBoolLit::Generate(OScope * scope)
{
  return llvm::ConstantInt::get(g_builtins->type_bool->GetLlType(), (value ? 1 : 0));
}

/* ctor */ OVarRef::OVarRef(OValSym * avalsym)
{
  ptype = avalsym->ptype;
  pvalsym = avalsym;
}

LlValue * OVarRef::Generate(OScope * scope)
{
  if (!pvalsym->ll_value)
  {
    throw logic_error(std::format("Variable \"{}\" was not prepared in the LLVM", pvalsym->name));
  }

  auto * alloca = dyn_cast<llvm::AllocaInst>(pvalsym->ll_value);
  if (alloca)
  {
    return ll_builder.CreateLoad(alloca->getAllocatedType(), pvalsym->ll_value, pvalsym->name);
  }
  else
  {
    return pvalsym->ll_value;  // function parameter (direct value)
  }
}

/* ctor */ OBinExpr::OBinExpr(EBinOp aop, OExpr * aleft, OExpr * aright)
{
  op    = aop;
  left  = aleft;
  right = aright;

  ptype = aleft->ptype;  // the right shuld be the same or compatible
  if (TK_INT == ptype->kind)
  {
    if (TK_FLOAT == right->ptype->kind)
    {
      ptype = right->ptype;  // change to float
    }
    else if (BINOP_DIV == op)  // the division of two integers results to float
    {
      ptype = g_builtins->type_float;
    }
  }
}

LlValue * OBinExpr::Generate(OScope * scope)
{
  LlValue * ll_left  = left->Generate(scope);
  LlValue * ll_right = right->Generate(scope);

  if (TK_INT == ptype->kind)
  {
    if      (BINOP_ADD == op)   return ll_builder.CreateAdd(ll_left, ll_right);
    else if (BINOP_SUB == op)   return ll_builder.CreateSub(ll_left, ll_right);
    else if (BINOP_MUL == op)   return ll_builder.CreateMul(ll_left, ll_right);
    else if (BINOP_IDIV == op)  return ll_builder.CreateSDiv(ll_left, ll_right);

    else if (BINOP_IOR  == op)  return ll_builder.CreateOr(ll_left, ll_right);
    else if (BINOP_IAND == op)  return ll_builder.CreateAnd(ll_left, ll_right);
    else if (BINOP_IXOR == op)  return ll_builder.CreateXor(ll_left, ll_right);
    else if (BINOP_ISHL == op)  return ll_builder.CreateShl(ll_left, ll_right);
    else if (BINOP_ISHR == op)  return ll_builder.CreateAShr(ll_left, ll_right);

    throw logic_error(std::format("OBinExpr.Generate(): Unhandled int binop = {} ", int(op)));
  }
  else if (TK_FLOAT == ptype->kind)
  {
    if      (BINOP_ADD == op)   return ll_builder.CreateFAdd(ll_left, ll_right);
    else if (BINOP_SUB == op)   return ll_builder.CreateFSub(ll_left, ll_right);
    else if (BINOP_MUL == op)   return ll_builder.CreateFMul(ll_left, ll_right);
    else if (BINOP_DIV == op)   return ll_builder.CreateFDiv(ll_left, ll_right);

    throw logic_error(std::format("OBinExpr.Generate(): Unhandled float binop = {} ", int(op)));
  }
  else
  {
    throw logic_error(std::format("OBinExpr.Generate(): Unhandled type kind = {} ", int(ptype->kind)));
  }
}

/* ctor */ OCompareExpr::OCompareExpr(ECompareOp aop, OExpr * aleft, OExpr * aright)
{
  op    = aop;
  left  = aleft;
  right = aright;

  ptype = g_builtins->type_bool;
}

LlValue * OCompareExpr::Generate(OScope * scope)
{
  LlValue * ll_left  = left->Generate(scope);
  LlValue * ll_right = right->Generate(scope);

  // TODO: handle unsigned !!!

  if      (COMPOP_EQ == op)   return ll_builder.CreateICmpEQ(ll_left, ll_right);
  else if (COMPOP_NE == op)   return ll_builder.CreateICmpNE(ll_left, ll_right);
  else if (COMPOP_LT == op)   return ll_builder.CreateICmpSLT(ll_left, ll_right);
  else if (COMPOP_GT == op)   return ll_builder.CreateICmpSGT(ll_left, ll_right);
  else if (COMPOP_LE == op)   return ll_builder.CreateICmpSLE(ll_left, ll_right);
  else if (COMPOP_GE == op)   return ll_builder.CreateICmpSGE(ll_left, ll_right);

  throw logic_error(std::format("GenerateExpr(): Unhandled compare operation= {} ", int(op)));
}

/* ctor */ OLogicalExpr::OLogicalExpr(ELogicalOp aop, OExpr * aleft, OExpr * aright)
{
  op    = aop;
  left  = aleft;
  right = aright;

  ptype = g_builtins->type_bool;
}

LlValue * OLogicalExpr::Generate(OScope * scope)
{
  LlValue * ll_left  = left->Generate(scope);
  LlValue * ll_right = right->Generate(scope);

  if      (LOGIOP_AND == op)  return ll_builder.CreateAnd(ll_left, ll_right);
  else if (LOGIOP_OR  == op)  return ll_builder.CreateOr(ll_left, ll_right);
  else if (LOGIOP_XOR == op)  return ll_builder.CreateXor(ll_left, ll_right);

  throw logic_error(std::format("GenerateExpr(): Unhandled logical operation= {} ", int(op)));
}

/* ctor */ ONotExpr::ONotExpr(OExpr * expr)
{
  operand = expr;
  ptype = g_builtins->type_bool;
}

LlValue * ONotExpr::Generate(OScope * scope)
{
  LlValue * ll_val = operand->Generate(scope);
  return ll_builder.CreateXor(ll_val, llvm::ConstantInt::get(g_builtins->type_bool->GetLlType(), 1));
}

/* ctor */ OBinNotExpr::OBinNotExpr(OExpr * expr)
{
  operand = expr;
  ptype = operand->ptype;
  if (TK_INT != ptype->kind)
  {
    ptype = g_builtins->type_int;
  }
}

LlValue * OBinNotExpr::Generate(OScope * scope)
{
  LlValue * ll_val = operand->Generate(scope);
  return ll_builder.CreateNot(ll_val);
}

/* ctor */ ONegExpr::ONegExpr(OExpr * expr)
{
  operand = expr;
  ptype = operand->ptype;
}

LlValue * ONegExpr::Generate(OScope * scope)
{
  LlValue * ll_val = operand->Generate(scope);
  return ll_builder.CreateNeg(ll_val);
}

/* ctor */ OAddrOfExpr::OAddrOfExpr(OValSym * avalsym)
{
  pvalsym = avalsym;
  ptype = avalsym->ptype->GetPointerType();
}

LlValue * OAddrOfExpr::Generate(OScope * scope)
{
  if (!pvalsym->ll_value)
  {
    throw logic_error(std::format("OAddrOfExpr: Variable \"{}\" was not prepared in the LLVM", pvalsym->name));
  }
  return pvalsym->ll_value;  // the alloca pointer IS the address
}

/* ctor */ ODerefExpr::ODerefExpr(OExpr * aoperand)
{
  operand = aoperand;
  OTypePointer * ptrtype = static_cast<OTypePointer *>(aoperand->ptype);
  ptype = ptrtype->basetype;
}

LlValue * ODerefExpr::Generate(OScope * scope)
{
  LlValue * ll_ptr = operand->Generate(scope);
  return ll_builder.CreateLoad(ptype->GetLlType(), ll_ptr, "deref");
}

/* ctor */ ONullLit::ONullLit()
{
  ptype = OTypePointer::GetNullPtrType();
}

LlValue * ONullLit::Generate(OScope * scope)
{
  return llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
}

/* ctor */ OArrayIndexExpr::OArrayIndexExpr(OValSym * aarray, OExpr * aindex)
{
  arrayvalsym = aarray;
  indexexpr   = aindex;

  if (TK_ARRAY == aarray->ptype->kind)
  {
    ptype = static_cast<OTypeArray *>(aarray->ptype)->elemtype;
  }
  else // TK_ARRAY_SLICE
  {
    ptype = static_cast<OTypeArraySlice *>(aarray->ptype)->elemtype;
  }
}

LlValue * OArrayIndexExpr::Generate(OScope * scope)
{
  LlValue * ll_index = indexexpr->Generate(scope);

  if (TK_ARRAY == arrayvalsym->ptype->kind)
  {
    // Fixed array: GEP with {0, index} into [N x T]
    LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
    LlValue * ll_gep = ll_builder.CreateGEP(
        arrayvalsym->ptype->GetLlType(),
        arrayvalsym->ll_value,
        {ll_zero, ll_index},
        "arr.elem"
    );
    return ll_builder.CreateLoad(ptype->GetLlType(), ll_gep, "arr.load");
  }
  else // TK_ARRAY_SLICE
  {
    // Slice: use StructGEP to get the pointer field from the alloca, then GEP into the data
    LlType * ll_slicetype = arrayvalsym->ptype->GetLlType();
    LlValue * ll_ptr_addr = ll_builder.CreateStructGEP(ll_slicetype, arrayvalsym->ll_value, 0, "slice.ptr.addr");
    LlValue * ll_ptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_ptr_addr, "slice.ptr");
    LlValue * ll_gep = ll_builder.CreateGEP(ptype->GetLlType(), ll_ptr, {ll_index}, "slice.elem");
    return ll_builder.CreateLoad(ptype->GetLlType(), ll_gep, "slice.load");
  }
}

/* ctor */ OArrayToSliceExpr::OArrayToSliceExpr(OValSym * aarray, OType * slicetype)
{
  arrayvalsym = aarray;
  ptype = slicetype;
}

LlValue * OArrayToSliceExpr::Generate(OScope * scope)
{
  OTypeArray * arrtype = static_cast<OTypeArray *>(arrayvalsym->ptype);

  // Get pointer to first element of the fixed array
  LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
  LlValue * ll_elemptr = ll_builder.CreateGEP(
      arrtype->GetLlType(), arrayvalsym->ll_value, {ll_zero, ll_zero}, "arr.data");

  // Build the slice struct {ptr, i64}
  LlValue * ll_slice = llvm::UndefValue::get(ptype->GetLlType());
  ll_slice = ll_builder.CreateInsertValue(ll_slice, ll_elemptr, 0, "slice.ptr");
  ll_slice = ll_builder.CreateInsertValue(ll_slice,
      llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), arrtype->arraylength),
      1, "slice.len");
  return ll_slice;
}

/* ctor */ OSliceLengthExpr::OSliceLengthExpr(OValSym * aslice)
{
  slicevalsym = aslice;
  ptype = g_builtins->type_int;
}

LlValue * OSliceLengthExpr::Generate(OScope * scope)
{
  // Use StructGEP to access the length field (index 1) of the slice struct
  LlType * ll_slicetype = slicevalsym->ptype->GetLlType();
  LlValue * ll_len_addr = ll_builder.CreateStructGEP(ll_slicetype, slicevalsym->ll_value, 1, "slice.len.addr");
  return ll_builder.CreateLoad(LlType::getInt64Ty(ll_ctx), ll_len_addr, "slice.len");
}

/* ctor */ OCallExpr::OCallExpr(OValSymFunc * avsfunc)
{
  vsfunc = avsfunc;
  ptype = static_cast<OTypeFunc *>(vsfunc->ptype)->rettype;
}

LlValue * OCallExpr::Generate(OScope * scope)
{
  LlFunction * ll_func = vsfunc->ll_func;
  if (!ll_func)
  {
    throw runtime_error("OCallExpr::Generate(): Unknown function: " + vsfunc->name);
  }

  vector<LlValue *>   ll_args;
  for (OExpr * arg : args)
  {
    ll_args.push_back(arg->Generate(scope));
  }
  return ll_builder.CreateCall(ll_func, ll_args);
}

OCallExpr::~OCallExpr()
{
  for (OExpr * arg : args)
  {
    delete arg;
  }
  args.clear();
}
