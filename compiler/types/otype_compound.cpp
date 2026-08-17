/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_compound.cpp
 * authors: nvitya
 * created: 2026-05-22
 * brief:   object type and value symbol helpers
 */

#include "otype_compound.h"
#include "dqc_ast.h"

#include <limits>
#include <utility>

#include "dqm_if.h"
#include "dqc.h"
#include "expressions.h"
#include "otype_func.h"
#include "otype_array.h"
#include "otype_string.h"
#include "otype_anyvalue.h"
#include "scope_builtins.h"
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/ConstantFold.h>

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

  if (g_builtins && abase == g_builtins->type_object)
  {
    return true;
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

static bool ConstructorUserSignaturesMatch(OValSymFunc * left, OValSymFunc * right)
{
  auto * lsig = dynamic_cast<OTypeFunc *>(left ? left->ptype : nullptr);
  auto * rsig = dynamic_cast<OTypeFunc *>(right ? right->ptype : nullptr);
  if (!lsig || !rsig || lsig->params.empty() || rsig->params.empty())
  {
    return false;
  }

  if (lsig->has_varargs || rsig->has_varargs || lsig->params.size() != rsig->params.size())
  {
    return false;
  }

  for (size_t i = 1; i < lsig->params.size(); ++i)
  {
    OFuncParam * lp = lsig->params[i];
    OFuncParam * rp = rsig->params[i];
    if (!lp || !rp || lp->mode != rp->mode)
    {
      return false;
    }
    if ((lp->ptype ? lp->ptype->ResolveAlias() : nullptr) != (rp->ptype ? rp->ptype->ResolveAlias() : nullptr))
    {
      return false;
    }
  }
  return true;
}

OValSymFunc * OTypeObject::FindConstructorMatchingSignature(OValSymFunc * acontract_ctor) const
{
  if (!acontract_ctor)
  {
    return FindSpecialMethod(OSF_CREATE, 0);
  }

  for (OValSymFunc * ctor : constructors)
  {
    if (ConstructorUserSignaturesMatch(acontract_ctor, ctor))
    {
      return ctor;
    }
  }
  return nullptr;
}

bool OTypeObject::SupportsConstructorContractFrom(OTypeObject * abase) const
{
  if (!abase || !IsSameOrDerivedFrom(abase))
  {
    return false;
  }

  for (OValSymFunc * contract_ctor : abase->ConstructorContractSlots())
  {
    if (!contract_ctor)
    {
      if (!HasTrivialDefaultConstructor() && !FindConstructorMatchingSignature(nullptr))
      {
        return false;
      }
      continue;
    }
    if (!FindConstructorMatchingSignature(contract_ctor))
    {
      return false;
    }
  }
  return true;
}

vector<OValSymFunc *> OTypeObject::ConstructorContractSlots() const
{
  vector<OValSymFunc *> result;
  if (auto * base_obj = GetBaseObject())
  {
    for (OValSymFunc * base_contract : base_obj->ConstructorContractSlots())
    {
      result.push_back(FindConstructorMatchingSignature(base_contract));
    }
  }

  bool has_default_contract = false;
  for (OValSymFunc * ctor : constructors)
  {
    OTypeFunc * sig = dynamic_cast<OTypeFunc *>(ctor ? ctor->ptype : nullptr);
    if (sig && sig->params.size() == 1)
    {
      has_default_contract = true;
      break;
    }
  }
  if (HasTrivialDefaultConstructor() && !has_default_contract)
  {
    result.push_back(nullptr);
  }

  for (OValSymFunc * ctor : constructors)
  {
    bool inherited_slot = false;
    for (OValSymFunc * existing : result)
    {
      if ((existing == ctor) || (existing && ConstructorUserSignaturesMatch(existing, ctor)))
      {
        inherited_slot = true;
        break;
      }
    }
    if (!inherited_slot)
    {
      result.push_back(ctor);
    }
  }
  return result;
}

int OTypeObject::FindConstructorContractSlot(OValSymFunc * acontract_ctor) const
{
  vector<OValSymFunc *> slots = ConstructorContractSlots();
  for (size_t i = 0; i < slots.size(); ++i)
  {
    OValSymFunc * slot_ctor = slots[i];
    if (!acontract_ctor)
    {
      if (!slot_ctor)
      {
        return int(i);
      }
      OTypeFunc * sig = dynamic_cast<OTypeFunc *>(slot_ctor->ptype);
      if (sig && sig->params.size() == 1)
      {
        return int(i);
      }
      continue;
    }
    if (slot_ctor && ConstructorUserSignaturesMatch(acontract_ctor, slot_ctor))
    {
      return int(i);
    }
  }
  return -1;
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
  EnsureLayout();

  if (ll_typeinfo && (!is_polymorphic || ll_vtable))
  {
    return;
  }

  llvm::PointerType * ptr_type = llvm::PointerType::get(ll_ctx, 0);

  if (manual_ll_layout)
  {
    // Imported type: do not emit local definitions, just use external references
    if (!ll_typeinfo)
    {
      auto * ti_arrtype = llvm::ArrayType::get(ptr_type, 3 + ConstructorContractSlots().size());
      string ti_ll_name = "_DQTI_" + name;
      ll_typeinfo = new llvm::GlobalVariable(*ll_module, ti_arrtype, false, llvm::GlobalValue::ExternalLinkage, nullptr, ti_ll_name);
    }

    if (is_polymorphic && !ll_vtable)
    {
      auto * vt_arrtype = llvm::ArrayType::get(ptr_type, virtual_methods.size() + 2);
      string ll_name = "_DQVT_" + name;
      ll_vtable = new llvm::GlobalVariable(*ll_module, vt_arrtype, false, llvm::GlobalValue::ExternalLinkage, nullptr, ll_name);
    }
    return;
  }


  LlLinkType linktype = (apublic ? llvm::GlobalValue::ExternalLinkage
                                 : llvm::GlobalValue::InternalLinkage);

  // Generate TypeInfo
  if (!ll_typeinfo)
  {
    vector<llvm::Constant *> typeinfo_entries;
    auto * str_init = llvm::ConstantDataArray::getString(ll_ctx, name);
    auto * str_gv = new llvm::GlobalVariable(*ll_module, str_init->getType(), true, llvm::GlobalValue::PrivateLinkage, str_init, ".str.ti." + name);
    typeinfo_entries.push_back(str_gv);

    if (base_type)
    {
      OTypeObject * base_obj = static_cast<OTypeObject *>(base_type);
      if (!base_obj->ll_typeinfo)
      {
        base_obj->GenVTableGlobal(false);
      }
      typeinfo_entries.push_back(base_obj->ll_typeinfo);
    }
    else
    {
      typeinfo_entries.push_back(llvm::ConstantPointerNull::get(ptr_type));
    }

    typeinfo_entries.push_back(llvm::ConstantExpr::getIntToPtr(
        llvm::ConstantInt::get(g_builtins->native_uint->GetLlType(), bytesize), ptr_type));

    for (OValSymFunc * ctor : ConstructorContractSlots())
    {
      typeinfo_entries.push_back((ctor && ctor->ll_func)
          ? static_cast<llvm::Constant *>(ctor->ll_func)
          : static_cast<llvm::Constant *>(llvm::ConstantPointerNull::get(ptr_type)));
    }

    auto * ti_arrtype = llvm::ArrayType::get(ptr_type, typeinfo_entries.size());
    auto * ti_init = llvm::ConstantArray::get(ti_arrtype, typeinfo_entries);
    string ti_ll_name = "_DQTI_" + name;
    ll_typeinfo = new llvm::GlobalVariable(*ll_module, ti_arrtype, true, linktype, ti_init, ti_ll_name);
  }

  if (!is_polymorphic || ll_vtable)
  {
    return;
  }

  vector<llvm::Constant *> entries;
  entries.push_back(ll_typeinfo);

  if (destructor && destructor->ll_func)
  {
    entries.push_back(destructor->ll_func);
  }
  else
  {
    entries.push_back(llvm::ConstantPointerNull::get(ptr_type));
  }
  for (OValSymFunc * method : virtual_methods)
  {
    if (method && method->attr_is_abstract)
    {
      entries.push_back(llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0)));
    }
    else if (method && method->ll_func)
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

void OTypeObject::GenerateFieldInitializers(OScope * scope, LlValue * ll_object_addr)
{
  GetLlType();
  for (OValSym * member : member_order)
  {
    if (!member) continue;

    LlValue * ll_field_addr = ll_builder.CreateStructGEP(GetLlType(), ll_object_addr,
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
}

void OTypeObject::GenerateFieldDestructors(OScope * scope, LlValue * ll_object_addr)
{
  GetLlType();
  for (auto it = member_order.rbegin(); it != member_order.rend(); ++it)
  {
    OValSym * member = *it;
    LlValue * ll_field_addr = ll_builder.CreateStructGEP(GetLlType(), ll_object_addr,
        member->ll_field_index, member->name + ".addr");

    if (auto * objmember = dynamic_cast<OVsObject *>(member); objmember && objmember->IsFixedObjectStorage())
    {
      objmember->GenerateDestructorCall(ll_field_addr);
      continue;
    }

    if (auto * dyntype = dynamic_cast<OTypeDynArray *>(member->ptype ? member->ptype->ResolveAlias() : nullptr))
    {
      GenerateDynArrayDestroy(scope, dyntype, ll_field_addr);
      continue;
    }

    if (auto * strtype = dynamic_cast<OTypeDynString *>(member->ptype ? member->ptype->ResolveAlias() : nullptr))
    {
      GenerateStringDestroy(scope, ll_field_addr);
      continue;
    }

    if (auto * anytype = dynamic_cast<OTypeAnyValue *>(member->ptype ? member->ptype->ResolveAlias() : nullptr))
    {
      GenerateAnyValueDestroy(scope, ll_field_addr);
      continue;
    }
  }
}

LlValue * OTypeObject::GenerateConversion(OScope * scope, OExpr * src)
{
  OType * srctype = src ? src->ResolvedType() : nullptr;
  if (!srctype)
  {
    throw logic_error("Object conversion requires a source type");
  }

  if (TK_POINTER == srctype->kind || TK_OBJECT == srctype->kind)
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


void OCompoundType::AddMember(OValSym * amember)
{
  member_scope.DefineValSym(amember);
  member_order.push_back(amember);
  layout_ready = false;
}

void OCompoundType::AddProperty(OValSymProperty * aproperty)
{
  if (member_scope.DefineValSym(aproperty) == aproperty)
  {
    properties.push_back(aproperty);
  }
  else
  {
    delete aproperty;
  }
}

OValSymProperty * OCompoundType::FindDefaultProperty(OCompoundType ** rdecl_type) const
{
  for (const OCompoundType * cur = this; cur; cur = cur->base_type)
  {
    for (auto it = cur->properties.rbegin(); it != cur->properties.rend(); ++it)
    {
      if ((*it)->is_default)
      {
        if (rdecl_type)
        {
          *rdecl_type = const_cast<OCompoundType *>(cur);
        }
        return *it;
      }
    }
  }
  return nullptr;
}

OValSym * OCompoundType::CreateValSym(OScPosition & apos, const string aname)
{
  return OType::CreateValSym(apos, aname);
}

int OCompoundType::FindMemberIndex(const string & aname)
{
  for (int i = 0; i < (int)member_order.size(); ++i)
  {
    if (member_order[i]->name == aname)  return i;
  }
  return -1;
}

OValSym * OCompoundType::FindMemberSymbol(const string & aname, OCompoundType ** rdecl_type) const
{
  for (const OCompoundType * cur = this; cur; cur = cur->base_type)
  {
    OValSym * vs = const_cast<OCompoundType *>(cur)->member_scope.FindValSym(aname, nullptr, false);
    if (vs)
    {
      if (rdecl_type)
      {
        *rdecl_type = const_cast<OCompoundType *>(cur);
      }
      return vs;
    }
  }
  return nullptr;
}

int OCompoundType::FindFieldIndex(const string & aname, OCompoundType ** rdecl_type) const
{
  for (const OCompoundType * cur = this; cur; cur = cur->base_type)
  {
    int idx = const_cast<OCompoundType *>(cur)->FindMemberIndex(aname);
    if (idx >= 0)
    {
      if (rdecl_type)
      {
        *rdecl_type = const_cast<OCompoundType *>(cur);
      }
      return idx;
    }
  }
  return -1;
}

bool OCompoundType::IsSameOrDerivedFrom(OCompoundType * abase) const
{
  if (!abase)
  {
    return false;
  }

  for (const OCompoundType * cur = this; cur; cur = cur->base_type)
  {
    if (cur == abase)
    {
      return true;
    }
  }
  return false;
}

bool OCompoundType::ContainsManagedStorage() const
{
  if (OType::ContainsManagedStorage())
  {
    return true;
  }
  if (base_type && base_type->ContainsManagedStorage())
  {
    return true;
  }
  for (OValSym * member : member_order)
  {
    OType * storage_type = (member ? member->GetStorageType() : nullptr);
    if (storage_type && storage_type->ContainsManagedStorage())
    {
      return true;
    }
  }
  return false;
}

void OCompoundType::EnsureLayout()
{
  if (layout_ready)
  {
    return;
  }
  if (layout_busy)
  {
    throw logic_error(format("Recursive compound layout is not supported: {}", name));
  }

  layout_busy = true;

  if (IsUnion())
  {
    uint32_t max_size = 0;
    uint32_t max_align = 1;
    for (OValSym * member : member_order)
    {
      if (!member || !member->ptype)
      {
        continue;
      }
      OType * storage_type = member->GetStorageType();
      storage_type->EnsureLayout();
      member->field_offset = 0;
      max_size = max(max_size, storage_type->bytesize);
      max_align = max(max_align, EffectiveStorageAlign(storage_type, member->attr_align));
    }
    alignsize = max_align;
    bytesize = AlignUpU32(max_size, alignsize);
    manual_ll_layout = true;
    layout_ready = true;
    layout_busy = false;
    return;
  }

  uint32_t offset = 0;
  uint32_t max_align = 1;
  manual_ll_layout = is_packed;

  if (base_type)
  {
    base_type->EnsureLayout();
    offset = base_type->bytesize;
    max_align = max<uint32_t>(max_align, base_type->alignsize);
  }
  else if (is_polymorphic)
  {
    offset = TARGET_PTRSIZE;
    max_align = max<uint32_t>(max_align, TARGET_PTRSIZE);
  }

  for (OValSym * m : member_order)
  {
    if (!m || !m->ptype)
    {
      continue;
    }

    OType * storage_type = m->GetStorageType();
    storage_type->EnsureLayout();

    uint32_t field_align = 1;
    if (!is_packed)
    {
      field_align = max<uint32_t>(storage_type->alignsize, 1);
      if (m->attr_align)
      {
        if (m->attr_align > storage_type->alignsize)
        {
          manual_ll_layout = true;
        }
        field_align = max(field_align, m->attr_align);
      }
      offset = AlignUpU32(offset, field_align);
      max_align = max(max_align, field_align);
    }

    m->field_offset = offset;
    offset += storage_type->bytesize;
  }

  alignsize = (is_packed ? 1 : max_align);
  bytesize = (is_packed ? offset : AlignUpU32(offset, alignsize));

  layout_ready = true;
  layout_busy = false;
}

LlType * OCompoundType::CreateLlType()
{
  EnsureLayout();

  if (IsUnion())
  {
    // LLVM has no union type. Use the naturally most-aligned member as the
    // storage anchor, followed by padding to the union's complete size. With
    // opaque pointers every source member can still address these same bytes.
    OValSym * anchor = nullptr;
    for (OValSym * member : member_order)
    {
      if (!anchor || EffectiveStorageAlign(member->GetStorageType(), member->attr_align)
          > EffectiveStorageAlign(anchor->GetStorageType(), anchor->attr_align))
      {
        anchor = member;
      }
      member->ll_field_index = 0;
    }

    vector<LlType *> storage;
    uint32_t stored_size = 0;
    if (anchor)
    {
      storage.push_back(anchor->GetStorageType()->GetLlType());
      stored_size = anchor->GetStorageType()->bytesize;
    }
    if (bytesize > stored_size)
    {
      storage.push_back(llvm::ArrayType::get(LlType::getInt8Ty(ll_ctx), bytesize - stored_size));
    }
    return llvm::StructType::create(ll_ctx, storage, name);
  }

  vector<LlType *> member_types;
  if (!manual_ll_layout)
  {
    uint32_t ll_index_base = 0;
    if (base_type)
    {
      member_types.push_back(base_type->GetLlType());
      ll_index_base = 1;
    }
    else if (is_polymorphic)
    {
      vtable_field_index = 0;
      member_types.push_back(llvm::PointerType::get(ll_ctx, 0));
      ll_index_base = 1;
    }
    for (int i = 0; i < (int)member_order.size(); ++i)
    {
      OValSym * m = member_order[i];
      m->ll_field_index = ll_index_base + uint32_t(i);
      member_types.push_back(m->GetStorageType()->GetLlType());
    }
    return llvm::StructType::create(ll_ctx, member_types, name);
  }

  uint32_t offset = 0;
  if (base_type)
  {
    member_types.push_back(base_type->GetLlType());
    offset = base_type->bytesize;
  }
  else if (is_polymorphic)
  {
    vtable_field_index = uint32_t(member_types.size());
    member_types.push_back(llvm::PointerType::get(ll_ctx, 0));
    offset = TARGET_PTRSIZE;
  }
  for (OValSym * m : member_order)
  {
    if (m->field_offset > offset)
    {
      member_types.push_back(llvm::ArrayType::get(LlType::getInt8Ty(ll_ctx), m->field_offset - offset));
      offset = m->field_offset;
    }

    m->ll_field_index = uint32_t(member_types.size());
    member_types.push_back(m->GetStorageType()->GetLlType());
    offset += m->GetStorageType()->bytesize;
  }

  if (bytesize > offset)
  {
    member_types.push_back(llvm::ArrayType::get(LlType::getInt8Ty(ll_ctx), bytesize - offset));
  }

  return llvm::StructType::create(ll_ctx, member_types, name, true);
}

LlDiType * OCompoundType::CreateDiType()
{
  EnsureLayout();
  uint64_t total_bits = uint64_t(bytesize) * 8;

  llvm::DICompositeType * di_compound_type = di_builder->createReplaceableCompositeType(
      IsUnion() ? llvm::dwarf::DW_TAG_union_type : llvm::dwarf::DW_TAG_structure_type,
      name, nullptr, nullptr, 0,
      0, total_bits, alignsize * 8, llvm::DINode::FlagZero);
  di_type = di_compound_type;

  vector<llvm::Metadata *> elements;
  if (base_type)
  {
    uint64_t size_bits = uint64_t(base_type->bytesize) * 8;
    elements.push_back(di_builder->createMemberType(
        nullptr, "__base", nullptr, 0, size_bits, 0,
        0, llvm::DINode::FlagZero, base_type->GetDiType()));
  }
  else if (is_polymorphic)
  {
    elements.push_back(di_builder->createMemberType(
        nullptr, "__vtable", nullptr, 0, TARGET_PTRSIZE * 8, 0,
        0, llvm::DINode::FlagZero, nullptr));
  }
  for (int i = 0; i < (int)member_order.size(); ++i)
  {
    OValSym * m = member_order[i];
    uint64_t offset_bits = uint64_t(m->field_offset) * 8;
    OType * storage_type = m->GetStorageType();
    uint64_t size_bits = uint64_t(storage_type->bytesize) * 8;
    elements.push_back(di_builder->createMemberType(
        nullptr, m->name, nullptr, 0, size_bits, 0,
        offset_bits, llvm::DINode::FlagZero, storage_type->GetDiType()));
  }

  di_builder->replaceArrays(di_compound_type, di_builder->getOrCreateArray(elements));
  return di_compound_type;
}

bool OCompoundType::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  EnsureLayout();

  int begin_tag = IsObject() ? DQMIF_OBJ_BEGIN : (IsUnion() ? DQMIF_UNION_BEGIN : DQMIF_STRUCT_BEGIN);
  int end_tag = IsObject() ? DQMIF_OBJ_END : (IsUnion() ? DQMIF_UNION_END : DQMIF_STRUCT_END);
  const char * kind_name = IsObject() ? "object" : (IsUnion() ? "union" : "struct");

  if (!writer.AddRecStr(begin_tag, name)) return false;
  if (bytesize > uint32_t(numeric_limits<int32_t>::max()))
  {
    return writer.Fail(format("Compound type {} is too large for DQM interface: {}", name, bytesize));
  }
  if (!writer.AddRecI32(DQMIF_SIZE_SPEC, int32_t(bytesize))) return false;
  if (alignsize > uint32_t(numeric_limits<int32_t>::max()))
  {
    return writer.Fail(format("Compound type {} alignment is too large for DQM interface: {}", name, alignsize));
  }
  if (!writer.AddRecI32(DQMIF_ALIGN_SPEC, int32_t(alignsize))) return false;
  if (base_type)
  {
    if (base_type->module && !base_type->module->name.empty())
    {
      if (!writer.AddRecStringPair(DQMIF_OBJ_BASE_QUAL, base_type->module->name, base_type->name)) return false;
    }
    else if (!writer.AddRecStr(DQMIF_OBJ_BASE, base_type->name)) return false;
  }

  for (OValSym * member : member_order)
  {
    if (!member)
    {
      return writer.Fail(format("Compound type {} has a null member", name));
    }
    if (!writer.AddRecStr(DQMIF_FIELD_BEGIN, member->name)) return false;
    if (!member->WriteDqmIfAttributes(writer)) return false;
    if (!member->ptype)
    {
      return writer.Fail(format("Field {}.{} has no type", name, member->name));
    }
    if (member->field_offset > uint32_t(numeric_limits<int32_t>::max()))
    {
      return writer.Fail(format("Field {}.{} offset is too large for DQM interface: {}",
          name, member->name, member->field_offset));
    }
    if (!writer.AddRecI32(DQMIF_FIELD_OFFSET, int32_t(member->field_offset))) return false;
    if (!member->ptype->WriteDqmIfTypeSpec(writer)) return false;
    if (!writer.AddRecEmpty(DQMIF_FIELD_END)) return false;
  }

  for (auto & [mname, vs] : Members()->valsyms)
  {
    (void)mname;
    if (!vs || VSK_FUNCTION != vs->kind)
    {
      continue;
    }
    if (auto * fn = dynamic_cast<OValSymFunc *>(vs))
    {
      if (!fn->WriteDqmIfFunction(writer, true)) return false;
    }
    else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
    {
      if (!ovset->WriteDqmIfMethods(writer)) return false;
    }
    else
    {
      return writer.Fail(format("Unsupported {} method symbol: {}", kind_name, vs->name));
    }
  }

  for (OValSymProperty * property : properties)
  {
    if (!writer.AddRecStr(DQMIF_PROPERTY_BEGIN, property->name)) return false;
    if (!property->WriteDqmIfAttributes(writer)) return false;
    if (property->is_default && !writer.AddRecEmpty(DQMIF_PROPERTY_DEFAULT)) return false;
    for (const OPropertyIndex & index : property->indices)
    {
      if (!writer.AddRecStr(DQMIF_PROPERTY_INDEX_BEGIN, index.name)) return false;
      if (FPM_REF == index.mode && !writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REF)) return false;
      if (FPM_REFIN == index.mode && !writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REFIN)) return false;
      if (FPM_REFOUT == index.mode && !writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REFOUT)) return false;
      if (FPM_REFNULL == index.mode && !writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REFNULL)) return false;
      if (!index.ptype->WriteDqmIfTypeSpec(writer)) return false;
      if (!writer.AddRecEmpty(DQMIF_PROPERTY_INDEX_END)) return false;
    }
    if (!writer.AddRecEmpty(DQMIF_PROPERTY_VALUE_TYPE)) return false;
    if (!property->ptype->WriteDqmIfTypeSpec(writer)) return false;
    if (property->read_accessor
        && !writer.AddRecStr(DQMIF_PROPERTY_READ, property->read_accessor->name)) return false;
    if (property->write_accessor
        && !writer.AddRecStr(DQMIF_PROPERTY_WRITE, property->write_accessor->name)) return false;
    if (!writer.AddRecEmpty(DQMIF_PROPERTY_END)) return false;
  }

  return writer.AddRecEmpty(end_tag);
}

