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
#include "otype_string.h"
#include "otype_anyvalue.h"
#include "otype_func.h"
#include "otype_compound.h"
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
    if (objvar && objvar->IsFixedObjectStorage() && vs->ll_value)
    {
      objvar->GenerateDestructorCall(vs->ll_value);
      continue;
    }

    auto * dyntype = dynamic_cast<OTypeDynArray *>(vs && vs->ptype ? vs->ptype->ResolveAlias() : nullptr);
    if (dyntype && vs->ll_value)
    {
      GenerateDynArrayDestroy(scope, dyntype, vs->ll_value);
    }
    auto * strtype = dynamic_cast<OTypeDynString *>(vs && vs->ptype ? vs->ptype->ResolveAlias() : nullptr);
    if (strtype && vs->ll_value)
    {
      GenerateStringDestroy(scope, vs->ll_value);
    }
    auto * anytype = dynamic_cast<OTypeAnyValue *>(vs && vs->ptype ? vs->ptype->ResolveAlias() : nullptr);
    if (anytype && vs->ll_value)
    {
      GenerateAnyValueDestroy(scope, vs->ll_value);
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

llvm::FunctionCallee get_unwind_resume() {
  return ll_module->getOrInsertFunction("_Unwind_Resume", llvm::Type::getVoidTy(ll_ctx), llvm::PointerType::get(ll_ctx, 0));
}
llvm::FunctionCallee get_gxx_personality_v0() {
  return ll_module->getOrInsertFunction("__gxx_personality_v0", llvm::Type::getInt32Ty(ll_ctx));
}

void GeneratePendingLandingPads(llvm::Function * ll_func)
{
  if (ll_pending_lpads.empty()) return;
  
  ll_func->setPersonalityFn(llvm::cast<llvm::Constant>(get_gxx_personality_v0().getCallee()));
  
  for (const SPendingLandingPad & lpad : ll_pending_lpads)
  {
    ll_builder.SetInsertPoint(lpad.lpad_bb);
    
    llvm::StructType * lpad_type = llvm::StructType::get(ll_ctx, {llvm::PointerType::get(ll_ctx, 0), llvm::Type::getInt32Ty(ll_ctx)});
    int num_clauses = (lpad.active_try ? 1 : 0);
    llvm::LandingPadInst * ll_lpad = ll_builder.CreateLandingPad(lpad_type, num_clauses, "lpad.inst");
    ll_lpad->setCleanup(true);
    
    if (lpad.active_try)
    {
      ll_lpad->addClause(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0))); // catch-all
    }
    
    // run destructors
    OScope * stop_scope = lpad.active_try ? lpad.active_try->try_scope : nullptr;
    for (OScope * cur = lpad.active_scope; cur; cur = cur->parent_scope)
    {
      EmitOwnedObjectDestructors(cur);
      if (cur == stop_scope) break;
    }
    
    if (lpad.active_try)
    {
      ll_builder.CreateStore(ll_lpad, lpad.active_try->ll_lpad_val_alloca);
      ll_builder.CreateBr(lpad.active_try->ll_bb_dispatch);
    }
    else
    {
      llvm::Value * exc_ptr = ll_builder.CreateExtractValue(ll_lpad, 0, "exc.ptr");
      auto resume_func = get_unwind_resume();
      ll_builder.CreateCall(resume_func.getFunctionType(), resume_func.getCallee(), {exc_ptr});
      ll_builder.CreateUnreachable();
    }
  }
  
  ll_pending_lpads.clear();
}

void OStmtBlock::Generate()
{
  OScope * prev_scope = ll_cg_scope;
  ll_cg_scope = scope;

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
  
  ll_cg_scope = prev_scope;
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
    else if (TK_ANYVALUE == vsfunc->vsresult->ptype->ResolveAlias()->kind)
    {
      if (!GenerateAnyValueAssignExpr(scope, vsfunc->vsresult->ll_value, value))
      {
        throw logic_error("Unsupported anyvalue return value");
      }
    }
    else
    {
      ll_value = value->Generate(scope);
      ll_builder.CreateStore(ll_value, vsfunc->vsresult->ll_value);
    }
  }

  EmitOwnedObjectDestructorsForReturn(scope, vsfunc);

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
  ll_builder.CreateCall(method->ll_func, ll_args);

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

  if (TK_DYNSTR == target->ResolvedType()->kind)
  {
    LlValue * ll_addr = target->GenerateAddress(scope);
    if (!GenerateStringAssignExpr(scope, ll_addr, value))
    {
      throw logic_error("Unsupported string assignment");
    }
    return;
  }

  if (auto * dyntype = dynamic_cast<OTypeDynArray *>(target->ResolvedType()))
  {
    LlValue * ll_addr = target->GenerateAddress(scope);
    if (!GenerateDynArrayAssignExpr(scope, dyntype, ll_addr, value))
    {
      throw logic_error("Unsupported dynamic array assignment");
    }
    return;
  }

  if (TK_ANYVALUE == target->ResolvedType()->kind)
  {
    LlValue * ll_addr = target->GenerateAddress(scope);
    if (!GenerateAnyValueAssignExpr(scope, ll_addr, value))
    {
      throw logic_error("Unsupported anyvalue assignment");
    }
    return;
  }

  if (TK_CSTRING == target->ResolvedType()->kind)
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
      root = root->GetBaseObject();
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

