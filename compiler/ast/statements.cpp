/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
#include "otype_string.h"
#include "otype_anyvalue.h"
#include "otype_func.h"
#include "otype_compound.h"
#include "comp_options.h"
#include "named_scopes.h"
#include "dqc.h"

using namespace std;



static bool GenerateDynArrayAssignExpr(OScope * scope, OTypeDynArray * dyntype, LlValue * targetaddr, OExpr * value)
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
      GenerateDynArrayAssignOther(scope, dyntype, targetaddr, srcmgr);
    }
    else
    {
      GenerateDynArrayDestroy(scope, dyntype, targetaddr);
      ll_builder.CreateStore(srcmgr, targetaddr);
    }
    return true;
  }

  return false;
}

bool GenerateAssignmentToAddress(OScope * scope, OType * targettype,
                                 LlValue * targetaddr, OExpr * value,
                                 bool volatile_store)
{
  OType * resolved_type = targettype ? targettype->ResolveAlias() : nullptr;
  if (!resolved_type || !targetaddr || !value)
  {
    return false;
  }
  if (TK_DYNSTR == resolved_type->kind)
  {
    return GenerateStringAssignExpr(scope, targetaddr, value);
  }
  if (auto * dyntype = dynamic_cast<OTypeDynArray *>(resolved_type))
  {
    return GenerateDynArrayAssignExpr(scope, dyntype, targetaddr, value);
  }
  if (TK_ANYVALUE == resolved_type->kind)
  {
    return GenerateAnyValueAssignExpr(scope, targetaddr, value);
  }
  if (auto * cstrtype = dynamic_cast<OTypeCString *>(resolved_type);
      cstrtype && cstrtype->maxlen > 0)
  {
    return cstrtype->GenerateStore(scope, targetaddr, value);
  }

  llvm::StoreInst * store = ll_builder.CreateStore(value->Generate(scope), targetaddr);
  store->setVolatile(volatile_store);
  return true;
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
  LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * bb_cleanup = nullptr;
  LlBasicBlock * bb_done = nullptr;
  bool exception_checks = (g_compiler->DqExceptionFunc("DqExcActive") != nullptr);
  if (exception_checks)
  {
    bb_cleanup = LlBasicBlock::Create(ll_ctx, scope->debugname + ".cleanup", ll_func);
    bb_done = LlBasicBlock::Create(ll_ctx, scope->debugname + ".done", ll_func);
  }
  LlBasicBlock * saved_exception_cleanup_bb = scope->exception_cleanup_bb;
  scope->exception_cleanup_bb = bb_cleanup;

  for (OStmt * bstmt : stlist)
  {
    bstmt->EmitDebugLocation(scope);
    bstmt->Generate(scope);
    if (ll_builder.GetInsertBlock()->getTerminator()) break;
    if (exception_checks)
    {
      EmitExpressionExceptionCheck(scope);
    }
  }

  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    if (exception_checks)
    {
      ll_builder.CreateBr(bb_cleanup);
    }
    else
    {
      scope->EmitOwnedObjectDestructors();
    }
  }

  if (exception_checks)
  {
    ll_builder.SetInsertPoint(bb_cleanup);
    scope->EmitOwnedObjectDestructors();
    if (!ll_builder.GetInsertBlock()->getTerminator())
    {
      ll_builder.CreateBr(bb_done);
    }
    ll_builder.SetInsertPoint(bb_done);
  }
  scope->exception_cleanup_bb = saved_exception_cleanup_bb;
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
    if (TK_DYNSTR == vsfunc->vsresult->ptype->ResolveAlias()->kind)
    {
      if (!GenerateStringAssignExpr(scope, vsfunc->vsresult->ll_value, value))
      {
        throw logic_error("Unsupported string return value");
      }
    }
    else if (auto * dyntype = dynamic_cast<OTypeDynArray *>(vsfunc->vsresult->ptype->ResolveAlias()))
    {
      if (!GenerateDynArrayAssignExpr(scope, dyntype, vsfunc->vsresult->ll_value, value))
      {
        throw logic_error("Unsupported dynamic array return value");
      }
    }
    else if (TK_ANYVALUE == vsfunc->vsresult->ptype->ResolveAlias()->kind)
    {
      if (!GenerateAnyValueAssignExpr(scope, vsfunc->vsresult->ll_value, value))
      {
        throw logic_error("Unsupported anyvalue return value");
      }
    }
    else
    {
      auto * result_object_type = dynamic_cast<OTypeObject *>(vsfunc->vsresult->ptype ? vsfunc->vsresult->ptype->ResolveAlias() : nullptr);
      if (result_object_type)
      {
        ll_value = value->Generate(scope);
        OType * storage_type = vsfunc->vsresult->GetStorageType();
        if (ll_value->getType() != storage_type->GetLlType())
        {
          ll_value = ll_builder.CreateBitCast(ll_value, storage_type->GetLlType());
        }
      }
      else
      {
        ll_value = value->Generate(scope);
      }
      ll_builder.CreateStore(ll_value, vsfunc->vsresult->ll_value);
    }
  }

  g_compiler->EmitActiveFinallyBlocks();
  if (ll_builder.GetInsertBlock()->getTerminator())
  {
    return;
  }

  LlValue * active = g_compiler->DqExceptionActiveValue();
  if (active)
  {
    LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
    LlBasicBlock * bb_exception = LlBasicBlock::Create(ll_ctx, "return.exception", ll_func);
    LlBasicBlock * bb_return = LlBasicBlock::Create(ll_ctx, "return.normal", ll_func);
    ll_builder.CreateCondBr(active, bb_exception, bb_return);

    ll_builder.SetInsertPoint(bb_exception);
    scope->EmitOwnedObjectDestructorsForReturn(vsfunc);
    vsfunc->GenerateFuncRet();

    ll_builder.SetInsertPoint(bb_return);
  }

  scope->EmitOwnedObjectDestructorsForReturn(vsfunc);
  vsfunc->GenerateFuncRet();
}