struct SStructInitField
{
  OValSym * field;
  vector<unsigned> ll_path;
};

static void CollectStructInitFields(OCompoundType * type, const vector<unsigned> & prefix,
                                    vector<SStructInitField> & result)
{
  // LLVM field indices are assigned while the concrete LLVM layout is built.
  type->GetLlType();
  if (type->base_type)
  {
    vector<unsigned> base_path(prefix);
    base_path.push_back(0);
    CollectStructInitFields(type->base_type, base_path, result);
  }
  for (OValSym * field : type->member_order)
  {
    vector<unsigned> path(prefix);
    path.push_back(field->ll_field_index);
    result.push_back({field, std::move(path)});
  }
}

OValueStruct::OValueStruct(OCompoundType * atype)
:
  super(atype)
{
  vector<SStructInitField> init_fields;
  CollectStructInitFields(atype, {}, init_fields);
  fields.reserve(init_fields.size());
  for (const SStructInitField & init_field : init_fields)
  {
    OValue * field_value = init_field.field->ptype->CreateValue();
    if (!field_value)
    {
      throw logic_error(format("Struct field type \"{}\" does not support constant values",
                               init_field.field->ptype->name));
    }
    fields.push_back({init_field.field, init_field.ll_path, field_value});
  }
}

OValueStruct::~OValueStruct()
{
  for (OStructConstField & field : fields)
  {
    delete field.value;
  }
}