llvm::FunctionCallee get_unwind_raise_exception() {
  return ll_module->getOrInsertFunction("_Unwind_RaiseException", llvm::Type::getInt32Ty(ll_ctx), llvm::PointerType::get(ll_ctx, 0));
}

void OStmtTry::Generate(OScope * scope)
{
  llvm::Function * ll_func = ll_builder.GetInsertBlock()->getParent();

  ll_bb_dispatch = llvm::BasicBlock::Create(ll_ctx, "try.dispatch");
  ll_bb_endtry = llvm::BasicBlock::Create(ll_ctx, "try.end");
  if (finally_body)
  {
    ll_bb_finally = llvm::BasicBlock::Create(ll_ctx, "try.finally");
  }

  for (OExceptBranch * branch : except_branches)
  {
    branch->ll_bb_catch = llvm::BasicBlock::Create(ll_ctx, "try.catch");
  }

  ll_lpad_val_alloca = CreateEntryBlockAlloca(llvm::StructType::get(ll_ctx, {llvm::PointerType::get(ll_ctx, 0), llvm::Type::getInt32Ty(ll_ctx)}), nullptr, "lpad.val");
  try_scope = scope;

  OStmtTry * prev_try = ll_cg_try;
  ll_cg_try = this;

  body->Generate();

  ll_cg_try = prev_try;

  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    ll_builder.CreateBr(ll_bb_finally ? ll_bb_finally : ll_bb_endtry);
  }

  if (finally_body)
  {
    ll_func->insert(ll_func->end(), ll_bb_finally);
    ll_builder.SetInsertPoint(ll_bb_finally);
    finally_body->Generate();
    if (!ll_builder.GetInsertBlock()->getTerminator())
    {
      ll_builder.CreateBr(ll_bb_endtry);
    }
  }

  if (!except_branches.empty())
  {
    ll_func->insert(ll_func->end(), ll_bb_dispatch);
    ll_builder.SetInsertPoint(ll_bb_dispatch);

    llvm::Value * lpad_val = ll_builder.CreateLoad(llvm::StructType::get(ll_ctx, {llvm::PointerType::get(ll_ctx, 0), llvm::Type::getInt32Ty(ll_ctx)}), ll_lpad_val_alloca, "lpad.val.load");
    llvm::Value * exc_ptr = ll_builder.CreateExtractValue(lpad_val, 0, "exc.ptr");

    llvm::Value * obj_ptr_int = ll_builder.CreatePtrToInt(exc_ptr, llvm::Type::getInt64Ty(ll_ctx));
    llvm::Value * obj_ptr_int_adj = ll_builder.CreateSub(obj_ptr_int, llvm::ConstantInt::get(llvm::Type::getInt64Ty(ll_ctx), 8));
    llvm::Value * obj_ptr = ll_builder.CreateIntToPtr(obj_ptr_int_adj, llvm::PointerType::get(ll_ctx, 0), "obj.ptr");

    llvm::Value * exc_class_ptr = ll_builder.CreateGEP(llvm::Type::getInt64Ty(ll_ctx), ll_builder.CreatePointerCast(exc_ptr, llvm::PointerType::get(ll_ctx, 0)), {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ll_ctx), 0)});
    llvm::Value * exc_class = ll_builder.CreateLoad(llvm::Type::getInt64Ty(ll_ctx), exc_class_ptr);
    
    llvm::Value * is_dq_exc = ll_builder.CreateICmpEQ(exc_class, llvm::ConstantInt::get(llvm::Type::getInt64Ty(ll_ctx), 0x4451000000000000));
    
    llvm::BasicBlock * bb_is_dq = llvm::BasicBlock::Create(ll_ctx, "try.is_dq", ll_func);
    llvm::BasicBlock * bb_not_dq = llvm::BasicBlock::Create(ll_ctx, "try.not_dq", ll_func);
    ll_builder.CreateCondBr(is_dq_exc, bb_is_dq, bb_not_dq);
    
    ll_builder.SetInsertPoint(bb_not_dq);
    if (prev_try) {
        ll_builder.CreateStore(lpad_val, prev_try->ll_lpad_val_alloca);
        ll_builder.CreateBr(prev_try->ll_bb_dispatch);
    } else {
        auto resume_func = get_unwind_resume();
        ll_builder.CreateCall(resume_func.getFunctionType(), resume_func.getCallee(), {exc_ptr});
        ll_builder.CreateUnreachable();
    }
    
    ll_builder.SetInsertPoint(bb_is_dq);

    for (size_t i = 0; i < except_branches.size(); ++i) {
      OExceptBranch * branch = except_branches[i];
      if (branch->exception_type) {
         LlValue * expected_vtable = branch->exception_type->ll_vtable;
         if (!expected_vtable) {
             branch->exception_type->GenVTableGlobal(false);
             expected_vtable = branch->exception_type->ll_vtable;
         }
         
         if (expected_vtable) {
             LlValue * actual_vtable_ptr = ll_builder.CreateGEP(llvm::Type::getInt64Ty(ll_ctx), ll_builder.CreatePointerCast(obj_ptr, llvm::PointerType::get(ll_ctx, 0)), {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ll_ctx), 0)});
             // actual_vtable_ptr is i64*, we want to load the pointer
             LlValue * actual_vtable_ptr_cast = ll_builder.CreatePointerCast(actual_vtable_ptr, llvm::PointerType::get(llvm::PointerType::get(ll_ctx, 0), 0));
             LlValue * actual_vtable = ll_builder.CreateLoad(llvm::PointerType::get(ll_ctx, 0), actual_vtable_ptr_cast);
             
             LlValue * expected_vtable_cast = ll_builder.CreatePointerCast(expected_vtable, llvm::PointerType::get(ll_ctx, 0));
             LlValue * is_exact = ll_builder.CreateICmpEQ(actual_vtable, expected_vtable_cast);
             
             llvm::BasicBlock * bb_next = llvm::BasicBlock::Create(ll_ctx, "try.next_catch", ll_func);
             ll_builder.CreateCondBr(is_exact, branch->ll_bb_catch, bb_next);
             ll_builder.SetInsertPoint(bb_next);
         } else {
             // No vtable, just assume it matches (fallback for non-polymorphic Exception)
             ll_builder.CreateBr(branch->ll_bb_catch);
             llvm::BasicBlock * bb_unreachable = llvm::BasicBlock::Create(ll_ctx, "try.unreachable", ll_func);
             ll_builder.SetInsertPoint(bb_unreachable); // so further code generation goes into dead block
         }
      } else {
         ll_builder.CreateBr(branch->ll_bb_catch);
         break;
      }
    }
    
    if (prev_try) {
        ll_builder.CreateStore(lpad_val, prev_try->ll_lpad_val_alloca);
        ll_builder.CreateBr(prev_try->ll_bb_dispatch);
    } else {
        auto resume_func = get_unwind_resume();
        ll_builder.CreateCall(resume_func.getFunctionType(), resume_func.getCallee(), {exc_ptr});
        ll_builder.CreateUnreachable();
    }
    
    for (size_t i = 0; i < except_branches.size(); ++i) {
      OExceptBranch * branch = except_branches[i];
      ll_func->insert(ll_func->end(), branch->ll_bb_catch);
      ll_builder.SetInsertPoint(branch->ll_bb_catch);
      
      if (branch->bound_variable) {
          LlType * var_type = branch->bound_variable->GetStorageType()->GetLlType();
          LlValue * var_alloca = CreateEntryBlockAlloca(var_type, nullptr, branch->bound_variable->name);
          branch->bound_variable->ll_value = var_alloca;
          
          LlValue * obj_cast = ll_builder.CreatePointerCast(obj_ptr, var_type);
          ll_builder.CreateStore(obj_cast, var_alloca);
      }
      
      branch->body->Generate();
      
      if (!ll_builder.GetInsertBlock()->getTerminator()) {
        ll_builder.CreateBr(ll_bb_finally ? ll_bb_finally : ll_bb_endtry);
      }
    }
  }

  ll_func->insert(ll_func->end(), ll_bb_endtry);
  ll_builder.SetInsertPoint(ll_bb_endtry);
}

