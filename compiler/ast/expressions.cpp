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
#include "otype_bool.h"
#include "otype_float.h"
#include "otype_int.h"
#include "otype_array.h"
#include "otype_cstring.h"
#include <llvm/IR/Intrinsics.h>

string GetBinopSymbol(EBinOp op)
{
  if (BINOP_ADD   == op)  return "+";
  if (BINOP_SUB   == op)  return "-";
  if (BINOP_MUL   == op)  return "*";
  if (BINOP_DIV   == op)  return "/";
  if (BINOP_IDIV  == op)  return "IDIV";
  if (BINOP_IMOD  == op)  return "IMOD";

  if (BINOP_IAND  == op)  return "AND";
  if (BINOP_IOR   == op)  return "OR";
  if (BINOP_IXOR  == op)  return "XOR";
  if (BINOP_ISHL  == op)  return "<<";
  if (BINOP_ISHR  == op)  return ">>";

  return format("int({})", int(op));
}

string GetCompareSymbol(ECompareOp op)
{
  if (COMPOP_EQ == op)  return "==";
  if (COMPOP_NE == op)  return "<>";
  if (COMPOP_LT == op)  return "<";
  if (COMPOP_LE == op)  return "<=";
  if (COMPOP_GT == op)  return ">";
  if (COMPOP_GE == op)  return ">=";

  return format("int({})", int(op));
}

string GetLogiOpSymbol(ELogicalOp op)
{
  if (LOGIOP_OR  == op)  return "or";
  if (LOGIOP_AND == op)  return "and";
  if (LOGIOP_XOR == op)  return "xor";

  return format("int({})", int(op));
}

string GetRoundModeName(ERoundMode mode)
{
  if (RNDMODE_ROUND == mode)  return "round";
  if (RNDMODE_CEIL  == mode)  return "ceil";
  if (RNDMODE_FLOOR == mode)  return "floor";

  return format("int({})", int(mode));
}

static OExpr * FoldScalarExpr(OExpr * expr);

static bool TryFoldScalarReplacement(OExpr * expr, OExpr ** rreplacement)
{
  OExpr * replacement = FoldScalarExpr(expr);
  if (replacement == expr)
  {
    return false;
  }

  *rreplacement = replacement;
  return true;
}

void OExpr::DeleteTree(OExpr * expr)
{
  if (!expr)
  {
    return;
  }

  expr->DeleteChildTree();
  delete expr;
}

void OExpr::FoldTree(OExpr ** rexpr)
{
  OExpr * expr = (rexpr ? *rexpr : nullptr);
  if (!expr)
  {
    return;
  }

  expr->FoldChildren();

  OExpr * replacement = nullptr;
  if (expr->TryFoldSelf(&replacement))
  {
    *rexpr = replacement;
    delete expr;
  }
}

/* ctor */ OExprTypeConv::OExprTypeConv(OType * dsttype, OExpr * asrc)
{
  ptype = dsttype;
  src   = asrc;
}

LlValue * OExprTypeConv::Generate(OScope * scope)
{
  return ptype->GenerateConversion(scope, src);
}

void OExprTypeConv::FoldChildren()
{
  OExpr::FoldTree(&src);
}

bool OExprTypeConv::TryFoldSelf(OExpr ** rreplacement)
{
  if (!TryFoldScalarReplacement(this, rreplacement))
  {
    return false;
  }

  OExpr::DeleteTree(src);
  src = nullptr;
  return true;
}

void OExprTypeConv::DeleteChildTree()
{
  OExpr::DeleteTree(src);
  src = nullptr;
}

/* ctor */ OIntLit::OIntLit(int64_t v, OType * atype)
{
  ptype = (atype ? atype : g_builtins->type_int);
  value = v;
}

LlValue * OIntLit::Generate(OScope * scope)
{
  return llvm::ConstantInt::get(ptype->GetLlType(), value);
}

/* ctor */ OFloatLit::OFloatLit(double v, OType * atype)
{
  ptype = (atype ? atype : g_builtins->type_float);
  value = v;
}

LlValue * OFloatLit::Generate(OScope *scope)
{
  return llvm::ConstantFP::get(ptype->GetLlType(), value);
}

/* ctor */ OBoolLit::OBoolLit(bool v, OType * atype)
{
  ptype = (atype ? atype : g_builtins->type_bool);
  value = v;
}

LlValue * OBoolLit::Generate(OScope * scope)
{
  return llvm::ConstantInt::get(ptype->GetLlType(), (value ? 1 : 0));
}

// --- LValue expression implementations ---

LlValue * OLValueExpr::Generate(OScope * scope)
{
  LlValue * addr = GenerateAddress(scope);
  return ll_builder.CreateLoad(ptype->GetLlType(), addr, "lval.load");
}

/* ctor */ OLValueVar::OLValueVar(OValSym * avalsym)
{
  ptype = avalsym->ptype;
  pvalsym = avalsym;
}

