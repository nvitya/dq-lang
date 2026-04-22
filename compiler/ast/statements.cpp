/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    statements.h
 * authors: nvitya
 * created: 2026-02-07
 * brief:   statements, statement blocks
 */

#include <format>
#include "scope_builtins.h"
#include "statements.h"
#include "otype_array.h"
#include "otype_cstring.h"
#include "comp_options.h"

using namespace std;

static void GetCStringCopySource(OScope * scope, OExpr * srcexpr, LlValue *& rsrcptr, LlValue *& rsrclimit)
{
  OTypeCString * srctype = dynamic_cast<OTypeCString *>(srcexpr->ResolvedType());
  if (!srctype)
  {
    throw logic_error("CString copy source must have cstring type");
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
      srcaddr = ll_builder.CreateAlloca(srctype->GetLlType(), nullptr, "cstr.src.tmp");
      ll_builder.CreateStore(srcexpr->Generate(scope), srcaddr);
    }

    LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
    rsrcptr = ll_builder.CreateGEP(srctype->GetLlType(), srcaddr, {ll_zero, ll_zero}, "cstr.src.ptr");
    rsrclimit = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), srctype->maxlen);
    return;
  }

  LlValue * ll_desc = srcexpr->Generate(scope);
  rsrcptr = ll_builder.CreateExtractValue(ll_desc, {0}, "cstr.src.ptr");
  rsrclimit = ll_builder.CreateExtractValue(ll_desc, {1}, "cstr.src.size");
}

static void EmitSizedCStringCopy(OScope * scope, LlValue * dstdaddr, OTypeCString * dsttype, OExpr * srcexpr)
{
  LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
  LlValue * ll_one = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 1);
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

  LlValue * ll_copy_limit = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), dsttype->maxlen - 1);

  ll_builder.CreateBr(cond_bb);

  ll_builder.SetInsertPoint(cond_bb);
  llvm::PHINode * ll_i = ll_builder.CreatePHI(LlType::getInt64Ty(ll_ctx), 2, "cstr.copy.i");
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
  llvm::PHINode * ll_term_index = ll_builder.CreatePHI(LlType::getInt64Ty(ll_ctx), 2, "cstr.term.i");
  ll_term_index->addIncoming(ll_i, cond_bb);
  ll_term_index->addIncoming(ll_i, load_bb);
  LlValue * ll_dstnull = ll_builder.CreateGEP(LlType::getInt8Ty(ll_ctx), ll_dstptr, {ll_term_index}, "cstr.dst.null");
  ll_builder.CreateStore(ll_i8_zero, ll_dstnull);
}

static bool EmitCStringStore(OScope * scope, LlValue * dstdaddr, OTypeCString * dsttype, OExpr * srcexpr)
{
  if (!dsttype || (dsttype->maxlen <= 0))
  {
    return false;
  }

  if (!srcexpr)
  {
    LlConst * ll_zero = llvm::ConstantAggregateZero::get(dsttype->GetLlType());
    ll_builder.CreateStore(ll_zero, dstdaddr);
    return true;
  }

  if (auto * strlit = dynamic_cast<OCStringLit *>(srcexpr))
  {
    OValueCString val(dsttype, dsttype->maxlen);
    val.value = strlit->value;
    LlConst * ll_const = val.CreateLlConst();
    ll_builder.CreateStore(ll_const, dstdaddr);
    return true;
  }

  if (dynamic_cast<OTypeCString *>(srcexpr->ResolvedType()))
  {
    EmitSizedCStringCopy(scope, dstdaddr, dsttype, srcexpr);
    return true;
  }

  return false;
}

void OStmt::EmitDebugLocation(OScope * scope, OScPosition * ascpos)
{
  if (not g_opt.dbg_info)
  {
    return;
  }

  if (not scope)
  {
    ll_builder.SetCurrentDebugLocation(llvm::DebugLoc());
    return;
  }

  OScPosition * scp = (ascpos ? ascpos : &scpos);

  ll_builder.SetCurrentDebugLocation(llvm::DILocation::get(ll_ctx, scp->line, scp->col, scope->GetDiScope()));
}

void OStmtBlock::Generate()
{
  for (OStmt * bstmt : stlist)
  {
    bstmt->EmitDebugLocation(scope);
    bstmt->Generate(scope);
    if (ll_builder.GetInsertBlock()->getTerminator()) break;
  }
}

void OStmtReturn::Generate(OScope * scope)
{
  LlValue * ll_value = nullptr;
  if (value)
  {
    if (not vsfunc->vsresult)
    {
      throw logic_error("OStmtReturn::Generate(): return value provided for void function");
    }
    ll_value = value->Generate(scope);
    ll_builder.CreateStore(ll_value, vsfunc->vsresult->ll_value);
  }

  vsfunc->GenerateFuncRet();
}