LlConst * OValueStruct::CreateLlConst()
{
  llvm::Constant * result = llvm::Constant::getNullValue(ptype->GetLlType());
  for (OStructConstField & field : fields)
  {
    result = llvm::ConstantFoldInsertValueInstruction(result, field.value->GetLlConst(), field.ll_path);
    if (!result)
    {
      throw logic_error(format("Could not build constant value for struct \"{}\"", ptype->name));
    }
  }
  return result;
}

bool OValueStruct::CalculateConstant(OExpr * expr, bool emit_errors)
{
  if (!expr)
  {
    return false;
  }
  if (!expr->ptype
      && !g_compiler->ConvertExprToType(ptype, &expr, emit_errors ? EXPCF_GENERATE_ERRORS : 0))
  {
    return false;
  }
  auto * literal = dynamic_cast<OStructLit *>(expr);
  if (!literal || literal->ResolvedType() != ptype->ResolveAlias())
  {
    if (emit_errors) g_compiler->Error(DQERR_CONSTEXPR_INVALID_FOR, ptype->name);
    return false;
  }

  for (OStructLitEntry & entry : literal->entries)
  {
    OValue * target = nullptr;
    for (OStructConstField & field : fields)
    {
      if (field.field == entry.field)
      {
        target = field.value;
        break;
      }
    }
    if (!target || !target->CalculateConstant(entry.value, emit_errors))
    {
      return false;
    }
  }
  return true;
}