LlValue * OLValueVar::GenerateAddress(OScope * scope)
{
  (void)scope;

  if (VSK_CONST == pvalsym->kind)
  {
    throw logic_error(std::format("Constant \"{}\" has no addressable storage", pvalsym->name));
  }

  if (!pvalsym->ll_value)
  {
    throw logic_error(std::format("Variable \"{}\" was not prepared in the LLVM", pvalsym->name));
  }

  if (pvalsym->IsRefLike())
  {
    if (auto * alloca = dyn_cast<llvm::AllocaInst>(pvalsym->ll_value))
    {
      return ll_builder.CreateLoad(alloca->getAllocatedType(), pvalsym->ll_value, pvalsym->name + ".addr");
    }

    if (auto * global = dyn_cast<llvm::GlobalVariable>(pvalsym->ll_value))
    {
      return ll_builder.CreateLoad(global->getValueType(), pvalsym->ll_value, pvalsym->name + ".addr");
    }

    if (VSK_PARAMETER == pvalsym->kind)
    {
      return pvalsym->ll_value;
    }

    throw logic_error(std::format("Unhandled reference storage for \"{}\"", pvalsym->name));
  }

  return pvalsym->ll_value;
}

LlValue * OLValueVar::Generate(OScope * scope)
{
  if (VSK_CONST == pvalsym->kind)
  {
    OValSymConst * vsconst = dynamic_cast<OValSymConst *>(pvalsym);
    if (!vsconst)
    {
      throw logic_error(std::format("Constant \"{}\" has invalid value storage", pvalsym->name));
    }
    return vsconst->pvalue->GetLlConst();
  }

  if (!pvalsym->ll_value)
  {
    throw logic_error(std::format("Variable \"{}\" was not prepared in the LLVM", pvalsym->name));
  }

  if (pvalsym->IsRefLike())
  {
    LlValue * addr = GenerateAddress(scope);
    return ll_builder.CreateLoad(ptype->GetLlType(), addr, pvalsym->name);
  }

  auto * alloca = dyn_cast<llvm::AllocaInst>(pvalsym->ll_value);
  if (alloca)
  {
    return ll_builder.CreateLoad(alloca->getAllocatedType(), pvalsym->ll_value, pvalsym->name);
  }

  auto * global = dyn_cast<llvm::GlobalVariable>(pvalsym->ll_value);
  if (global)
  {
    return ll_builder.CreateLoad(global->getValueType(), pvalsym->ll_value, pvalsym->name);
  }

  if (VSK_PARAMETER == pvalsym->kind)
  {
    return pvalsym->ll_value;  // function parameter (direct value)
  }

  throw logic_error(std::format("Unhandled variable storage for \"{}\"", pvalsym->name));
}

/* ctor */ OLValueDeref::OLValueDeref(OExpr * aptr)
{
  ptrexpr = aptr;
  OTypePointer * pt = static_cast<OTypePointer *>(aptr->ptype);
  ptype = pt->basetype;
}

LlValue * OLValueDeref::GenerateAddress(OScope * scope)
{
  return ptrexpr->Generate(scope);  // the pointer value IS the address
}

void OLValueDeref::FoldChildren()
{
  OExpr::FoldTree(&ptrexpr);
}

void OLValueDeref::DeleteChildTree()
{
  OExpr::DeleteTree(ptrexpr);
  ptrexpr = nullptr;
}

/* ctor */ OLValueMember::OLValueMember(OLValueExpr * abase, OType * astype, uint32_t aidx, OType * amembertype)
{
  base        = abase;
  structtype  = astype;
  memberindex = aidx;
  ptype       = amembertype;
}

LlValue * OLValueMember::GenerateAddress(OScope * scope)
{
  LlValue * baseaddr = base->GenerateAddress(scope);
  return ll_builder.CreateStructGEP(structtype->GetLlType(), baseaddr, memberindex, "member.addr");
}

void OLValueMember::FoldChildren()
{
  OExpr * tmp = base;
  OExpr::FoldTree(&tmp);
  base = static_cast<OLValueExpr *>(tmp);
}

void OLValueMember::DeleteChildTree()
{
  OExpr::DeleteTree(base);
  base = nullptr;
}

/* ctor */ OLValueIndex::OLValueIndex(OLValueExpr * abase, OType * acontainertype, OExpr * aindex)
{
  base          = abase;
  containertype = acontainertype;
  indexexpr     = aindex;

  if (TK_ARRAY == acontainertype->kind)
  {
    ptype = static_cast<OTypeArray *>(acontainertype)->elemtype;
  }
  else if (TK_ARRAY_SLICE == acontainertype->kind)
  {
    ptype = static_cast<OTypeArraySlice *>(acontainertype)->elemtype;
  }
  else if (TK_STRING == acontainertype->kind)
  {
    ptype = g_builtins->type_cchar;
  }
}

