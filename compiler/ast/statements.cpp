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
#include "comp_options.h"

using namespace std;

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
  LlType * ll_type = variable->ptype->GetLlType();
  variable->ll_value = ll_builder.CreateAlloca(ll_type, nullptr, variable->name);
  if (g_opt.dbg_info)
  {
    llvm::DILocalVariable * di_var = di_builder->createAutoVariable(
        scope->GetDiScope(), variable->name, scpos.scfile->di_file, scpos.line, variable->ptype->GetDiType() );
    di_builder->insertDeclare(variable->ll_value, di_var, di_builder->createExpression(),
        llvm::DILocation::get(ll_ctx, scpos.line, scpos.col, scope->GetDiScope()), ll_builder.GetInsertBlock() );
  }

  if (initvalue)
  {
    LlValue * ll_initval = initvalue->Generate(scope);
    ll_builder.CreateStore(ll_initval, variable->ll_value);
  }
}

void OStmtAssign::Generate(OScope * scope)
{
  LlValue * ll_set_value = value->Generate(scope);

  if (!variable->ll_value)
  {
    throw logic_error(std::format("Variable \"{}\" was not prepared in the LLVM", variable->name));
  }

  ll_builder.CreateStore(ll_set_value, variable->ll_value);
}

void OStmtModifyAssign::Generate(OScope *scope)
{
  LlValue * ll_mod_value = value->Generate(scope);

  if (!variable->ll_value)
  {
    throw logic_error(std::format("Variable \"{}\" was not prepared in the LLVM", variable->name));
  }

  // Load current value
  LlValue * ll_curval = ll_builder.CreateLoad(variable->ptype->GetLlType(), variable->ll_value, variable->name);

  LlValue * ll_newval = nullptr;
  if (TK_POINTER == variable->ptype->kind)
  {
    OTypePointer * ptrtype = static_cast<OTypePointer *>(variable->ptype);
    LlType * ll_elemtype = ptrtype->basetype->GetLlType();
    if (BINOP_ADD == op)
      ll_newval = ll_builder.CreateGEP(ll_elemtype, ll_curval, {ll_mod_value}, "ptr.adv");
    else if (BINOP_SUB == op)
      ll_newval = ll_builder.CreateGEP(ll_elemtype, ll_curval, {ll_builder.CreateNeg(ll_mod_value)}, "ptr.rev");
  }
  else
  {
    if      (BINOP_ADD  == op)  ll_newval = ll_builder.CreateAdd(ll_curval, ll_mod_value);
    else if (BINOP_SUB  == op)  ll_newval = ll_builder.CreateSub(ll_curval, ll_mod_value);
    else if (BINOP_MUL  == op)  ll_newval = ll_builder.CreateMul(ll_curval, ll_mod_value);
    else if (BINOP_IDIV == op)  ll_newval = ll_builder.CreateSDiv(ll_curval, ll_mod_value);
  }

  if (ll_newval)
  {
    ll_builder.CreateStore(ll_newval, variable->ll_value);
  }
  else
  {
    throw logic_error(std::format("Unsupported modify-assign operation: {}", int(op)));
  }
}

void OStmtDerefAssign::Generate(OScope * scope)
{
  // Load the pointer value from the pointer variable, then store the value through it
  LlValue * ll_ptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ptrvariable->ll_value, "ptr");
  LlValue * ll_val = value->Generate(scope);
  ll_builder.CreateStore(ll_val, ll_ptr);
}

void OStmtArrayAssign::Generate(OScope * scope)
{
  LlValue * ll_index = indexexpr->Generate(scope);
  LlValue * ll_val = value->Generate(scope);

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
    ll_builder.CreateStore(ll_val, ll_gep);
  }
  else // TK_ARRAY_SLICE
  {
    // Slice: use StructGEP to get the pointer field, then GEP into the data
    OTypeArraySlice * slicetype = static_cast<OTypeArraySlice *>(arrayvalsym->ptype);
    LlType * ll_slicetype = arrayvalsym->ptype->GetLlType();
    LlValue * ll_ptr_addr = ll_builder.CreateStructGEP(ll_slicetype, arrayvalsym->ll_value, 0, "slice.ptr.addr");
    LlValue * ll_ptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_ptr_addr, "slice.ptr");
    LlValue * ll_gep = ll_builder.CreateGEP(slicetype->elemtype->GetLlType(), ll_ptr, {ll_index}, "slice.elem");
    ll_builder.CreateStore(ll_val, ll_gep);
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
