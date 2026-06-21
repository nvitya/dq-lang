/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
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

  if (manual_ll_layout)
  {
    // Imported type: do not emit local definitions, just use external references
    auto * ptr_type = llvm::PointerType::get(ll_ctx, 0);
    auto * ti_arrtype = llvm::ArrayType::get(ptr_type, 2);
    string ti_ll_name = "_DQTI_" + name;
    ll_typeinfo = new llvm::GlobalVariable(*ll_module, ti_arrtype, false, llvm::GlobalValue::ExternalLinkage, nullptr, ti_ll_name);

    auto * vt_arrtype = llvm::ArrayType::get(ptr_type, virtual_methods.size() + 2);
    string ll_name = "_DQVT_" + name;
    ll_vtable = new llvm::GlobalVariable(*ll_module, vt_arrtype, false, llvm::GlobalValue::ExternalLinkage, nullptr, ll_name);
    return;
  }


  LlLinkType linktype = (apublic ? llvm::GlobalValue::ExternalLinkage
                                 : llvm::GlobalValue::InternalLinkage);
  llvm::PointerType * ptr_type = llvm::PointerType::get(ll_ctx, 0);

  // Generate TypeInfo
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

  auto * ti_arrtype = llvm::ArrayType::get(ptr_type, 2);
  auto * ti_init = llvm::ConstantArray::get(ti_arrtype, typeinfo_entries);
  string ti_ll_name = "_DQTI_" + name;
  ll_typeinfo = new llvm::GlobalVariable(*ll_module, ti_arrtype, true, linktype, ti_init, ti_ll_name);

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
      llvm::dwarf::DW_TAG_structure_type, name, nullptr, nullptr, 0,
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

  int begin_tag = IsObject() ? DQMIF_OBJ_BEGIN : DQMIF_STRUCT_BEGIN;
  int end_tag = IsObject() ? DQMIF_OBJ_END : DQMIF_STRUCT_END;
  const char * kind_name = IsObject() ? "object" : "struct";

  if (!writer.AddRecStr(begin_tag, name)) return false;
  if (bytesize > uint32_t(numeric_limits<int32_t>::max()))
  {
    return writer.Fail(format("Compound type {} is too large for DQM interface: {}", name, bytesize));
  }
  if (!writer.AddRecI32(DQMIF_SIZE_SPEC, int32_t(bytesize))) return false;
  if (base_type)
  {
    int base_tag = IsObject() ? DQMIF_OBJ_BASE : DQMIF_OBJ_BASE; // Structs use DQMIF_OBJ_BASE too in original code
    if (!writer.AddRecStr(base_tag, base_type->name)) return false;
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

bool OCompoundType::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  return OType::ConvertFromExpr(rexpr, aflags);
}

int OCompoundType::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  return OType::GetConversionCostFromExpr(expr, aflags);
}


bool OTypeObject::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
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

  return true;
}

int OTypeObject::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
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

  return 0;
}