bool OValueStruct::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.AddRecEmpty(DQMIF_VALUE_LINKED);
}

OValue * OCompoundType::CreateValue()
{
  return (TK_STRUCT == kind) ? new OValueStruct(this) : nullptr;
}

static int AnalyzeStructLiteral(OCompoundType * type, OStructLit * literal,
                                uint32_t aflags, bool convert)
{
  if (literal->entries.empty() && !literal->fill_missing)
  {
    if (TK_OBJECT == type->kind)
    {
      if (convert && (aflags & EXPCF_GENERATE_ERRORS))
      {
        g_compiler->Error(DQERR_NOT_SUPPORTED, "value-style object zero-initialization");
      }
      return -1;
    }
    if (convert)
    {
      literal->ptype = type;
    }
    return 0;
  }
  if ((TK_STRUCT != type->kind) || type->IsUnion())
  {
    if (convert && (aflags & EXPCF_GENERATE_ERRORS))
    {
      g_compiler->Error(DQERR_STRUCT_INIT_CONTEXT);
    }
    return -1;
  }

  vector<SStructInitField> fields;
  CollectStructInitFields(type, {}, fields);
  vector<bool> assigned(fields.size(), false);
  size_t cursor = 0;
  int total_cost = 0;

  for (OStructLitEntry & entry : literal->entries)
  {
    size_t field_index = fields.size();
    if (entry.name.empty())
    {
      field_index = cursor;
      if (field_index >= fields.size())
      {
        if (convert && (aflags & EXPCF_GENERATE_ERRORS))
        {
          g_compiler->Error(DQERR_STRUCT_INIT_EXCESS, type->name);
        }
        return -1;
      }
      ++cursor;
    }
    else
    {
      OCompoundType * declaring_type = nullptr;
      int local_index = type->FindFieldIndex(entry.name, &declaring_type);
      if (local_index < 0)
      {
        if (convert && (aflags & EXPCF_GENERATE_ERRORS))
        {
          g_compiler->Error(DQERR_STRUCT_INIT_UNKNOWN_FIELD, entry.name, type->name);
        }
        return -1;
      }
      OValSym * named_field = declaring_type->member_order[size_t(local_index)];
      for (size_t i = 0; i < fields.size(); ++i)
      {
        if (fields[i].field == named_field)
        {
          field_index = i;
          break;
        }
      }
      cursor = field_index + 1;
    }

    if (assigned[field_index])
    {
      if (convert && (aflags & EXPCF_GENERATE_ERRORS))
      {
        g_compiler->Error(DQERR_STRUCT_INIT_DUP_FIELD, fields[field_index].field->name);
      }
      return -1;
    }

    OType * field_type = fields[field_index].field->ptype;
    if (convert)
    {
      if (!g_compiler->ConvertExprToType(field_type, &entry.value,
                                         aflags | EXPCF_ALLOW_LAZY_CSTRING))
      {
        return -1;
      }
      entry.field = fields[field_index].field;
      entry.ll_path = fields[field_index].ll_path;
    }
    else
    {
      int cost = g_compiler->GetAssignTypeConversionCost(
          field_type, entry.value, aflags | EXPCF_ALLOW_LAZY_CSTRING);
      if (cost < 0)
      {
        return -1;
      }
      total_cost += cost;
    }
    assigned[field_index] = true;
  }

  size_t missing_index = fields.size();
  for (size_t i = 0; i < assigned.size(); ++i)
  {
    if (!assigned[i])
    {
      missing_index = i;
      break;
    }
  }

  // Empty braces retain DQ's existing whole-aggregate default initialization.
  bool whole_default = literal->entries.empty();
  if ((missing_index < fields.size()) && !literal->fill_missing && !whole_default)
  {
    if (convert && (aflags & EXPCF_GENERATE_ERRORS))
    {
      g_compiler->Error(DQERR_STRUCT_INIT_MISSING, fields[missing_index].field->name, type->name);
    }
    return -1;
  }
  if (literal->fill_missing && (missing_index == fields.size()))
  {
    if (convert && (aflags & EXPCF_GENERATE_ERRORS))
    {
      g_compiler->Error(DQERR_STRUCT_INIT_FILL_REDUNDANT);
    }
    return -1;
  }

  if (convert)
  {
    literal->ptype = type;
  }
  return total_cost;
}