void OStmtVarDecl::Generate(OScope * scope)
{
  // Local variable declaration
  OType * storage_type = variable->GetStorageType();
  LlType * ll_type = storage_type->GetLlType();
  auto * alloca = CreateEntryBlockAlloca(ll_type, nullptr, variable->name);
  alloca->setAlignment(llvm::Align(EffectiveStorageAlign(storage_type, variable->attr_align)));
  variable->ll_value = alloca;
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

  if (auto * dyntype = dynamic_cast<OTypeDynArray *>(variable->ptype ? variable->ptype->ResolveAlias() : nullptr))
  {
    GenerateDynArrayCreate(scope, dyntype, variable->ll_value);
    if (initvalue)
    {
      if (!GenerateDynArrayAssignExpr(scope, dyntype, variable->ll_value, initvalue))
      {
        throw logic_error(std::format("Unsupported dynamic array initializer for \"{}\"", variable->name));
      }
    }
    variable->initialized = true;
    return;
  }

  if (auto * strtype = dynamic_cast<OTypeDynString *>(variable->ptype ? variable->ptype->ResolveAlias() : nullptr))
  {
    (void)strtype;
    GenerateStringCreate(scope, variable->ll_value);
    if (initvalue)
    {
      if (!GenerateStringAssignExpr(scope, variable->ll_value, initvalue))
      {
        throw logic_error(std::format("Unsupported string initializer for \"{}\"", variable->name));
      }
    }
    variable->initialized = true;
    return;
  }

  if (auto * anytype = dynamic_cast<OTypeAnyValue *>(variable->ptype ? variable->ptype->ResolveAlias() : nullptr))
  {
    (void)anytype;
    GenerateAnyValueCreate(scope, variable->ll_value);
    if (initvalue)
    {
      if (!GenerateAnyValueAssignExpr(scope, variable->ll_value, initvalue))
      {
        throw logic_error(std::format("Unsupported anyvalue initializer for \"{}\"", variable->name));
      }
    }
    variable->initialized = true;
    return;
  }

  if (TK_STRVIEW == variable->ptype->ResolveAlias()->kind)
  {
    if (!initvalue)
    {
      LlConst * ll_zero = llvm::ConstantAggregateZero::get(variable->ptype->GetLlType());
      ll_builder.CreateStore(ll_zero, variable->ll_value);
      variable->initialized = true;
      return;
    }
  }

  auto * objvar = dynamic_cast<OVsObject *>(variable);
  if (objvar && objvar->IsObjectReference())
  {
    LlValue * ll_init = nullptr;
    if (initvalue)
    {
      ll_init = initvalue->Generate(scope);
    }
    else
    {
      ll_init = llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
    }
    ll_builder.CreateStore(ll_init, variable->ll_value);
    return;
  }

  if (objvar && objvar->IsFixedObjectStorage())
  {
    LlConst * ll_zero = llvm::ConstantAggregateZero::get(variable->ptype->GetLlType());
    ll_builder.CreateStore(ll_zero, variable->ll_value);
    objvar->GenerateConstructorCall(scope, variable->ll_value);
    return;
  }

  if (TK_CSTRING == variable->ptype->kind)
  {
    OTypeCString * cstrtype = static_cast<OTypeCString *>(variable->ptype);
    if (cstrtype->GenerateStore(scope, variable->ll_value, initvalue))
    {
      if (!initvalue)
      {
        variable->initialized = true;
      }
      return;
    }
  }

  // Compound/static-array zero-initialization.
  if ((variable->ptype->IsCompound() || TK_ARRAY == variable->ptype->kind) and not initvalue and variable->initialized)
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

OStmtObjectCall::~OStmtObjectCall()
{
  OExpr::DeleteTree(target);
  target = nullptr;
  for (OExpr *& arg : args)
  {
    OExpr::DeleteTree(arg);
    arg = nullptr;
  }
  args.clear();
}

void OStmtObjectCall::Generate(OScope * scope)
{
  if (!method || !method->ll_func || !target)
  {
    return;
  }

  vector<LlValue *> ll_args;
  ll_args.push_back(target->GenerateObjectAddress(scope));
  for (OExpr * arg : args)
  {
    ll_args.push_back(arg->Generate(scope));
  }
  GenerateFunctionCall(scope, method, ll_args);
}

OStmtInheritedCall::~OStmtInheritedCall()
{
  for (OExpr *& arg : args)
  {
    OExpr::DeleteTree(arg);
    arg = nullptr;
  }
  args.clear();
}

void OStmtInheritedCall::Generate(OScope * scope)
{
  if (!caller || !method || !method->ll_func || !caller->receiver_arg)
  {
    return;
  }

  OLValueVar this_expr(caller->receiver_arg);
  LlValue * ll_this = this_expr.GenerateAddress(scope);

  if (emit_derived_field_destroy)
  {
    auto * object_type = dynamic_cast<OTypeObject *>(caller->owner_compound_type);
    if (object_type)
    {
      object_type->GenerateFieldDestructors(scope, ll_this);
    }
  }

  vector<LlValue *> ll_args;
  ll_args.push_back(ll_this);
  for (OExpr * arg : args)
  {
    ll_args.push_back(arg->Generate(scope));
  }
  GenerateFunctionCall(scope, method, ll_args, true);

  if (emit_derived_field_init && caller->owner_compound_type)
  {
    auto * owner_object = dynamic_cast<OTypeObject *>(caller->owner_compound_type);
    if (owner_object)
    {
      owner_object->GenerateVTableStore(ll_this);
      owner_object->GenerateFieldInitializers(scope, ll_this);
    }
  }
}

void OStmtConstructFixedObject::Generate(OScope * scope)
{
  (void)scope;
  auto * objvar = dynamic_cast<OVsObject *>(variable);
  if (!objvar || !objvar->IsFixedObjectStorage())
  {
    return;
  }
  objvar->GenerateConstructorCall(scope, variable->ll_value);
}

void OStmtConstructDynArray::Generate(OScope * scope)
{
  auto * dyntype = dynamic_cast<OTypeDynArray *>(variable && variable->ptype ? variable->ptype->ResolveAlias() : nullptr);
  if (!dyntype || !variable->ll_value)
  {
    return;
  }
  GenerateDynArrayCreate(scope, dyntype, variable->ll_value);
}

void OStmtAssign::Generate(OScope * scope)
{
  if (auto * idx = dynamic_cast<OLValueIndex *>(target))
  {
    OType * ctype = idx->containertype ? idx->containertype->ResolveAlias() : nullptr;
    if (ctype && TK_DYNSTR == ctype->kind)
    {
      GenerateStringSetChar(scope, idx->base, idx->indexexpr, value);
      return;
    }
  }

  LlValue * ll_addr = target->GenerateAddress(scope);
  if (!GenerateAssignmentToAddress(scope, target->ptype, ll_addr, value, target->RequiresVolatileMemoryAccess()))
  {
    throw logic_error("Unsupported assignment");
  }
}

void OStmtModifyAssign::Generate(OScope * scope)
{
  LlValue * ll_addr = target->GenerateAddress(scope);
  OType * valtype = target->ptype;

  if (TK_DYNSTR == valtype->ResolveAlias()->kind && BINOP_ADD == op)
  {
    LlValue * ll_newval = GenerateStringConcat(scope, target, value);
    GenerateStringDestroy(scope, ll_addr);
    ll_builder.CreateStore(ll_newval, ll_addr);
    return;
  }

  // Load current value
  llvm::LoadInst * ll_curval = ll_builder.CreateLoad(valtype->GetLlType(), ll_addr, "cur");
  ll_curval->setVolatile(target->RequiresVolatileMemoryAccess());
  LlValue * ll_mod_value = value->Generate(scope);
  LlValue * ll_newval = GenerateModifyAssignValue(valtype, op, ll_curval, ll_mod_value);
  llvm::StoreInst * store = ll_builder.CreateStore(ll_newval, ll_addr);
  store->setVolatile(target->RequiresVolatileMemoryAccess());
}

LlValue * GenerateModifyAssignValue(OType * valtype, EBinOp op,
                                    LlValue * ll_curval, LlValue * ll_mod_value)
{
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
    return ll_newval;
  }
  throw logic_error(std::format("Unsupported modify-assign operation: {}", int(op)));
}

