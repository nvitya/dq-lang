/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    oscope.cpp
 * authors: nvitya
 * created: 2026-06-20
 * brief:   Scope implementation
 */

#include "symbols.h"
#include "dqc_base.h"
#include "otype_compound.h"
#include "otype_array.h"
#include "otype_cstring.h"
#include "otype_anyvalue.h"
#include "dqc.h"
#include "errorcodes.h"

using namespace std;

OType * OScope::DefineType(OType * atype)
{
  auto found = typesyms.find(atype->name);
  if (found != typesyms.end())
  {
    g_compiler->Error(DQERR_TYPE_ALREADY_DEFINED_IN, atype->name, this->debugname);
    return found->second;
  }

  typesyms[atype->name] = atype;
  return atype;
}

OValSym * OScope::DefineValSym(OValSym * avalsym)
{
  auto found = valsyms.find(avalsym->name);
  if (found != valsyms.end())
  {
    g_compiler->Error(DQERR_VS_ALREADY_DECL_SCOPE, avalsym->name, this->debugname);
    return found->second;
  }

  valsyms[avalsym->name] = avalsym;
  auto * objsym = dynamic_cast<OVsObject *>(avalsym);
  if ((objsym && objsym->IsFixedObjectStorage())
      || ((VSK_VARIABLE == avalsym->kind)
          && avalsym->ptype
          && (TK_DYN_ARRAY == avalsym->ptype->ResolveAlias()->kind)
          && ("result" != avalsym->name)))
  {
    owned_objects.push_back(avalsym);
  }
  else if (avalsym->ptype
           && (TK_DYNSTR == avalsym->ptype->ResolveAlias()->kind)
           && !avalsym->IsRefLike()
           && ("result" != avalsym->name))
  {
    owned_objects.push_back(avalsym);
  }
  else if (avalsym->ptype
           && (TK_ANYVALUE == avalsym->ptype->ResolveAlias()->kind)
           && !avalsym->IsRefLike()
           && ("result" != avalsym->name))
  {
    owned_objects.push_back(avalsym);
  }
  return avalsym;
}

OType * OScope::FindType(const string & name, OScope ** rscope, bool arecursive)
{
  auto it = typesyms.find(name);
  if (it != typesyms.end())
  {
    if (rscope)
    {
      *rscope = this;
    }
    return it->second;
  }

  // If not found here, check the parent scope
  if (arecursive and (parent_scope != nullptr))
  {
    return parent_scope->FindType(name, rscope);
  }

  return nullptr;
}

OValSym * OScope::FindValSym(const string & name, OScope ** rscope, bool arecursive)
{
  auto it = valsyms.find(name);
  if (it != valsyms.end())
  {
    if (rscope)
    {
      *rscope = this;
    }
    return it->second;
  }

  // If not found here, check the parent scope
  if (arecursive and vs_lookup_parent and (parent_scope != nullptr))
  {
    return parent_scope->FindValSym(name, rscope);
  }

  return nullptr;
}

void OScope::SetVarInitialized(OValSym * vs)
{
  if (not vs->initialized)
  {
    firstassign.push_back(vs);
    vs->initialized = true;
  }
}

void OScope::RevertFirstAssignments()
{
  for (OValSym * vs : firstassign)
  {
    vs->initialized = false;
  }
}

bool OScope::FirstAssigned(OValSym * avs)
{
  for (OValSym * vs : firstassign)
  {
    if (avs == vs)
    {
      return true;
    }
  }
  return false;
}