bool OCompoundType::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  if (auto * literal = dynamic_cast<OStructLit *>(*rexpr); literal && !literal->ptype)
  {
    return AnalyzeStructLiteral(this, literal, aflags, true) >= 0;
  }
  if (IsUnion())
  {
    OType * source_type = (*rexpr)->ResolvedType();
    if (source_type == this)
    {
      return true;
    }
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      if (aflags & EXPCF_EXPLICIT_CAST)
      {
        g_compiler->Error(DQERR_CAST_INVALID, source_type->name, name);
      }
      else
      {
        g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", name, source_type->name);
      }
    }
    return false;
  }
  OType * source_type = (*rexpr)->ResolvedType();
  if (source_type == this)
  {
    return true;
  }
  if (aflags & EXPCF_GENERATE_ERRORS)
  {
    if (aflags & EXPCF_EXPLICIT_CAST)
    {
      g_compiler->Error(DQERR_CAST_INVALID, source_type->name, name);
    }
    else
    {
      g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", name, source_type->name);
    }
  }
  return false;
}

int OCompoundType::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  if (auto * literal = dynamic_cast<OStructLit *>(expr); literal && !literal->ptype)
  {
    return AnalyzeStructLiteral(this, literal, aflags, false);
  }
  if (IsUnion())
  {
    return expr->ResolvedType() == this ? 0 : -1;
  }
  return expr->ResolvedType() == this ? 0 : -1;
}


