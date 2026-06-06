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
#include "otype_func.h"
#include "otype_object.h"
#include "comp_options.h"

using namespace std;

static void EmitOwnedObjectDestructors(OScope * scope)
{
  if (!scope)
  {
    return;
  }

  for (auto it = scope->owned_objects.rbegin(); it != scope->owned_objects.rend(); ++it)
  {
    OValSym * vs = *it;
    auto * objvar = dynamic_cast<OVsObject *>(vs);
    if (objvar && objvar->IsFixedObjectStorage())
    {
      objvar->GenerateDestructorCall(vs->ll_value);
      continue;
    }

    auto * dyntype = dynamic_cast<OTypeDynArray *>(vs && vs->ptype ? vs->ptype->ResolveAlias() : nullptr);
    if (dyntype && vs->ll_value)
    {
      GenerateDynArrayDestroy(scope, dyntype, vs->ll_value);
    }
  }
}

static void EmitOwnedObjectDestructorsForReturn(OScope * scope, OValSymFunc * vsfunc)
{
  OScope * stop_scope = (vsfunc && vsfunc->body ? vsfunc->body->scope : nullptr);
  for (OScope * cur = scope; cur; cur = cur->parent_scope)
  {
    EmitOwnedObjectDestructors(cur);
    if (cur == stop_scope)
    {
      break;
    }
  }
}

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

  if (TK_ARRAY == srctype->kind)
  {
    auto * arrtype = static_cast<OTypeArray *>(srctype);
    LlValue * srcptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
    LlValue * count = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), arrtype->arraylength);
    if (arrtype->arraylength > 0)
    {
      LlValue * arrayaddr = nullptr;
      if (auto * arrlit = dynamic_cast<OArrayLit *>(value))
      {
        arrayaddr = ll_builder.CreateAlloca(arrtype->GetLlType(), nullptr, "dyn.assign.literal");
        ll_builder.CreateStore(arrlit->Generate(scope), arrayaddr);
      }
      else if (auto * lval = dynamic_cast<OLValueExpr *>(value))
      {
        arrayaddr = lval->GenerateAddress(scope);
      }
      if (!arrayaddr)
      {
        return false;
      }
      LlValue * zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
      srcptr = ll_builder.CreateGEP(arrtype->GetLlType(), arrayaddr, {zero, zero}, "dyn.assign.arr.ptr");
    }
    GenerateDynArrayAssignData(scope, dyntype, targetaddr, srcptr, count);
    return true;
  }

  if (TK_ARRAY_SLICE == srctype->kind)
  {
    LlValue * slice = value->Generate(scope);
    LlValue * srcptr = ll_builder.CreateExtractValue(slice, {0}, "dyn.assign.slice.ptr");
    LlValue * count = ll_builder.CreateExtractValue(slice, {1}, "dyn.assign.slice.len");
    GenerateDynArrayAssignData(scope, dyntype, targetaddr, srcptr, count);
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

  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    EmitOwnedObjectDestructors(scope);
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

  EmitOwnedObjectDestructorsForReturn(scope, vsfunc);

  vsfunc->GenerateFuncRet();
}

void OStmtVarDecl::Generate(OScope * scope)
{
  // Local variable declaration
  OType * storage_type = variable->GetStorageType();
  LlType * ll_type = storage_type->GetLlType();
  auto * alloca = ll_builder.CreateAlloca(ll_type, nullptr, variable->name);
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

  if (TK_STRING == variable->ptype->kind)
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
  if ((TK_COMPOUND == variable->ptype->kind || TK_ARRAY == variable->ptype->kind) and not initvalue and variable->initialized)
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
  ll_builder.CreateCall(method->ll_func, ll_args);
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

  auto emit_field_init = [&]()
  {
    auto * object_type = dynamic_cast<OTypeObject *>(caller->owner_compound_type);
    if (!object_type)
    {
      return;
    }
    object_type->GetLlType();
    for (OValSym * member : object_type->member_order)
    {
      if (!member)
      {
        continue;
      }
      LlValue * ll_field_addr = ll_builder.CreateStructGEP(object_type->GetLlType(), ll_this,
          member->ll_field_index, member->name + ".addr");
      if (auto * objmember = dynamic_cast<OVsObject *>(member); objmember && objmember->IsFixedObjectStorage())
      {
        objmember->GenerateConstructorCall(scope, ll_field_addr);
      }
      else if (member->field_init_expr)
      {
        member->GenerateFieldInitStore(scope, ll_field_addr);
      }
    }
  };

  auto emit_field_destroy = [&]()
  {
    auto * object_type = dynamic_cast<OTypeObject *>(caller->owner_compound_type);
    if (!object_type)
    {
      return;
    }
    object_type->GetLlType();
    for (auto it = object_type->member_order.rbegin(); it != object_type->member_order.rend(); ++it)
    {
      OValSym * member = *it;
      auto * objmember = dynamic_cast<OVsObject *>(member);
      if (!objmember || !objmember->IsFixedObjectStorage())
      {
        continue;
      }
      LlValue * ll_field_addr = ll_builder.CreateStructGEP(object_type->GetLlType(), ll_this,
          member->ll_field_index, member->name + ".addr");
      objmember->GenerateDestructorCall(ll_field_addr);
    }
  };

  if (emit_derived_field_destroy && caller->owner_compound_type)
  {
    emit_field_destroy();
  }

  vector<LlValue *> ll_args;
  ll_args.push_back(ll_this);
  for (OExpr * arg : args)
  {
    ll_args.push_back(arg->Generate(scope));
  }
  ll_builder.CreateCall(method->ll_func, ll_args);

  if (emit_derived_field_init && caller->owner_compound_type)
  {
    auto * owner_object = dynamic_cast<OTypeObject *>(caller->owner_compound_type);
    if (owner_object)
    {
      owner_object->GenerateVTableStore(ll_this);
    }
    emit_field_init();
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
  if (auto * dyntype = dynamic_cast<OTypeDynArray *>(target->ResolvedType()))
  {
    LlValue * ll_addr = target->GenerateAddress(scope);
    if (!GenerateDynArrayAssignExpr(scope, dyntype, ll_addr, value))
    {
      throw logic_error("Unsupported dynamic array assignment");
    }
    return;
  }

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

      if (cstrtype->GenerateStore(scope, ll_addr, value))
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
      root = root->base_type;
    }
    root->GetLlType();
    LlValue * ll_vptr_addr = ll_builder.CreateStructGEP(root->GetLlType(), ll_ptr,
        root->vtable_field_index, "delete.vtable.addr");
    LlValue * ll_vptr = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), ll_vptr_addr, "delete.vtable");
    LlValue * ll_slot_addr = ll_builder.CreateGEP(llvm::PointerType::get(ll_ctx, 0), ll_vptr,
        {llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0)}, "delete.dtor.slot");
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
    ll_builder.CreateCall(init_func->ll_func, {});
  }
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

  ll_loop_stack.push_back({ll_step_bb, ll_end_bb});

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
    ll_builder.CreateBr(ll_step_bb);
  }

  ll_builder.SetInsertPoint(ll_step_bb);
  step->Generate();
  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    ll_builder.CreateBr(ll_cond_bb);
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