LlValue * OLValueIndex::GenerateAddress(OScope * scope)
{
  LlValue * ll_index = indexexpr->Generate(scope);

  if (TK_ARRAY == containertype->kind)
  {
    // Fixed array: GEP with {0, index} into [N x T]
    LlValue * baseaddr = base->GenerateAddress(scope);
    LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
    return ll_builder.CreateGEP(
        containertype->GetLlType(), baseaddr,
        {ll_zero, ll_index}, "arr.elem");
  }
  else if (TK_ARRAY_SLICE == containertype->kind)
  {
    // Slice: extract pointer from descriptor, then GEP into the data
    LlValue * baseaddr = base->GenerateAddress(scope);
    LlType * ll_slicetype = containertype->GetLlType();
    LlValue * ll_ptr_addr = ll_builder.CreateStructGEP(ll_slicetype, baseaddr, 0, "slice.ptr.addr");
    LlValue * ll_ptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_ptr_addr, "slice.ptr");
    return ll_builder.CreateGEP(ptype->GetLlType(), ll_ptr, {ll_index}, "slice.elem");
  }
  else if (TK_STRING == containertype->kind)
  {
    // CString indexing
    OTypeCString * cstrtype = static_cast<OTypeCString *>(containertype);
    if (cstrtype->maxlen > 0)
    {
      // Sized cstring[N]: GEP into [N x i8] with {0, index}
      LlValue * baseaddr = base->GenerateAddress(scope);
      LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
      return ll_builder.CreateGEP(
          cstrtype->GetLlType(), baseaddr,
          {ll_zero, ll_index}, "cstr.elem");
    }
    else
    {
      // Unsized cstring param: extract ptr from descriptor, then GEP
      LlValue * baseaddr = base->GenerateAddress(scope);
      LlType * ll_desctype = cstrtype->GetLlType();
      LlValue * ll_ptr_addr = ll_builder.CreateStructGEP(ll_desctype, baseaddr, 0, "cstr.ptr.addr");
      LlValue * ll_ptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_ptr_addr, "cstr.ptr");
      return ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_ptr, {ll_index}, "cstr.elem");
    }
  }

  throw logic_error("OLValueIndex::GenerateAddress: unsupported container type");
}

void OLValueIndex::FoldChildren()
{
  OExpr * tmp = base;
  OExpr::FoldTree(&tmp);
  base = static_cast<OLValueExpr *>(tmp);
  OExpr::FoldTree(&indexexpr);
}

void OLValueIndex::DeleteChildTree()
{
  OExpr::DeleteTree(base);
  OExpr::DeleteTree(indexexpr);
  base = nullptr;
  indexexpr = nullptr;
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
  // pointer arithmetic: ptr +/- int → result is pointer
  // ptype is already set to left->ptype which is the pointer type
}