void OStmtVarDecl::Generate(OScope * scope)
{
  // Local variable declaration
  LlType * ll_type = variable->GetStorageType()->GetLlType();
  variable->ll_value = ll_builder.CreateAlloca(ll_type, nullptr, variable->name);
  if (g_opt.dbg_info)
  {
    llvm::DILocalVariable * di_var = di_builder->createAutoVariable(
        scope->GetDiScope(), variable->name, scpos.scfile->di_file, scpos.line, variable->GetStorageType()->GetDiType() );
    di_builder->insertDeclare(variable->ll_value, di_var, di_builder->createExpression(),
        llvm::DILocation::get(ll_ctx, scpos.line, scpos.col, scope->GetDiScope()), ll_builder.GetInsertBlock() );
  }

  if (variable->IsRefLike())
  {
    if (!initvalue)
    {
      throw logic_error(std::format("Reference variable \"{}\" requires an initializer", variable->name));
    }

    LlValue * ll_initaddr = initvalue->Generate(scope);
    ll_builder.CreateStore(ll_initaddr, variable->ll_value);
    return;
  }

  if (TK_STRING == variable->ptype->kind)
  {
    OTypeCString * cstrtype = static_cast<OTypeCString *>(variable->ptype);
    if (EmitCStringStore(scope, variable->ll_value, cstrtype, initvalue))
    {
      if (!initvalue)
      {
        variable->initialized = true;
      }
      return;
    }
  }

  // Compound type zero-initialization: var sm : SMain = {};
  if (TK_COMPOUND == variable->ptype->kind and not initvalue and variable->initialized)
  {
    LlConst * ll_zero = llvm::ConstantAggregateZero::get(ll_type);
    ll_builder.CreateStore(ll_zero, variable->ll_value);
    return;
  }

  if (initvalue)
  {
    LlValue * ll_initval = initvalue->Generate(scope);
    ll_builder.CreateStore(ll_initval, variable->ll_value);
  }
}

void OStmtAssign::Generate(OScope * scope)
{
  if (TK_STRING == target->ResolvedType()->kind)
  {
    OTypeCString * cstrtype = static_cast<OTypeCString *>(target->ResolvedType());
    if (cstrtype->maxlen > 0)
    {
      LlValue * ll_addr = nullptr;
      if (auto * varref = dynamic_cast<OLValueVar *>(target))
      {
        ll_addr = varref->pvalsym->ll_value;
      }
      else
      {
        ll_addr = target->GenerateAddress(scope);
      }

      if (EmitCStringStore(scope, ll_addr, cstrtype, value))
      {
        return;
      }
    }
  }

  LlValue * ll_addr = target->GenerateAddress(scope);
  LlValue * ll_set_value = value->Generate(scope);
  ll_builder.CreateStore(ll_set_value, ll_addr);
}

void OStmtModifyAssign::Generate(OScope * scope)
{
  LlValue * ll_addr = target->GenerateAddress(scope);
  OType * valtype = target->ptype;

  // Load current value
  LlValue * ll_curval = ll_builder.CreateLoad(valtype->GetLlType(), ll_addr, "cur");
  LlValue * ll_mod_value = value->Generate(scope);

  LlValue * ll_newval = nullptr;
  if (TK_POINTER == valtype->kind)
  {
    OTypePointer * ptrtype = static_cast<OTypePointer *>(valtype);
    LlType * ll_elemtype = ptrtype->basetype->GetLlType();
    if (BINOP_ADD == op)
      ll_newval = ll_builder.CreateGEP(ll_elemtype, ll_curval, {ll_mod_value}, "ptr.adv");
    else if (BINOP_SUB == op)
      ll_newval = ll_builder.CreateGEP(ll_elemtype, ll_curval, {ll_builder.CreateNeg(ll_mod_value)}, "ptr.rev");
  }
  else if (TK_FLOAT == valtype->kind)
  {
    if      (BINOP_ADD == op)  ll_newval = ll_builder.CreateFAdd(ll_curval, ll_mod_value);
    else if (BINOP_SUB == op)  ll_newval = ll_builder.CreateFSub(ll_curval, ll_mod_value);
    else if (BINOP_MUL == op)  ll_newval = ll_builder.CreateFMul(ll_curval, ll_mod_value);
    else if (BINOP_DIV == op)  ll_newval = ll_builder.CreateFDiv(ll_curval, ll_mod_value);
  }
  else if (TK_INT == valtype->kind)
  {
    bool issigned = static_cast<OTypeInt *>(valtype->ResolveAlias())->issigned;

    if      (BINOP_ADD  == op)  ll_newval = ll_builder.CreateAdd(ll_curval, ll_mod_value);
    else if (BINOP_SUB  == op)  ll_newval = ll_builder.CreateSub(ll_curval, ll_mod_value);
    else if (BINOP_MUL  == op)  ll_newval = ll_builder.CreateMul(ll_curval, ll_mod_value);
    else if (BINOP_IDIV == op)  ll_newval = ( issigned ? ll_builder.CreateSDiv(ll_curval, ll_mod_value)
                                                       : ll_builder.CreateUDiv(ll_curval, ll_mod_value) );
    else if (BINOP_IMOD == op)  ll_newval = ( issigned ? ll_builder.CreateSRem(ll_curval, ll_mod_value)
                                                       : ll_builder.CreateURem(ll_curval, ll_mod_value) );
    else if (BINOP_IAND == op)  ll_newval = ll_builder.CreateAnd(ll_curval, ll_mod_value);
    else if (BINOP_IOR  == op)  ll_newval = ll_builder.CreateOr (ll_curval, ll_mod_value);
    else if (BINOP_IXOR == op)  ll_newval = ll_builder.CreateXor(ll_curval, ll_mod_value);
    else if (BINOP_ISHL == op)  ll_newval = ll_builder.CreateShl (ll_curval, ll_mod_value);
    else if (BINOP_ISHR == op)  ll_newval = ( issigned ? ll_builder.CreateAShr(ll_curval, ll_mod_value)
                                                       : ll_builder.CreateLShr(ll_curval, ll_mod_value) );
  }
  else
  {
    throw logic_error(std::format("Unsupported modify-assign type: {}", valtype->name));
  }

  if (ll_newval)
  {
    ll_builder.CreateStore(ll_newval, ll_addr);
  }
  else
  {
    throw logic_error(std::format("Unsupported modify-assign operation: {}", int(op)));
  }
}

