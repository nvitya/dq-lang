/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    symbols.h
 * authors: nvitya
 * created: 2026-02-01
 * brief:   Compiler Symbol Objects
 */

#include <stdexcept>
#include "string.h"
#include <format>

#include "symbols.h"
#include "dqc_base.h"

#include "otype_array.h"
#include "otype_int.h"
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
  if (arecursive and (parent_scope != nullptr))
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

OType::~OType()
{
  delete ptr_type;
  delete slice_type;
  for (auto & [len, arrtype] : array_types)
  {
    delete arrtype;
  }
}

OTypePointer * OType::GetPointerType()
{
  if (!ptr_type)
  {
    ptr_type = new OTypePointer(this);
  }
  return ptr_type;
}

OTypeArray * OType::GetArrayType(uint32_t alength)
{
  auto it = array_types.find(alength);
  if (it != array_types.end())
  {
    return it->second;
  }
  OTypeArray * result = new OTypeArray(this, alength);
  array_types[alength] = result;
  return result;
}

OTypeArraySlice * OType::GetSliceType()
{
  if (!slice_type)
  {
    slice_type = new OTypeArraySlice(this);
  }
  return slice_type;
}

OValSym * OType::CreateValSym(OScPosition & apos, const string aname)
{
  OValSym * result = new OValSym(apos, aname, this);
  return result;
}

LlValue * OTypePointer::GenerateConversion(OScope * scope, OExpr * src)
{
  OType * srctype = src->ResolvedType();
  if (!srctype)
  {
    throw logic_error("Pointer conversion requires a source type");
  }

  if (TK_POINTER == srctype->kind)
  {
    LlValue * ll_value = src->Generate(scope);
    if (ll_value->getType() == GetLlType())
    {
      return ll_value;
    }
    return ll_builder.CreateBitCast(ll_value, GetLlType());
  }

  OTypeInt * srcint = dynamic_cast<OTypeInt *>(srctype);
  if (srcint)
  {
    LlValue * ll_value = src->Generate(scope);
    LlType * ll_ptrint = LlType::getIntNTy(ll_ctx, TARGET_PTRSIZE * 8);

    if (srcint->bitlength < TARGET_PTRSIZE * 8)
    {
      ll_value = srcint->issigned
          ? ll_builder.CreateSExt(ll_value, ll_ptrint)
          : ll_builder.CreateZExt(ll_value, ll_ptrint);
    }
    else if (srcint->bitlength > TARGET_PTRSIZE * 8)
    {
      ll_value = ll_builder.CreateTrunc(ll_value, ll_ptrint);
    }

    return ll_builder.CreateIntToPtr(ll_value, GetLlType());
  }

  throw logic_error(format("Unsupported pointer conversion from \"{}\"", src->ptype->name));
}

void OCompoundType::AddMember(OValSym * amember)
{
  member_scope.DefineValSym(amember);
  member_order.push_back(amember);
}

int OCompoundType::FindMemberIndex(const string & aname)
{
  for (int i = 0; i < (int)member_order.size(); ++i)
  {
    if (member_order[i]->name == aname)  return i;
  }
  return -1;
}

LlType * OCompoundType::CreateLlType()
{
  vector<LlType *> member_types;
  for (OValSym * m : member_order)
  {
    member_types.push_back(m->ptype->GetLlType());
  }
  return llvm::StructType::create(ll_ctx, member_types, name);
}

