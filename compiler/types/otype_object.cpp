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
 * brief:   object type and value symbol helpers
 */

#include "otype_object.h"

#include <limits>
#include <utility>

#include "dqm_if.h"
#include "dqc.h"
#include "expressions.h"
#include "otype_func.h"
#include <llvm/IR/GlobalVariable.h>

OValSym * OTypeObject::CreateValSym(OScPosition & apos, const string aname)
{
  return new OVsObject(apos, aname, this);
}

OValSym * OTypeAlias::CreateValSym(OScPosition & apos, const string aname)
{
  if (dynamic_cast<OTypeObject *>(ResolveAlias()))
  {
    return new OVsObject(apos, aname, this);
  }
  return OType::CreateValSym(apos, aname);
}

bool OTypeObject::IsSameOrDerivedFrom(OCompoundType * abase) const
{
  if (!abase)
  {
    return false;
  }

  for (const OTypeObject * cur = this; cur; cur = cur->GetBaseObject())
  {
    if (cur == abase)
    {
      return true;
    }
  }
  return false;
}

bool OTypeObject::HasTrivialDefaultConstructor() const
{
  return constructors.empty();
}

OValSymFunc * OTypeObject::FindSpecialMethod(EObjectSpecFuncKind akind, size_t auser_arg_count) const
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

OValSymFunc * OTypeObject::FindConstructorForArgs(const vector<OExpr *> & aargs, bool * rambiguous) const
{
  if (rambiguous)
  {
    *rambiguous = false;
  }

  OValSymFunc * best_func = nullptr;
  TFuncCallMatchScore best_score;
  bool ambiguous = false;

  for (OValSymFunc * ctor : constructors)
  {
    OTypeFunc * sig = dynamic_cast<OTypeFunc *>(ctor ? ctor->ptype : nullptr);
    if (!sig || sig->params.empty())
    {
      continue;
    }

    size_t hidden_this_count = 1;
    size_t required_user_count = sig->RequiredParamCount();
    if (required_user_count > 0)
    {
      required_user_count -= hidden_this_count;
    }

    if (aargs.size() < required_user_count)
    {
      continue;
    }
    if (!sig->has_varargs && (aargs.size() + hidden_this_count > sig->params.size()))
    {
      continue;
    }

    TFuncCallMatchScore score;
    bool match = true;
    for (size_t i = 0; i < aargs.size(); ++i)
    {
      OExpr * arg = aargs[i];
      if (!arg)
      {
        match = false;
        break;
      }

      size_t param_index = i + hidden_this_count;
      if (param_index >= sig->params.size())
      {
        score.uses_varargs = true;
        continue;
      }

      OFuncParam * fparam = sig->params[param_index];
      if (!fparam)
      {
        match = false;
        break;
      }

      if (!fparam->IsRefLike())
      {
        int conv_cost = g_compiler->GetAssignTypeConversionCost(
            fparam->ptype, arg, EXPCF_ALLOW_LAZY_CSTRING | EXPCF_ALLOW_ARRAY_LITERAL_SLICE);
        if (conv_cost < 0)
        {
          match = false;
          break;
        }
        score.conversions += conv_cost;
        continue;
      }

      if (dynamic_cast<ONullLit *>(arg))
      {
        if (FPM_REFNULL != fparam->mode)
        {
          match = false;
          break;
        }
        continue;
      }

      OLValueExpr * arglval = dynamic_cast<OLValueExpr *>(arg);
      OValSym * rootvalsym = (arglval ? g_compiler->GetAssignRootValSym(arglval) : nullptr);
      bool bind_ok = (arglval != nullptr);
      if (bind_ok && rootvalsym)
      {
        bind_ok = ((VSK_CONST != rootvalsym->kind) && rootvalsym->IsRefWriteable());
      }
      if (!bind_ok || !OTypeFunc::SameRefBindingType(fparam->ptype, arg->ptype))
      {
        match = false;
        break;
      }
    }

    if (!match)
    {
      continue;
    }

    for (size_t i = aargs.size() + hidden_this_count; i < sig->params.size(); ++i)
    {
      if (!sig->params[i]->defvalue)
      {
        match = false;
        break;
      }
      ++score.defaults;
    }

    if (!match)
    {
      continue;
    }

    if (!best_func)
    {
      best_func = ctor;
      best_score = score;
      ambiguous = false;
      continue;
    }

    int cmp = OTypeFunc::CompareCallCandidateScore(score, best_score);
    if (cmp < 0)
    {
      best_func = ctor;
      best_score = score;
      ambiguous = false;
    }
    else if (0 == cmp)
    {
      ambiguous = true;
    }
  }

  if (rambiguous)
  {
    *rambiguous = ambiguous;
  }
  return (ambiguous ? nullptr : best_func);
}