void OStmtVoidCall::Generate(OScope * scope)
{
  LlValue * ll_value = callexpr->Generate(scope);
}

void OStmtWhile::Generate(OScope * scope)
{
  LlFunction *    ll_func    = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock *  ll_cond_bb = LlBasicBlock::Create(ll_ctx, "while.cond", ll_func);
  LlBasicBlock *  ll_body_bb = LlBasicBlock::Create(ll_ctx, "while.body", ll_func);
  LlBasicBlock *  ll_end_bb  = LlBasicBlock::Create(ll_ctx, "while.end", ll_func);

  // Push loop context for break/continue
  ll_loop_stack.push_back({ll_cond_bb, ll_end_bb});

  // Jump to condition check
  ll_builder.CreateBr(ll_cond_bb);

  // Generate condition
  ll_builder.SetInsertPoint(ll_cond_bb);
  EmitDebugLocation(scope);
  LlValue * ll_cond = condition->Generate(scope);
  if (ll_cond->getType() != g_builtins->type_bool->GetLlType())
  {
    throw logic_error("Type mismatch: while condition must be bool");
  }

  ll_builder.CreateCondBr(ll_cond, ll_body_bb, ll_end_bb);

  // Generate body
  ll_builder.SetInsertPoint(ll_body_bb);
  body->Generate();

  // Jump back to condition
  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    ll_builder.CreateBr(ll_cond_bb);
  }

  ll_loop_stack.pop_back();

  // Continue after loop
  ll_builder.SetInsertPoint(ll_end_bb);
}

void OBreakStmt::Generate(OScope * scope)
{
  if (ll_loop_stack.size() < 1)
  {
    throw logic_error("BreakStmt::Generate(): empty loop_stack!");
  }

  ll_builder.CreateBr(ll_loop_stack.back().end_bb);
}

void OContinueStmt::Generate(OScope * scope)
{
  if (ll_loop_stack.size() < 1)
  {
    throw logic_error("BreakStmt::Generate(): empty loop_stack!");
  }

  ll_builder.CreateBr(ll_loop_stack.back().cond_bb);
}

void OStmtIf::Generate(OScope * scope)
{
  LlFunction *   ll_func   = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * bb_merge  = LlBasicBlock::Create(ll_ctx, "if.end", ll_func);

  for (size_t i = 0; i < branches.size(); i++)
  {
    OIfBranch * branch = branches[i];
    if (branch->condition == nullptr)
    {
      // else branch - just emit the body
      branch->body->Generate();
      if (!ll_builder.GetInsertBlock()->getTerminator())
      {
        ll_builder.CreateBr(bb_merge);
      }
    }
    else // if or elif branch
    {
      LlValue * ll_cond = branch->condition->Generate(scope);
      if (ll_cond->getType() != g_builtins->type_bool->GetLlType())
      {
        throw runtime_error("Type mismatch: if condition must be bool");
      }

      LlBasicBlock * bb_then = LlBasicBlock::Create(ll_ctx, "if.then", ll_func);
      LlBasicBlock * bb_else;

      // If there's a next branch, the else goes to it; otherwise to merge
      if (i + 1 < branches.size())
      {
        bb_else = LlBasicBlock::Create(ll_ctx, "if.else", ll_func);
      }
      else
      {
        bb_else = bb_merge;
      }

      ll_builder.CreateCondBr(ll_cond, bb_then, bb_else);

      // Generate then body
      ll_builder.SetInsertPoint(bb_then);
      branch->body->Generate();

      if (!ll_builder.GetInsertBlock()->getTerminator())
      {
        ll_builder.CreateBr(bb_merge);
      }

      // Set insert point to else block for next branch
      if (bb_else != bb_merge)
      {
        ll_builder.SetInsertPoint(bb_else);
      }
    }
  }

  ll_builder.SetInsertPoint(bb_merge);
}