OStmtPropertyAssign::~OStmtPropertyAssign()
{
  OExpr::DeleteTree(target);
  target = nullptr;
  OExpr::DeleteTree(value);
  value = nullptr;
}

void OStmtPropertyAssign::Generate(OScope * scope)
{
  if (BINOP_NONE == op)
  {
    target->GenerateWrite(scope, value);
  }
  else
  {
    target->GenerateModifyWrite(scope, op, value);
  }
}

void OStmtVoidCall::Generate(OScope * scope)
{
  LlValue * ll_value = callexpr->Generate(scope);
}

void OStmtDelete::Generate(OScope * scope)
{
  if (!memfree_func || !memfree_func->ll_func)
  {
    throw runtime_error("OStmtDelete::Generate(): missing MemFree function");
  }

  OTypeObject * object_type = dynamic_cast<OTypeObject *>(ptrexpr->ResolvedType());
  LlValue * ll_ptr = nullptr;
  if (object_type)
  {
    auto * lval = dynamic_cast<OLValueExpr *>(ptrexpr);
    ll_ptr = (lval ? lval->GenerateObjectAddress(scope) : ptrexpr->Generate(scope));
  }
  else
  {
    ll_ptr = ptrexpr->Generate(scope);
  }

  LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * bb_delete = LlBasicBlock::Create(ll_ctx, "delete.run", ll_func);
  LlBasicBlock * bb_done = LlBasicBlock::Create(ll_ctx, "delete.done", ll_func);
  LlValue * ll_null = llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
  LlValue * ll_is_null = ll_builder.CreateICmpEQ(ll_ptr, ll_null, "delete.is_null");
  ll_builder.CreateCondBr(ll_is_null, bb_done, bb_delete);

  ll_builder.SetInsertPoint(bb_delete);
  if (object_type && object_type->is_polymorphic)
  {
    OTypeObject * root = object_type;
    while (root->base_type)
    {
      root = root->GetBaseObject();
    }
    root->GetLlType();
    LlValue * ll_vptr_addr = ll_builder.CreateStructGEP(root->GetLlType(), ll_ptr,
        root->vtable_field_index, "delete.vtable.addr");
    LlValue * ll_vptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_vptr_addr, "delete.vtable");
    LlValue * ll_slot_addr = ll_builder.CreateGEP(llvm::PointerType::get(ll_ctx, 0), ll_vptr,
        {llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 1)}, "delete.dtor.slot");
    LlValue * ll_dtor = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_slot_addr, "delete.dtor");
    if (object_dtor_func)
    {
      ll_builder.CreateCall(static_cast<LlFuncType *>(object_dtor_func->ptype->GetLlType()), ll_dtor, {ll_ptr});
    }
  }
  else if (object_dtor_func && object_dtor_func->ll_func)
  {
    ll_builder.CreateCall(object_dtor_func->ll_func, {ll_ptr});
  }
  ll_builder.CreateCall(memfree_func->ll_func, {ll_ptr});

  if (clear_after_free)
  {
    auto * lval = dynamic_cast<OLValueExpr *>(ptrexpr);
    if (!lval)
    {
      throw logic_error("OStmtDelete::Generate(): clear target is not an lvalue");
    }

    LlValue * ll_addr = lval->GenerateAddress(scope);
    ll_builder.CreateStore(ll_null, ll_addr);
  }

  ll_builder.CreateBr(bb_done);
  ll_builder.SetInsertPoint(bb_done);
}