LlDiType * OCompoundType::CreateDiType()
{
  LlType * ll_stype = GetLlType();
  const llvm::DataLayout & dl = ll_module->getDataLayout();
  const llvm::StructLayout * sl = dl.getStructLayout(static_cast<llvm::StructType *>(ll_stype));

  vector<llvm::Metadata *> elements;
  for (int i = 0; i < (int)member_order.size(); ++i)
  {
    OValSym * m = member_order[i];
    uint64_t offset_bits = sl->getElementOffsetInBits(i);
    uint64_t size_bits = dl.getTypeSizeInBits(m->ptype->GetLlType());
    elements.push_back(di_builder->createMemberType(
        nullptr, m->name, nullptr, 0, size_bits, 0,
        offset_bits, llvm::DINode::FlagZero, m->ptype->GetDiType()));
  }

  uint64_t total_bits = dl.getTypeSizeInBits(ll_stype);
  bytesize = total_bits / 8;

  return di_builder->createStructType(
      nullptr, name, nullptr, 0, total_bits, 0,
      llvm::DINode::FlagZero, nullptr,
      di_builder->getOrCreateArray(elements)
  );
}

void OValSym::GenGlobalDecl(bool apublic, OValue * ainitval)
{
  if (VSK_VARIABLE == kind)
  {
    LlLinkType  linktype =
      (apublic ? LlLinkType::ExternalLinkage
               : LlLinkType::InternalLinkage);

    LlType *          ll_type  = ptype->GetLlType();
    LlConst *         ll_init_val = ainitval->GetLlConst();

    llvm::GlobalVariable * gv = new llvm::GlobalVariable(*ll_module, ll_type, false, linktype, ll_init_val, name);
    ll_value = gv;
    if (!attr_section_name.empty())
    {
      gv->setSection(attr_section_name);
    }

    if (g_opt.dbg_info)
    {
      llvm::DIGlobalVariableExpression * debug_expr = di_builder->createGlobalVariableExpression(
          di_unit,            // The scope (usually the compile unit)
          name,               // The name in the source code
          name,               // The linkage name (mangled name, if applicable)
          scpos.scfile->di_file, // The file where it is declared
          scpos.line,         // The line number in the source code (example: line 10)
          ptype->GetDiType(), // The debug type
          not apublic         // Is it local to the compile unit? (false for true globals)
      );
      gv->addDebugInfo(debug_expr);
    }

  }
  else if (VSK_CONST == kind)
  {
    OValSymConst * vsconst = static_cast<OValSymConst *>(this);
    if (TK_ARRAY == ptype->kind)
    {
      LlLinkType  linktype =
        (apublic ? LlLinkType::ExternalLinkage
                 : LlLinkType::InternalLinkage);

      LlType * ll_type = ptype->GetLlType();
      LlConst * ll_init_val = vsconst->pvalue->GetLlConst();

      llvm::GlobalVariable * gv =
          new llvm::GlobalVariable(*ll_module, ll_type, true, linktype, ll_init_val, name);
      ll_value = gv;
      if (!attr_section_name.empty())
      {
        gv->setSection(attr_section_name);
      }
    }
    else
    {
      ll_value = vsconst->pvalue->GetLlConst();
    }
  }
}

void OValSym::ApplyAttributes(OAttr * attr, EAttrTarget atarget)
{
  if (!attr || !attr->flags)
  {
    return;
  }

  attr->CheckInvalidAttributes(atarget);

  if ((ATGT_FUNCTION == atarget) || (ATGT_GLOBAL_VAR == atarget) || (ATGT_GLOBAL_CONST == atarget))
  {
    if (attr->IsSet(ATTF_ALIGN))
    {
      attr_align = attr->align_value;
    }
    if (attr->IsSet(ATTF_SECTION))
    {
      attr_section_name = attr->section_name;
    }
  }

  if (ATGT_FUNCTION == atarget)
  {
    attr_is_overload = attr->IsSet(ATTF_OVERLOAD);
    attr_is_override = attr->IsSet(ATTF_OVERRIDE);
    attr_is_virtual  = attr->IsSet(ATTF_VIRTUAL);
  }

  if ((ATGT_GLOBAL_VAR == atarget) || (ATGT_STRUCT_MEMBER == atarget))
  {
    attr_is_volatile = attr->IsSet(ATTF_VOLATILE);
  }
}

OExpr::OExpr()
{
  ptype = g_builtins->type_int;
}
