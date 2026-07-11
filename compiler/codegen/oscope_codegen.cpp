/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    oscope_codegen.cpp
 * authors: nvitya
 * created: 2026-06-20
 * brief:   Scope codegen implementation
 */

#include "symbols.h"
#include "dqc_codegen.h"
#include "otype_array.h"
#include "otype_cstring.h"
#include "otype_anyvalue.h"
#include "otype_string.h"
#include "otype_func.h"
#include "otype_compound.h"
#include "statements.h"
#include "dqc.h"

using namespace std;

LlDiScope * OScope::GetDiScope()
{
  if (di_scope)
  {
    return di_scope;
  }

  if (parent_scope)
  {
    return parent_scope->GetDiScope();
  }

  return di_main_file;
}

void OScope::EmitOwnedObjectDestructors()
{
  for (auto it = owned_objects.rbegin(); it != owned_objects.rend(); ++it)
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
      GenerateDynArrayDestroy(this, dyntype, vs->ll_value);
    }
    auto * strtype = dynamic_cast<OTypeDynString *>(vs && vs->ptype ? vs->ptype->ResolveAlias() : nullptr);
    if (strtype && vs->ll_value)
    {
      GenerateStringDestroy(this, vs->ll_value);
    }
    auto * anytype = dynamic_cast<OTypeAnyValue *>(vs && vs->ptype ? vs->ptype->ResolveAlias() : nullptr);
    if (anytype && vs->ll_value)
    {
      GenerateAnyValueDestroy(this, vs->ll_value);
    }
  }
}

void OScope::EmitOwnedObjectDestructorsUntil(OScope * stop_scope)
{
  for (OScope * cur = this; cur; cur = cur->parent_scope)
  {
    cur->EmitOwnedObjectDestructors();
    if (cur == stop_scope)
    {
      break;
    }
  }
}

void OScope::EmitOwnedObjectDestructorsForReturn(OValSymFunc * vsfunc)
{
  OScope * stop_scope = (vsfunc && vsfunc->body ? vsfunc->body->scope : nullptr);
  EmitOwnedObjectDestructorsUntil(stop_scope);
}

LlBasicBlock * OScope::GetExceptionCleanupBB()
{
  for (OScope * cur = this; cur; cur = cur->parent_scope)
  {
    if (cur->exception_cleanup_bb)
    {
      return cur->exception_cleanup_bb;
    }
  }
  return nullptr;
}

LlValue * OScope::GenerateCallOrInvoke(LlFuncType * func_type, LlValue * callee, const std::vector<LlValue*> & args, const std::string & invoke_cont_name, LlBasicBlock * override_cleanup_bb)
{
  LlBasicBlock * bb_cleanup = override_cleanup_bb ? override_cleanup_bb : GetExceptionCleanupBB();

  if (bb_cleanup)
  {
    LlFunction * cur_func = ll_builder.GetInsertBlock()->getParent();
    g_compiler->EnsurePersonalityFn(cur_func);
    LlBasicBlock * bb_normal = LlBasicBlock::Create(ll_ctx, invoke_cont_name, cur_func);
    LlValue * result = ll_builder.CreateInvoke(func_type, callee, bb_normal, bb_cleanup, args);
    ll_builder.SetInsertPoint(bb_normal);
    return result;
  }

  return ll_builder.CreateCall(func_type, callee, args);
}