void OStmtModuleInitCalls::Generate(OScope * scope)
{
  if (guard)
  {
    LlType * bool_type = g_builtins->type_bool->GetLlType();
    LlValue * initialized = ll_builder.CreateLoad(bool_type, guard->ll_value, guard->name);
    LlFunction * ll_curfunc = ll_builder.GetInsertBlock()->getParent();
    LlBasicBlock * bb_run = LlBasicBlock::Create(ll_ctx, "module_init.run", ll_curfunc);
    LlBasicBlock * bb_done = LlBasicBlock::Create(ll_ctx, "module_init.done", ll_curfunc);

    ll_builder.CreateCondBr(initialized, bb_done, bb_run);
    ll_builder.SetInsertPoint(bb_done);
    ll_builder.CreateRetVoid();

    ll_builder.SetInsertPoint(bb_run);
    ll_builder.CreateStore(llvm::ConstantInt::get(static_cast<llvm::IntegerType *>(bool_type), 1), guard->ll_value);
  }

  for (OValSymFunc * init_func : init_funcs)
  {
    if (!init_func || !init_func->ll_func)
    {
      throw runtime_error("OStmtModuleInitCalls::Generate(): missing module init function");
    }
    GenerateFunctionCall(scope, init_func, {});
  }
}

void OStmtWhile::Generate(OScope * scope)
{
  LlFunction *    ll_func    = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock *  ll_cond_bb = LlBasicBlock::Create(ll_ctx, "while.cond", ll_func);
  LlBasicBlock *  ll_body_bb = LlBasicBlock::Create(ll_ctx, "while.body", ll_func);
  LlBasicBlock *  ll_end_bb  = LlBasicBlock::Create(ll_ctx, "while.end", ll_func);

  // Push loop context for break/continue
  ll_loop_stack.push_back({ll_cond_bb, ll_end_bb, body->scope});

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
    LlValue * active = g_compiler->DqExceptionActiveValue();
    if (active)
    {
      ll_builder.CreateCondBr(active, ll_end_bb, ll_cond_bb);
    }
    else
    {
      ll_builder.CreateBr(ll_cond_bb);
    }
  }

  ll_loop_stack.pop_back();

  // Continue after loop
  ll_builder.SetInsertPoint(ll_end_bb);
}

