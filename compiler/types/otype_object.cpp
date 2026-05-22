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

  for (const OTypeObject * cur = this; cur; cur = cur->base_type)
  {
    if (cur == abase)
    {
      return true;
    }
  }
  return false;
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

OValSym * OTypeObject::FindObjectMemberSymbol(const string & aname, OCompoundType ** rdecl_type) const
{
  for (const OTypeObject * cur = this; cur; cur = cur->base_type)
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

int OTypeObject::FindObjectFieldIndex(const string & aname, OCompoundType ** rdecl_type) const
{
  for (const OTypeObject * cur = this; cur; cur = cur->base_type)
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

  for (OTypeObject * cur = base_type; cur; cur = cur->base_type)
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
  if (base_type)
  {
    virtual_methods = base_type->virtual_methods;
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
  while (root->base_type)
  {
    root = root->base_type;
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

void OTypeObject::EnsureLayout()
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

LlType * OTypeObject::CreateLlType()
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

LlDiType * OTypeObject::CreateDiType()
{
  EnsureLayout();

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

  uint64_t total_bits = uint64_t(bytesize) * 8;

  return di_builder->createStructType(
      nullptr, name, nullptr, 0, total_bits, 0,
      llvm::DINode::FlagZero, nullptr,
      di_builder->getOrCreateArray(elements)
  );
}

bool OTypeObject::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  EnsureLayout();

  if (!writer.AddRecStr(DQMIF_OBJ_BEGIN, name)) return false;
  if (bytesize > uint32_t(numeric_limits<int32_t>::max()))
  {
    return writer.Fail(format("Compound type {} is too large for DQM interface: {}", name, bytesize));
  }
  if (!writer.AddRecI32(DQMIF_SIZE_SPEC, int32_t(bytesize))) return false;
  if (base_type)
  {
    if (!writer.AddRecStr(DQMIF_OBJ_BASE, base_type->name)) return false;
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
      return writer.Fail(format("Unsupported object method symbol: {}", vs->name));
    }
  }

  return writer.AddRecEmpty(DQMIF_OBJ_END);
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
  OTypeObject * ctype = ObjectType();
  return (ctype ? ctype->FindSpecialMethod(OSF_CREATE, object_ctor_args.size()) : nullptr);
}

OValSymFunc * OVsObject::FindDestructor() const
{
  OTypeObject * ctype = ObjectType();
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
