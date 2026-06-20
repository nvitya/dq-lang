/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
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