void OStmtFor::Generate(OScope * scope)
{
  init->Generate();
  if (ll_builder.GetInsertBlock()->getTerminator())
  {
    return;
  }

  LlFunction *    ll_func    = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock *  ll_cond_bb = LlBasicBlock::Create(ll_ctx, "for.cond", ll_func);
  LlBasicBlock *  ll_body_bb = LlBasicBlock::Create(ll_ctx, "for.body", ll_func);
  LlBasicBlock *  ll_step_bb = LlBasicBlock::Create(ll_ctx, "for.step", ll_func);
  LlBasicBlock *  ll_end_bb  = LlBasicBlock::Create(ll_ctx, "for.end", ll_func);

  ll_loop_stack.push_back({ll_step_bb, ll_end_bb, body->scope});

  ll_builder.CreateBr(ll_cond_bb);

  ll_builder.SetInsertPoint(ll_cond_bb);
  EmitDebugLocation(init->scope);
  LlValue * ll_cond = condition->Generate(init->scope);
  if (ll_cond->getType() != g_builtins->type_bool->GetLlType())
  {
    throw logic_error("Type mismatch: for condition must be bool");
  }

  ll_builder.CreateCondBr(ll_cond, ll_body_bb, ll_end_bb);

  ll_builder.SetInsertPoint(ll_body_bb);
  body->Generate();
  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    LlValue * active = g_compiler->DqExceptionActiveValue();
    if (active)
    {
      ll_builder.CreateCondBr(active, ll_end_bb, ll_step_bb);
    }
    else
    {
      ll_builder.CreateBr(ll_step_bb);
    }
  }

  ll_builder.SetInsertPoint(ll_step_bb);
  step->Generate();
  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    LlValue * active = g_compiler->DqExceptionActiveValue();
    if (active)
    {
      ll_builder.CreateCondBr(active, ll_end_bb, ll_cond_bb);
    }
    else
    {
      ll_builder.CreateBr(ll_cond_bb);
    }
  }

  ll_loop_stack.pop_back();

  ll_builder.SetInsertPoint(ll_end_bb);
}