OValSym * OTypeObject::FindObjectMemberSymbol(const string & aname, OCompoundType ** rdecl_type) const
{
  for (const OTypeObject * cur = this; cur; cur = cur->GetBaseObject())
  {
    OValSym * vs = const_cast<OTypeObject *>(cur)->member_scope.FindValSym(aname, nullptr, false);
    if (vs)
    {
      if (rdecl_type)
      {
        *rdecl_type = const_cast<OTypeObject *>(cur);
      }
      return vs;
    }
  }
  return nullptr;
}

OValSym * OTypeObject::FindMemberSymbol(const string & aname, OCompoundType ** rdecl_type) const
{
  return FindObjectMemberSymbol(aname, rdecl_type);
}

int OTypeObject::FindObjectFieldIndex(const string & aname, OCompoundType ** rdecl_type) const
{
  for (const OTypeObject * cur = this; cur; cur = cur->GetBaseObject())
  {
    int idx = const_cast<OTypeObject *>(cur)->FindMemberIndex(aname);
    if (idx >= 0)
    {
      if (rdecl_type)
      {
        *rdecl_type = const_cast<OTypeObject *>(cur);
      }
      return idx;
    }
  }
  return -1;
}

int OTypeObject::FindFieldIndex(const string & aname, OCompoundType ** rdecl_type) const
{
  return FindObjectFieldIndex(aname, rdecl_type);
}

OValSymFunc * OTypeObject::FindVirtualBaseMethod(OValSymFunc * afunc, OCompoundType ** rdecl_type) const
{
  OTypeFunc * fsig = dynamic_cast<OTypeFunc *>(afunc ? afunc->ptype : nullptr);
  if (!fsig)
  {
    return nullptr;
  }

  auto matches_method_signature = [fsig](OValSymFunc * method) -> bool
  {
    OTypeFunc * msig = dynamic_cast<OTypeFunc *>(method ? method->ptype : nullptr);
    if (!msig || !method->attr_is_virtual)
    {
      return false;
    }
    if (fsig->has_varargs != msig->has_varargs
        || fsig->ResolvedRetType() != msig->ResolvedRetType()
        || fsig->params.size() != msig->params.size())
    {
      return false;
    }
    for (size_t i = 1; i < fsig->params.size(); ++i)
    {
      OFuncParam * left = fsig->params[i];
      OFuncParam * right = msig->params[i];
      if (!left || !right || left->mode != right->mode || !left->ptype || !right->ptype
          || left->ptype->ResolveAlias() != right->ptype->ResolveAlias())
      {
        return false;
      }
    }
    return true;
  };

  for (OTypeObject * cur = GetBaseObject(); cur; cur = cur->GetBaseObject())
  {
    OValSym * vs = cur->member_scope.FindValSym(afunc->name, nullptr, false);
    if (auto * method = dynamic_cast<OValSymFunc *>(vs))
    {
      if (matches_method_signature(method))
      {
        if (rdecl_type)
        {
          *rdecl_type = cur;
        }
        return method;
      }
    }
    else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
    {
      for (OValSymFunc * method : ovset->funcs)
      {
        if (matches_method_signature(method))
        {
          if (rdecl_type)
          {
            *rdecl_type = cur;
          }
          return method;
        }
      }
    }
  }

  return nullptr;
}

int OTypeObject::FindVirtualSlot(OValSymFunc * afunc) const
{
  if (!afunc)
  {
    return -1;
  }
  OValSymFunc * base_virtual = afunc;
  if (afunc->owner_compound_type == this && afunc->attr_is_override)
  {
    base_virtual = FindVirtualBaseMethod(afunc);
  }

  for (size_t i = 0; i < virtual_methods.size(); ++i)
  {
    OValSymFunc * slot_func = virtual_methods[i];
    if (slot_func == afunc || slot_func == base_virtual)
    {
      return int(i);
    }
    auto * slot_owner = dynamic_cast<OTypeObject *>(slot_func ? slot_func->owner_compound_type : nullptr);
    if (slot_owner && base_virtual && slot_owner->FindVirtualBaseMethod(slot_func) == base_virtual)
    {
      return int(i);
    }
  }
  return -1;
}