bool OTypeObject::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  if (dynamic_cast<OStructLit *>(*rexpr) && !(*rexpr)->ptype)
  {
    return OCompoundType::ConvertFromExpr(rexpr, aflags);
  }
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_OBJECT != tks)
  {
    if (TK_POINTER == tks)
    {
      OTypePointer * ptrsrc = static_cast<OTypePointer *>(resolved_src);
      OTypeObject * src_object = dynamic_cast<OTypeObject *>(ptrsrc->basetype ? ptrsrc->basetype->ResolveAlias() : nullptr);
      if (ptrsrc->IsNullPointer() || (src_object && src_object->IsSameOrDerivedFrom(this)))
      {
        if (src_object && src_object != this)
        {
          *rexpr = new OObjectUpcastExpr(this, src);
        }
        else if (is_explicit_cast)
        {
          *rexpr = new OExprTypeConv(this, src);
          FoldExprTreeAfterTypeRewrite(rexpr);
        }
        return true;
      }
      if (is_explicit_cast)
      {
        *rexpr = new OExprTypeConv(this, src);
        FoldExprTreeAfterTypeRewrite(rexpr);
        return true;
      }
      if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
      return false;
    }
    return OType::ConvertFromExpr(rexpr, aflags);
  }

  OTypeObject * src_object = dynamic_cast<OTypeObject *>(resolved_src);
  if (src_object && src_object->IsSameOrDerivedFrom(this))
  {
    if (src_object != this)
    {
      *rexpr = new OObjectUpcastExpr(this, src);
    }
    return true;
  }

  if (is_explicit_cast)
  {
    *rexpr = new OExprTypeConv(this, src);
    FoldExprTreeAfterTypeRewrite(rexpr);
    return true;
  }

  if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
  return false;
}