void OBreakStmt::Generate(OScope * scope)
{
  if (ll_loop_stack.size() < 1)
  {
    throw logic_error("BreakStmt::Generate(): empty loop_stack!");
  }

  g_compiler->EmitActiveFinallyBlocks();
  if (ll_builder.GetInsertBlock()->getTerminator())
  {
    return;
  }

  scope->EmitOwnedObjectDestructorsUntil(ll_loop_stack.back().cleanup_scope);

  LlValue * active = g_compiler->DqExceptionActiveValue();
  if (active)
  {
    ll_builder.CreateCondBr(active, ll_loop_stack.back().end_bb, ll_loop_stack.back().end_bb);
  }
  else
  {
    ll_builder.CreateBr(ll_loop_stack.back().end_bb);
  }
}

void OContinueStmt::Generate(OScope * scope)
{
  if (ll_loop_stack.size() < 1)
  {
    throw logic_error("BreakStmt::Generate(): empty loop_stack!");
  }

  g_compiler->EmitActiveFinallyBlocks();
  if (ll_builder.GetInsertBlock()->getTerminator())
  {
    return;
  }

  scope->EmitOwnedObjectDestructorsUntil(ll_loop_stack.back().cleanup_scope);

  LlValue * active = g_compiler->DqExceptionActiveValue();
  if (active)
  {
    ll_builder.CreateCondBr(active, ll_loop_stack.back().end_bb, ll_loop_stack.back().cond_bb);
  }
  else
  {
    ll_builder.CreateBr(ll_loop_stack.back().cond_bb);
  }
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

void OStmtRaise::Generate(OScope * scope)
{
  (void)scope;

  if (!value)
  {
    OValSymFunc * fn = g_compiler->DqExceptionFunc("DqExcRethrow");
    if (!fn || !fn->ll_func)
    {
      throw runtime_error("OStmtRaise::Generate(): missing DqExcRethrow");
    }
    ll_builder.CreateCall(fn->ll_func, {});
    EmitExpressionExceptionCheck(scope);
    return;
  }

  OValSymFunc * fn = g_compiler->DqExceptionFunc("DqExcRaise");
  if (!fn || !fn->ll_func)
  {
    throw runtime_error("OStmtRaise::Generate(): missing DqExcRaise");
  }

  LlValue * exc = value->Generate(scope);
  LlValue * owns_initial_ref = llvm::ConstantInt::get(g_builtins->type_bool->GetLlType(), (dynamic_cast<ONewExpr *>(value) != nullptr) ? 1 : 0);
  ll_builder.CreateCall(fn->ll_func, {exc, owns_initial_ref});
  EmitExpressionExceptionCheck(scope);
}



void OStmtTry::Generate(OScope * scope)
{
  LlFunction * ll_func = ll_builder.GetInsertBlock()->getParent();
  LlBasicBlock * bb_dispatch = LlBasicBlock::Create(ll_ctx, "try.dispatch", ll_func);
  LlBasicBlock * bb_after_handlers = LlBasicBlock::Create(ll_ctx, "try.after_handlers", ll_func);
  LlBasicBlock * bb_finally = finally_body ? LlBasicBlock::Create(ll_ctx, "try.finally", ll_func) : nullptr;
  LlBasicBlock * bb_done = LlBasicBlock::Create(ll_ctx, "try.done", ll_func);

  if (finally_body)
  {
    g_compiler->ll_finally_stack.push_back(finally_body);
  }
  body->Generate();
  if (finally_body)
  {
    g_compiler->ll_finally_stack.pop_back();
  }
  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    LlValue * active = g_compiler->DqExceptionActiveValue();
    if (active && !except_branches.empty())
    {
      ll_builder.CreateCondBr(active, bb_dispatch, bb_after_handlers);
    }
    else
    {
      ll_builder.CreateBr(bb_after_handlers);
    }
  }

  ll_builder.SetInsertPoint(bb_dispatch);
  if (except_branches.empty())
  {
    ll_builder.CreateBr(bb_after_handlers);
  }
  else
  {
    for (size_t i = 0; i < except_branches.size(); ++i)
    {
      OExceptBranch * branch = except_branches[i];
      LlBasicBlock * bb_match = LlBasicBlock::Create(ll_ctx, "except.match", ll_func);
      LlBasicBlock * bb_next = (i + 1 < except_branches.size())
          ? LlBasicBlock::Create(ll_ctx, "except.next", ll_func)
          : bb_after_handlers;

      g_compiler->GenerateExceptBranchMatch(branch, bb_match, bb_next);

      ll_builder.SetInsertPoint(bb_match);
      if (branch->bound_variable)
      {
        OType * storage_type = branch->bound_variable->GetStorageType();
        auto * alloca = CreateEntryBlockAlloca(storage_type->GetLlType(), nullptr, branch->bound_variable->name);
        alloca->setAlignment(llvm::Align(EffectiveStorageAlign(storage_type, branch->bound_variable->attr_align)));
        branch->bound_variable->ll_value = alloca;
        LlValue * exc = g_compiler->DqCurrentExceptionValue();
        if (exc && exc->getType() != storage_type->GetLlType())
        {
          exc = ll_builder.CreateBitCast(exc, storage_type->GetLlType(), "except.cast");
        }
        if (exc)
        {
          ll_builder.CreateStore(exc, branch->bound_variable->ll_value);
        }
      }
      g_compiler->DqBeginCatch();
      if (finally_body)
      {
        g_compiler->ll_finally_stack.push_back(finally_body);
      }
      branch->body->Generate();
      if (finally_body)
      {
        g_compiler->ll_finally_stack.pop_back();
      }
      if (!ll_builder.GetInsertBlock()->getTerminator())
      {
        g_compiler->DqEndCatch();
        ll_builder.CreateBr(bb_after_handlers);
      }

      if (bb_next != bb_after_handlers)
      {
        ll_builder.SetInsertPoint(bb_next);
      }
    }
  }

  ll_builder.SetInsertPoint(bb_after_handlers);
  if (finally_body)
  {
    ll_builder.CreateBr(bb_finally);
    ll_builder.SetInsertPoint(bb_finally);
    finally_body->Generate();
    if (!ll_builder.GetInsertBlock()->getTerminator())
    {
      ll_builder.CreateBr(bb_done);
    }
  }
  else if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    ll_builder.CreateBr(bb_done);
  }

  ll_builder.SetInsertPoint(bb_done);
}
