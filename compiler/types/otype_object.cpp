/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_object.cpp
 * authors: nvitya
 * created: 2026-05-22
 * brief:   object value symbol helpers
 */

#include "otype_object.h"

#include <utility>

#include "expressions.h"
#include "otype_func.h"

OValSym * OCompoundType::CreateValSym(OScPosition & apos, const string aname)
{
  if (is_object)
  {
    return new OVsObject(apos, aname, this);
  }
  return OType::CreateValSym(apos, aname);
}

OValSym * OTypeAlias::CreateValSym(OScPosition & apos, const string aname)
{
  OCompoundType * ctype = dynamic_cast<OCompoundType *>(ResolveAlias());
  if (ctype && ctype->is_object)
  {
    return new OVsObject(apos, aname, this);
  }
  return OType::CreateValSym(apos, aname);
}

OValSymFunc * OCompoundType::FindSpecialMethod(EObjectSpecFuncKind akind, size_t auser_arg_count) const
{
  if (OSF_CREATE == akind)
  {
    for (OValSymFunc * ctor : constructors)
    {
      OTypeFunc * sig = dynamic_cast<OTypeFunc *>(ctor ? ctor->ptype : nullptr);
      if (!sig || sig->params.empty())
      {
        continue;
      }
      size_t user_params = sig->params.size() - 1; // hidden __this
      if ((size_t(-1) == auser_arg_count) || (user_params == auser_arg_count))
      {
        return ctor;
      }
    }
    return nullptr;
  }

  if (OSF_DESTROY == akind)
  {
    return destructor;
  }

  return nullptr;
}

OVsObject::~OVsObject()
{
  for (OExpr *& arg : object_ctor_args)
  {
    OExpr::DeleteTree(arg);
    arg = nullptr;
  }
  object_ctor_args.clear();
}

void OVsObject::SetObjectCtorArgs(vector<OExpr *> aargs)
{
  for (OExpr *& arg : object_ctor_args)
  {
    OExpr::DeleteTree(arg);
    arg = nullptr;
  }
  object_ctor_args = std::move(aargs);
}

OType * OVsObject::GetStorageType() const
{
  return ((IsRefLike() || IsObjectReference()) ? ptype->GetPointerType() : ptype);
}

OCompoundType * OVsObject::ObjectType() const
{
  return dynamic_cast<OCompoundType *>(ptype ? ptype->ResolveAlias() : nullptr);
}

OValSymFunc * OVsObject::FindConstructor() const
{
  OCompoundType * ctype = ObjectType();
  return (ctype ? ctype->FindSpecialMethod(OSF_CREATE, object_ctor_args.size()) : nullptr);
}

OValSymFunc * OVsObject::FindDestructor() const
{
  OCompoundType * ctype = ObjectType();
  return (ctype ? ctype->FindSpecialMethod(OSF_DESTROY) : nullptr);
}

void OVsObject::GenerateConstructorCall(OScope * scope, LlValue * ll_object_addr) const
{
  if (!object_ctor_call_at_decl)
  {
    return;
  }

  OValSymFunc * ctor = FindConstructor();
  if (!ctor || !ctor->ll_func)
  {
    return;
  }

  vector<LlValue *> ll_args;
  ll_args.push_back(ll_object_addr);
  for (OExpr * arg : object_ctor_args)
  {
    ll_args.push_back(arg->Generate(scope));
  }
  ll_builder.CreateCall(ctor->ll_func, ll_args);
}

void OVsObject::GenerateDestructorCall(LlValue * ll_object_addr) const
{
  OValSymFunc * dtor = FindDestructor();
  if (dtor && dtor->ll_func)
  {
    ll_builder.CreateCall(dtor->ll_func, {ll_object_addr});
  }
}