LlValue * OBinExpr::Generate(OScope * scope)
{
  LlValue * ll_left  = left->Generate(scope);
  LlValue * ll_right = right->Generate(scope);

  if (TK_POINTER == ptype->kind)
  {
    // Pointer arithmetic: ptr + int or ptr - int
    OTypePointer * ptrtype = static_cast<OTypePointer *>(ptype);
    LlType * ll_elemtype = ptrtype->basetype->GetLlType();
    if (BINOP_ADD == op)
      return ll_builder.CreateGEP(ll_elemtype, ll_left, {ll_right}, "ptr.add");
    else if (BINOP_SUB == op)
      return ll_builder.CreateGEP(ll_elemtype, ll_left, {ll_builder.CreateNeg(ll_right)}, "ptr.sub");

    throw logic_error(std::format("OBinExpr.Generate(): Unhandled pointer binop = {} ", int(op)));
  }

  if (TK_INT == ptype->kind)
  {
    bool issigned = static_cast<OTypeInt *>(ResolvedType())->issigned;

    if      (BINOP_ADD == op)   return ll_builder.CreateAdd(ll_left, ll_right);
    else if (BINOP_SUB == op)   return ll_builder.CreateSub(ll_left, ll_right);
    else if (BINOP_MUL == op)   return ll_builder.CreateMul(ll_left, ll_right);
    else if (BINOP_IDIV == op)  return ( issigned ? ll_builder.CreateSDiv(ll_left, ll_right)
                                                  : ll_builder.CreateUDiv(ll_left, ll_right) );
    else if (BINOP_IMOD == op)  return ( issigned ? ll_builder.CreateSRem(ll_left, ll_right)
                                                  : ll_builder.CreateURem(ll_left, ll_right) );
    else if (BINOP_IOR  == op)  return ll_builder.CreateOr(ll_left, ll_right);
    else if (BINOP_IAND == op)  return ll_builder.CreateAnd(ll_left, ll_right);
    else if (BINOP_IXOR == op)  return ll_builder.CreateXor(ll_left, ll_right);
    else if (BINOP_ISHL == op)  return ll_builder.CreateShl(ll_left, ll_right);
    else if (BINOP_ISHR == op)  return ( issigned ? ll_builder.CreateAShr(ll_left, ll_right)
                                                  : ll_builder.CreateLShr(ll_left, ll_right) );

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

void OBinExpr::FoldChildren()
{
  OExpr::FoldTree(&left);
  OExpr::FoldTree(&right);
}

bool OBinExpr::TryFoldSelf(OExpr ** rreplacement)
{
  if (!TryFoldScalarReplacement(this, rreplacement))
  {
    return false;
  }

  OExpr::DeleteTree(left);
  OExpr::DeleteTree(right);
  left = nullptr;
  right = nullptr;
  return true;
}

void OBinExpr::DeleteChildTree()
{
  OExpr::DeleteTree(left);
  OExpr::DeleteTree(right);
  left = nullptr;
  right = nullptr;
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

  OType * optype = left->ptype;

  if (TK_FLOAT == optype->kind)
  {
    if      (COMPOP_EQ == op)   return ll_builder.CreateFCmpOEQ(ll_left, ll_right);
    else if (COMPOP_NE == op)   return ll_builder.CreateFCmpONE(ll_left, ll_right);
    else if (COMPOP_LT == op)   return ll_builder.CreateFCmpOLT(ll_left, ll_right);
    else if (COMPOP_GT == op)   return ll_builder.CreateFCmpOGT(ll_left, ll_right);
    else if (COMPOP_LE == op)   return ll_builder.CreateFCmpOLE(ll_left, ll_right);
    else if (COMPOP_GE == op)   return ll_builder.CreateFCmpOGE(ll_left, ll_right);
  }
  else if (TK_INT == optype->kind)
  {
    bool issigned = static_cast<OTypeInt *>(left->ResolvedType())->issigned;

    if      (COMPOP_EQ == op)   return ll_builder.CreateICmpEQ(ll_left, ll_right);
    else if (COMPOP_NE == op)   return ll_builder.CreateICmpNE(ll_left, ll_right);
    else if (issigned)
    {
      if      (COMPOP_LT == op)   return ll_builder.CreateICmpSLT(ll_left, ll_right);
      else if (COMPOP_GT == op)   return ll_builder.CreateICmpSGT(ll_left, ll_right);
      else if (COMPOP_LE == op)   return ll_builder.CreateICmpSLE(ll_left, ll_right);
      else if (COMPOP_GE == op)   return ll_builder.CreateICmpSGE(ll_left, ll_right);
    }
    else
    {
      if      (COMPOP_LT == op)   return ll_builder.CreateICmpULT(ll_left, ll_right);
      else if (COMPOP_GT == op)   return ll_builder.CreateICmpUGT(ll_left, ll_right);
      else if (COMPOP_LE == op)   return ll_builder.CreateICmpULE(ll_left, ll_right);
      else if (COMPOP_GE == op)   return ll_builder.CreateICmpUGE(ll_left, ll_right);
    }
  }
  else if (TK_POINTER == optype->kind)
  {
    // Pointer comparisons (unsigned — comparing addresses)
    if      (COMPOP_EQ == op)   return ll_builder.CreateICmpEQ(ll_left, ll_right);
    else if (COMPOP_NE == op)   return ll_builder.CreateICmpNE(ll_left, ll_right);
    else if (COMPOP_LT == op)   return ll_builder.CreateICmpULT(ll_left, ll_right);
    else if (COMPOP_GT == op)   return ll_builder.CreateICmpUGT(ll_left, ll_right);
    else if (COMPOP_LE == op)   return ll_builder.CreateICmpULE(ll_left, ll_right);
    else if (COMPOP_GE == op)   return ll_builder.CreateICmpUGE(ll_left, ll_right);
  }

  throw logic_error(std::format("GenerateExpr(): Unhandled compare operation= {} ", int(op)));
}

void OCompareExpr::FoldChildren()
{
  OExpr::FoldTree(&left);
  OExpr::FoldTree(&right);
}

bool OCompareExpr::TryFoldSelf(OExpr ** rreplacement)
{
  if (!TryFoldScalarReplacement(this, rreplacement))
  {
    return false;
  }

  OExpr::DeleteTree(left);
  OExpr::DeleteTree(right);
  left = nullptr;
  right = nullptr;
  return true;
}

void OCompareExpr::DeleteChildTree()
{
  OExpr::DeleteTree(left);
  OExpr::DeleteTree(right);
  left = nullptr;
  right = nullptr;
}

/* ctor */ OIifExpr::OIifExpr(OExpr * acond, OExpr * atrue, OExpr * afalse, OType * aresult_type)
{
  condition  = acond;
  true_expr  = atrue;
  false_expr = afalse;
  ptype      = aresult_type;
}

OIifExpr::~OIifExpr()
{
  delete condition;
  delete true_expr;
  delete false_expr;
}

LlValue * OIifExpr::Generate(OScope * scope)
{
  LlValue * ll_cond = condition->Generate(scope);
  if (ll_cond->getType() != g_builtins->type_bool->GetLlType())
  {
    throw logic_error("OIifExpr::Generate(): condition must be bool");
  }

  LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * cond_bb  = ll_builder.GetInsertBlock();
  LlBasicBlock * true_bb  = LlBasicBlock::Create(ll_ctx, "iif.true", ll_func);
  LlBasicBlock * false_bb = LlBasicBlock::Create(ll_ctx, "iif.false", ll_func);
  LlBasicBlock * merge_bb = LlBasicBlock::Create(ll_ctx, "iif.end", ll_func);

  ll_builder.CreateCondBr(ll_cond, true_bb, false_bb);

  ll_builder.SetInsertPoint(true_bb);
  LlValue * ll_true = true_expr->Generate(scope);
  LlBasicBlock * true_end_bb = ll_builder.GetInsertBlock();
  ll_builder.CreateBr(merge_bb);

  ll_builder.SetInsertPoint(false_bb);
  LlValue * ll_false = false_expr->Generate(scope);
  LlBasicBlock * false_end_bb = ll_builder.GetInsertBlock();
  ll_builder.CreateBr(merge_bb);

  ll_builder.SetInsertPoint(merge_bb);
  llvm::PHINode * ll_result = ll_builder.CreatePHI(ptype->GetLlType(), 2, "iif.result");
  ll_result->addIncoming(ll_true, true_end_bb);
  ll_result->addIncoming(ll_false, false_end_bb);
  return ll_result;
}

void OIifExpr::FoldChildren()
{
  OExpr::FoldTree(&condition);
  if (dynamic_cast<OBoolLit *>(condition))
  {
    return;
  }

  OExpr::FoldTree(&true_expr);
  OExpr::FoldTree(&false_expr);
}

bool OIifExpr::TryFoldSelf(OExpr ** rreplacement)
{
  auto * cond = dynamic_cast<OBoolLit *>(condition);
  if (!cond)
  {
    return false;
  }

  bool select_true = cond->value;
  OExpr * selected = (select_true ? true_expr : false_expr);
  OExpr * discarded = (select_true ? false_expr : true_expr);

  condition = nullptr;
  true_expr = nullptr;
  false_expr = nullptr;

  OExpr::DeleteTree(cond);
  OExpr::DeleteTree(discarded);
  OExpr::FoldTree(&selected);
  *rreplacement = selected;
  return true;
}

void OIifExpr::DeleteChildTree()
{
  OExpr::DeleteTree(condition);
  OExpr::DeleteTree(true_expr);
  OExpr::DeleteTree(false_expr);
  condition = nullptr;
  true_expr = nullptr;
  false_expr = nullptr;
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

void OLogicalExpr::FoldChildren()
{
  OExpr::FoldTree(&left);
  OExpr::FoldTree(&right);
}

bool OLogicalExpr::TryFoldSelf(OExpr ** rreplacement)
{
  if (!TryFoldScalarReplacement(this, rreplacement))
  {
    return false;
  }

  OExpr::DeleteTree(left);
  OExpr::DeleteTree(right);
  left = nullptr;
  right = nullptr;
  return true;
}

void OLogicalExpr::DeleteChildTree()
{
  OExpr::DeleteTree(left);
  OExpr::DeleteTree(right);
  left = nullptr;
  right = nullptr;
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

void ONotExpr::FoldChildren()
{
  OExpr::FoldTree(&operand);
}

bool ONotExpr::TryFoldSelf(OExpr ** rreplacement)
{
  if (!TryFoldScalarReplacement(this, rreplacement))
  {
    return false;
  }

  OExpr::DeleteTree(operand);
  operand = nullptr;
  return true;
}

void ONotExpr::DeleteChildTree()
{
  OExpr::DeleteTree(operand);
  operand = nullptr;
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

void OBinNotExpr::FoldChildren()
{
  OExpr::FoldTree(&operand);
}

bool OBinNotExpr::TryFoldSelf(OExpr ** rreplacement)
{
  if (!TryFoldScalarReplacement(this, rreplacement))
  {
    return false;
  }

  OExpr::DeleteTree(operand);
  operand = nullptr;
  return true;
}

void OBinNotExpr::DeleteChildTree()
{
  OExpr::DeleteTree(operand);
  operand = nullptr;
}

/* ctor */ ONegExpr::ONegExpr(OExpr * expr)
{
  operand = expr;
  ptype = operand->ptype;
}

LlValue * ONegExpr::Generate(OScope * scope)
{
  LlValue * ll_val = operand->Generate(scope);
  if (ll_val->getType()->isFloatingPointTy())
  {
    return ll_builder.CreateFNeg(ll_val);
  }

  return ll_builder.CreateNeg(ll_val);
}

void ONegExpr::FoldChildren()
{
  OExpr::FoldTree(&operand);
}

bool ONegExpr::TryFoldSelf(OExpr ** rreplacement)
{
  if (!TryFoldScalarReplacement(this, rreplacement))
  {
    return false;
  }

  OExpr::DeleteTree(operand);
  operand = nullptr;
  return true;
}

void ONegExpr::DeleteChildTree()
{
  OExpr::DeleteTree(operand);
  operand = nullptr;
}

/* ctor */ OAddrOfExpr::OAddrOfExpr(OLValueExpr * atarget)
{
  target = atarget;
  ptype = atarget->ptype->GetPointerType();
}

LlValue * OAddrOfExpr::Generate(OScope * scope)
{
  return target->GenerateAddress(scope);
}

void OAddrOfExpr::FoldChildren()
{
  OExpr * tmp = target;
  OExpr::FoldTree(&tmp);
  target = static_cast<OLValueExpr *>(tmp);
}

void OAddrOfExpr::DeleteChildTree()
{
  OExpr::DeleteTree(target);
  target = nullptr;
}

/* ctor */ ONullLit::ONullLit()
{
  ptype = OTypePointer::GetNullPtrType();
}

LlValue * ONullLit::Generate(OScope * scope)
{
  return llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
}

/* ctor */ OPointerIndexExpr::OPointerIndexExpr(OExpr * aptr, OExpr * aindex)
{
  ptrexpr   = aptr;
  indexexpr = aindex;
  ptype     = aptr->ptype;  // same pointer type: result is a pointer to element i
}

LlValue * OPointerIndexExpr::Generate(OScope * scope)
{
  LlValue *      ll_ptr   = ptrexpr->Generate(scope);
  LlValue *      ll_index = indexexpr->Generate(scope);
  OTypePointer * ptrtype  = static_cast<OTypePointer *>(ptype);
  LlType *       ll_elem  = ptrtype->basetype->GetLlType();
  return ll_builder.CreateGEP(ll_elem, ll_ptr, {ll_index}, "ptr.idx");
}

void OPointerIndexExpr::FoldChildren()
{
  OExpr::FoldTree(&ptrexpr);
  OExpr::FoldTree(&indexexpr);
}

void OPointerIndexExpr::DeleteChildTree()
{
  OExpr::DeleteTree(ptrexpr);
  OExpr::DeleteTree(indexexpr);
  ptrexpr = nullptr;
  indexexpr = nullptr;
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

/* ctor */ OFloatRoundExpr::OFloatRoundExpr(ERoundMode amode, OExpr * asrc)
{
  mode  = amode;
  src   = asrc;
  ptype = g_builtins->type_int;
}

OFloatRoundExpr::~OFloatRoundExpr()
{
  delete src;
}

LlValue * OFloatRoundExpr::Generate(OScope * scope)
{
  LlValue * ll_src = src->Generate(scope);
  llvm::Intrinsic::ID iid;
  if      (RNDMODE_ROUND == mode)  iid = llvm::Intrinsic::round;
  else if (RNDMODE_CEIL  == mode)  iid = llvm::Intrinsic::ceil;
  else                             iid = llvm::Intrinsic::floor;
  LlValue * ll_rounded = ll_builder.CreateUnaryIntrinsic(iid, ll_src);
  return ll_builder.CreateFPToSI(ll_rounded, g_builtins->type_int->GetLlType());
}

void OFloatRoundExpr::FoldChildren()
{
  OExpr::FoldTree(&src);
}

bool OFloatRoundExpr::TryFoldSelf(OExpr ** rreplacement)
{
  if (!TryFoldScalarReplacement(this, rreplacement))
  {
    return false;
  }

  OExpr::DeleteTree(src);
  src = nullptr;
  return true;
}

void OFloatRoundExpr::DeleteChildTree()
{
  OExpr::DeleteTree(src);
  src = nullptr;
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

  OTypeFunc * tfunc = static_cast<OTypeFunc *>(vsfunc->ptype);
  vector<LlValue *>   ll_args;
  for (size_t i = 0; i < args.size(); ++i)
  {
    LlValue * val = args[i]->Generate(scope);

    // C varargs default argument promotions for extra arguments
    if (tfunc->has_varargs && i >= tfunc->params.size())
    {
      LlType * valtype = val->getType();
      if (valtype->isFloatTy())
      {
        // float32 -> double promotion
        val = ll_builder.CreateFPExt(val, llvm::Type::getDoubleTy(ll_ctx));
      }
      else if (valtype->isIntegerTy() && valtype->getIntegerBitWidth() < 32)
      {
        // small integers use the C default argument promotions
        OTypeInt * inttype = dynamic_cast<OTypeInt *>(args[i]->ResolvedType());
        if (inttype && inttype->issigned)
        {
          val = ll_builder.CreateSExt(val, llvm::Type::getInt32Ty(ll_ctx));
        }
        else
        {
          val = ll_builder.CreateZExt(val, llvm::Type::getInt32Ty(ll_ctx));
        }
      }
    }

    ll_args.push_back(val);
  }
  return ll_builder.CreateCall(ll_func, ll_args);
}

void OCallExpr::FoldChildren()
{
  for (OExpr *& arg : args)
  {
    OExpr::FoldTree(&arg);
  }
}

void OCallExpr::DeleteChildTree()
{
  for (OExpr *& arg : args)
  {
    OExpr::DeleteTree(arg);
    arg = nullptr;
  }
  args.clear();
}

OCallExpr::~OCallExpr()
{
  for (OExpr * arg : args)
  {
    delete arg;
  }
  args.clear();
}

/* ctor */ OArrayLit::OArrayLit(const vector<OExpr *> & aelements)
{
  elements = aelements;

  if (elements.empty())
  {
    // Default to int[0] or handle error? For now assuming int.
    ptype = g_builtins->type_int->GetArrayType(0);
  }
  else
  {
    // Infer type from the first element
    // TODO: Check if all elements are compatible
    OType * elemtype = elements[0]->ptype;
    ptype = elemtype->GetArrayType(elements.size());
  }
}

LlValue * OArrayLit::Generate(OScope * scope)
{
  // Create an undefined array value and insert elements one by one
  LlValue * ll_arr = llvm::UndefValue::get(ptype->GetLlType());

  for (size_t i = 0; i < elements.size(); ++i)
  {
    LlValue * ll_val = elements[i]->Generate(scope);
    ll_arr = ll_builder.CreateInsertValue(ll_arr, ll_val, {(unsigned)i});
  }

  return ll_arr;
}

void OArrayLit::FoldChildren()
{
  for (OExpr *& elem : elements)
  {
    OExpr::FoldTree(&elem);
  }
}

void OArrayLit::DeleteChildTree()
{
  for (OExpr *& elem : elements)
  {
    OExpr::DeleteTree(elem);
    elem = nullptr;
  }
  elements.clear();
}

// --- cstring expressions ---

/* ctor */ OCStringLit::OCStringLit(const string & avalue)
{
  value = avalue;
  ptype = g_builtins->type_cchar->GetPointerType();  // ^cchar
}

LlValue * OCStringLit::Generate(OScope * scope)
{
  return ll_builder.CreateGlobalString(value, ".str");
}

/* ctor */ OCStringSizeExpr::OCStringSizeExpr(OValSym * avs)
{
  cstrvalsym = avs;
  ptype = g_builtins->type_int;
}

LlValue * OCStringSizeExpr::Generate(OScope * scope)
{
  // Extract size field (index 1) from the cstring descriptor {ptr, i64}
  LlType * ll_desctype = cstrvalsym->ptype->GetLlType();
  LlValue * ll_size_addr = ll_builder.CreateStructGEP(ll_desctype, cstrvalsym->ll_value, 1, "cstr.size.addr");
  return ll_builder.CreateLoad(LlType::getInt64Ty(ll_ctx), ll_size_addr, "cstr.size");
}

/* ctor */ OCStringLenExpr::OCStringLenExpr(OValSym * avs)
{
  cstrvalsym = avs;
  ptype = g_builtins->type_int;
}

#define USE_INLINE_STRLEN 0

#if USE_INLINE_STRLEN  // inline version

LlValue * OCStringLenExpr::Generate(OScope * scope)
{
  LlValue * ll_charptr;
  OTypeCString * cstrtype = static_cast<OTypeCString *>(cstrvalsym->ptype);

  if (cstrtype->maxlen > 0)
  {
    // Fixed cstring[N]: GEP to element 0
    LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
    ll_charptr = ll_builder.CreateGEP(cstrtype->GetLlType(), cstrvalsym->ll_value,
        {ll_zero, ll_zero}, "cstr.ptr");
  }
  else
  {
    // Unsized cstring param: extract pointer from descriptor
    LlType * ll_desctype = cstrtype->GetLlType();
    LlValue * ll_ptr_addr = ll_builder.CreateStructGEP(ll_desctype, cstrvalsym->ll_value, 0, "cstr.ptr.addr");
    ll_charptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_ptr_addr, "cstr.ptr");
  }

  // Inline strlen loop
  LlFunction * func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * entry_bb = ll_builder.GetInsertBlock();
  LlBasicBlock * scan_bb = LlBasicBlock::Create(ll_ctx, "strlen.scan", func);
  LlBasicBlock * done_bb = LlBasicBlock::Create(ll_ctx, "strlen.done", func);

  LlType * ll_i64 = LlType::getInt64Ty(ll_ctx);
  LlType * ll_i8  = LlType::getInt8Ty(ll_ctx);

  ll_builder.CreateBr(scan_bb);
  ll_builder.SetInsertPoint(scan_bb);

  llvm::PHINode * ll_i = ll_builder.CreatePHI(ll_i64, 2, "strlen.i");
  ll_i->addIncoming(llvm::ConstantInt::get(ll_i64, 0), entry_bb);

  LlValue * ll_ch_ptr = ll_builder.CreateGEP(ll_i8, ll_charptr, {ll_i}, "strlen.ch.ptr");
  LlValue * ll_ch = ll_builder.CreateLoad(ll_i8, ll_ch_ptr, "strlen.ch");
  LlValue * ll_is_null = ll_builder.CreateICmpEQ(ll_ch, llvm::ConstantInt::get(ll_i8, 0), "strlen.is_null");
  LlValue * ll_i_next = ll_builder.CreateAdd(ll_i, llvm::ConstantInt::get(ll_i64, 1), "strlen.i.next");

  ll_i->addIncoming(ll_i_next, scan_bb);
  ll_builder.CreateCondBr(ll_is_null, done_bb, scan_bb);

  ll_builder.SetInsertPoint(done_bb);
  return ll_i;
}

#else  // call the strnlen from libc

LlValue * OCStringLenExpr::Generate(OScope * scope)
{
  LlValue * ll_charptr;
  LlValue * ll_maxlen;
  OTypeCString * cstrtype = static_cast<OTypeCString *>(cstrvalsym->ptype);

  if (cstrtype->maxlen > 0)
  {
    // Fixed cstring[N]: GEP to element 0
    LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
    ll_charptr = ll_builder.CreateGEP(cstrtype->GetLlType(), cstrvalsym->ll_value,
        {ll_zero, ll_zero}, "cstr.ptr");
    ll_maxlen = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), cstrtype->maxlen);
  }
  else
  {
    // Unsized cstring param: extract pointer from descriptor
    LlType * ll_desctype = cstrtype->GetLlType();
    LlValue * ll_ptr_addr = ll_builder.CreateStructGEP(ll_desctype, cstrvalsym->ll_value, 0, "cstr.ptr.addr");
    ll_charptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_ptr_addr, "cstr.ptr");

    // Extract maxlen from descriptor
    LlValue * ll_maxlen_addr = ll_builder.CreateStructGEP(ll_desctype, cstrvalsym->ll_value, 1, "cstr.maxlen.addr");
    ll_maxlen = ll_builder.CreateLoad(LlType::getInt64Ty(ll_ctx), ll_maxlen_addr, "cstr.maxlen");
  }

  // Call strnlen from libc
  llvm::FunctionType * ft = llvm::FunctionType::get(LlType::getInt64Ty(ll_ctx), {llvm::PointerType::get(ll_ctx, 0), LlType::getInt64Ty(ll_ctx)}, false);
  llvm::FunctionCallee callee = ll_module->getOrInsertFunction("strnlen", ft);
  return ll_builder.CreateCall(callee, {ll_charptr, ll_maxlen}, "len");
}

#endif

/* ctor */ OCStringToDescExpr::OCStringToDescExpr(OValSym * avs, OType * desctype)
{
  cstrvalsym = avs;
  ptype = desctype;
}

LlValue * OCStringToDescExpr::Generate(OScope * scope)
{
  OTypeCString * cstrtype = static_cast<OTypeCString *>(cstrvalsym->ptype);

  // Get pointer to first element
  LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
  LlValue * ll_elemptr = ll_builder.CreateGEP(
      cstrtype->GetLlType(), cstrvalsym->ll_value, {ll_zero, ll_zero}, "cstr.data");

  // Build descriptor {ptr, i64}
  LlValue * ll_desc = llvm::UndefValue::get(ptype->GetLlType());
  ll_desc = ll_builder.CreateInsertValue(ll_desc, ll_elemptr, 0, "cstr.desc.ptr");
  ll_desc = ll_builder.CreateInsertValue(ll_desc,
      llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), cstrtype->maxlen),
      1, "cstr.desc.size");
  return ll_desc;
}