void OStmtRaise::Generate(OScope * scope)
{
  if (expr)
  {
    LlValue * obj_addr = expr->Generate(scope);
    LlValue * unwind_exc_addr = ll_builder.CreateGEP(llvm::Type::getInt64Ty(ll_ctx), ll_builder.CreatePointerCast(obj_addr, llvm::PointerType::get(ll_ctx, 0)), {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ll_ctx), 1)});
    
    ll_builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ll_ctx), 0x4451000000000000), unwind_exc_addr);
    
    LlValue * cleanup_addr = ll_builder.CreateGEP(llvm::Type::getInt64Ty(ll_ctx), ll_builder.CreatePointerCast(obj_addr, llvm::PointerType::get(ll_ctx, 0)), {llvm::ConstantInt::get(llvm::Type::getInt64Ty(ll_ctx), 2)});
    ll_builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(ll_ctx), 0), cleanup_addr);
    
    auto raise_func = get_unwind_raise_exception();
    ll_builder.CreateCall(raise_func.getFunctionType(), raise_func.getCallee(), {ll_builder.CreatePointerCast(unwind_exc_addr, llvm::PointerType::get(ll_ctx, 0))});
    ll_builder.CreateUnreachable();
  }
  else
  {
    // rethrow
    throw logic_error("OStmtRaise::Generate rethrow is not implemented yet");
  }
}