int OTypeObject::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  if (dynamic_cast<OStructLit *>(expr) && !expr->ptype)
  {
    return OCompoundType::GetConversionCostFromExpr(expr, aflags);
  }
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_OBJECT != tks)
  {
    if (TK_POINTER == tks)
    {
      OTypePointer * ptrsrc = static_cast<OTypePointer *>(resolved_src);
      OTypeObject * src_object = dynamic_cast<OTypeObject *>(ptrsrc->basetype ? ptrsrc->basetype->ResolveAlias() : nullptr);
      if (ptrsrc->IsNullPointer() || (src_object && src_object->IsSameOrDerivedFrom(this))) return 0;
      return is_explicit_cast ? 1 : -1;
    }
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  OTypeObject * src_object = dynamic_cast<OTypeObject *>(resolved_src);
  if (src_object && src_object->IsSameOrDerivedFrom(this)) return 0;
  return is_explicit_cast ? 1 : -1;
}

class OValueObjectTypeRef : public OValue
{
public:
  OValueObjectTypeRef(OType * atype)
  :
    OValue(atype)
  {
  }

  LlConst * CreateLlConst() override
  {
    return llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
  }
};

LlType * OTypeObjectTypeRef::CreateLlType()
{
  return llvm::PointerType::get(ll_ctx, 0);
}