void OTypeObject::UpdateObjectInheritanceFlags()
{
  virtual_methods.clear();
  if (GetBaseObject())
  {
    virtual_methods = GetBaseObject()->virtual_methods;
  }

  is_polymorphic = (base_type && base_type->is_polymorphic);
  is_abstract = false;

  for (auto & [name, vs] : member_scope.valsyms)
  {
    (void)name;
    if (auto * fn = dynamic_cast<OValSymFunc *>(vs))
    {
      if (fn->attr_is_virtual || fn->attr_is_override)
      {
        is_polymorphic = true;
        if (fn->attr_is_override)
        {
          OValSymFunc * base_virtual = FindVirtualBaseMethod(fn);
          int slot = FindVirtualSlot(base_virtual);
          if (slot >= 0)
          {
            virtual_methods[size_t(slot)] = fn;
          }
        }
        else
        {
          virtual_methods.push_back(fn);
        }
      }
      if (fn->attr_is_abstract)
      {
        is_abstract = true;
      }
    }
    else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
    {
      for (OValSymFunc * fn : ovset->funcs)
      {
        if (!fn)
        {
          continue;
        }
        if (fn->attr_is_virtual || fn->attr_is_override)
        {
          is_polymorphic = true;
          if (fn->attr_is_override)
          {
            OValSymFunc * base_virtual = FindVirtualBaseMethod(fn);
            int slot = FindVirtualSlot(base_virtual);
            if (slot >= 0)
            {
              virtual_methods[size_t(slot)] = fn;
            }
          }
          else
          {
            virtual_methods.push_back(fn);
          }
        }
        if (fn->attr_is_abstract)
        {
          is_abstract = true;
        }
      }
    }
  }

  for (OValSymFunc * fn : virtual_methods)
  {
    if (fn && fn->attr_is_abstract)
    {
      is_abstract = true;
      break;
    }
  }
}

void OTypeObject::GenVTableGlobal(bool apublic)
{
  if (!is_polymorphic || ll_vtable)
  {
    return;
  }

  vector<llvm::Constant *> entries;
  LlType * ptr_type = llvm::PointerType::get(ll_ctx, 0);
  if (destructor && destructor->ll_func)
  {
    entries.push_back(destructor->ll_func);
  }
  else
  {
    entries.push_back(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)));
  }
  for (OValSymFunc * method : virtual_methods)
  {
    if (method && method->ll_func)
    {
      entries.push_back(method->ll_func);
    }
    else
    {
      entries.push_back(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)));
    }
  }

  auto * arrtype = llvm::ArrayType::get(ptr_type, entries.size());
  auto * init = llvm::ConstantArray::get(arrtype, entries);
  LlLinkType linktype = (apublic ? llvm::GlobalValue::ExternalLinkage
                                 : llvm::GlobalValue::InternalLinkage);
  string ll_name = "_DQVT_" + name;
  ll_vtable = new llvm::GlobalVariable(*ll_module, arrtype, true, linktype, init, ll_name);
}

void OTypeObject::GenerateVTableStore(LlValue * ll_object_addr)
{
  if (!is_polymorphic || !ll_object_addr)
  {
    return;
  }
  if (!ll_vtable)
  {
    GenVTableGlobal(false);
  }

  OTypeObject * root = this;
  while (root->GetBaseObject())
  {
    root = root->GetBaseObject();
  }
  root->GetLlType();
  LlValue * ll_vptr_addr = ll_builder.CreateStructGEP(root->GetLlType(), ll_object_addr,
      root->vtable_field_index, "vtable.addr");
  LlValue * ll_zero = llvm::ConstantInt::get(LlType::getInt64Ty(ll_ctx), 0);
  LlValue * ll_vtable_ptr = ll_builder.CreateGEP(
      static_cast<llvm::GlobalVariable *>(ll_vtable)->getValueType(),
      ll_vtable, {ll_zero, ll_zero}, "vtable.ptr");
  ll_builder.CreateStore(ll_vtable_ptr, ll_vptr_addr);
}

LlValue * OTypeObject::GenerateConversion(OScope * scope, OExpr * src)
{
  OType * srctype = src ? src->ResolvedType() : nullptr;
  if (!srctype)
  {
    throw logic_error("Object conversion requires a source type");
  }

  if (TK_POINTER == srctype->kind)
  {
    LlValue * ll_value = src->Generate(scope);
    LlType * ll_objptr = GetPointerType()->GetLlType();
    if (ll_value->getType() == ll_objptr)
    {
      return ll_value;
    }
    return ll_builder.CreateBitCast(ll_value, ll_objptr);
  }

  throw logic_error(format("Unsupported object conversion from \"{}\"", src->ptype->name));
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

OTypeObject * OVsObject::ObjectType() const
{
  return dynamic_cast<OTypeObject *>(ptype ? ptype->ResolveAlias() : nullptr);
}

OValSymFunc * OVsObject::FindConstructor() const
{
  OTypeObject * object_type = ObjectType();
  return (object_type ? object_type->FindConstructorForArgs(object_ctor_args) : nullptr);
}

OValSymFunc * OVsObject::FindDestructor() const
{
  OTypeObject * object_type = ObjectType();
  return (object_type ? object_type->FindSpecialMethod(OSF_DESTROY) : nullptr);
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