/* ctor */ OCStringLitToDescExpr::OCStringLitToDescExpr(OExpr * alit, uint32_t alen, OType * desctype)
{
  litexpr = alit;
  litlen  = alen;
  ptype   = desctype;
}

LlValue * OCStringLitToDescExpr::Generate(OScope * scope)
{
  LlValue * ll_ptr = litexpr->Generate(scope);

  LlValue * ll_desc = llvm::UndefValue::get(ptype->GetLlType());
  ll_desc = ll_builder.CreateInsertValue(ll_desc, ll_ptr, 0, "strlit.desc.ptr");
  ll_desc = ll_builder.CreateInsertValue(ll_desc,
      llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), litlen),
      1, "strlit.desc.size");
  return ll_desc;
}

void OCStringLitToDescExpr::FoldChildren()
{
  OExpr::FoldTree(&litexpr);
}

void OCStringLitToDescExpr::DeleteChildTree()
{
  OExpr::DeleteTree(litexpr);
  litexpr = nullptr;
}

static OExpr * FoldScalarExpr(OExpr * expr)
{
  if (!expr)
  {
    return nullptr;
  }

  OType * exprtype = expr->ResolvedType();
  if (!exprtype)
  {
    return expr;
  }

  if ((TK_INT != exprtype->kind) && (TK_FLOAT != exprtype->kind) && (TK_BOOL != exprtype->kind))
  {
    return expr;
  }

  OValue * folded_value = exprtype->CreateValue();
  if (!folded_value)
  {
    return expr;
  }

  bool fold_ok = folded_value->CalculateConstant(expr, false);
  if (!fold_ok)
  {
    delete folded_value;
    return expr;
  }

  OExpr * result = expr;
  if (TK_INT == exprtype->kind)
  {
    auto * vint = static_cast<OValueInt *>(folded_value);
    result = new OIntLit(vint->value, exprtype);
  }
  else if (TK_FLOAT == exprtype->kind)
  {
    auto * vfloat = static_cast<OValueFloat *>(folded_value);
    result = new OFloatLit(vfloat->value, exprtype);
  }
  else if (TK_BOOL == exprtype->kind)
  {
    auto * vbool = static_cast<OValueBool *>(folded_value);
    result = new OBoolLit(vbool->value, exprtype);
  }

  delete folded_value;
  return result;
}