LlDiType * OTypeObjectTypeRef::CreateDiType()
{
  return di_builder->createPointerType(nullptr, bytesize * 8);
}

OValue * OTypeObjectTypeRef::CreateValue()
{
  return new OValueObjectTypeRef(this);
}

LlValue * OTypeObjectTypeRef::GenerateConversion(OScope * scope, OExpr * src)
{
  return src->Generate(scope);
}

bool OTypeObjectTypeRef::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = (rexpr ? *rexpr : nullptr);
  OType * resolved_src = src ? src->ResolvedType() : nullptr;
  auto * srctype = dynamic_cast<OTypeObjectTypeRef *>(resolved_src);
  auto * srcptr = dynamic_cast<OTypePointer *>(resolved_src);
  bool is_null = srcptr && srcptr->IsNullPointer();

  if (is_null)
  {
    return true;
  }

  if (!srctype || !srctype->object_type || !object_type)
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", name, resolved_src ? resolved_src->name : "?");
    }
    return false;
  }

  OTypeObject * src_obj = srctype->object_type;
  if (src_obj->is_abstract || !src_obj->IsSameOrDerivedFrom(object_type)
      || !src_obj->SupportsConstructorContractFrom(object_type))
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", name, srctype->name);
    }
    return false;
  }

  if (src->ptype != this)
  {
    *rexpr = new OExprTypeConv(this, src);
    FoldExprTreeAfterTypeRewrite(rexpr);
  }
  return true;
}

int OTypeObjectTypeRef::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  (void)aflags;
  OType * resolved_src = expr ? expr->ResolvedType() : nullptr;
  auto * srcptr = dynamic_cast<OTypePointer *>(resolved_src);
  if (srcptr && srcptr->IsNullPointer())
  {
    return 0;
  }

  auto * srctype = dynamic_cast<OTypeObjectTypeRef *>(resolved_src);
  if (!srctype || !srctype->object_type || !object_type)
  {
    return -1;
  }

  OTypeObject * src_obj = srctype->object_type;
  return (!src_obj->is_abstract && src_obj->IsSameOrDerivedFrom(object_type)
          && src_obj->SupportsConstructorContractFrom(object_type)) ? 0 : -1;
}

bool OTypeObjectTypeRef::WriteDqmIfTypeSpec(ODqmIfWriter & writer)
{
  if (!object_type)
  {
    return writer.Fail("Can not write an object type reference without an object type");
  }
  if (object_type->module && !object_type->module->name.empty())
  {
    return writer.AddRecStringPair(DQMIF_TYPE_SPEC_OBJECT_TYPE_QUAL,
                                   object_type->module->name, object_type->name);
  }
  return writer.AddRecStr(DQMIF_TYPE_SPEC_OBJECT_TYPE, object_type->name);
}
