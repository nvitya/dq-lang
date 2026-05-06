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

#include "dqm_if.h"
#include "expressions.h"
#include "otype_array.h"
#include "otype_func.h"
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

bool OType::WriteDqmIfTypeSpec(ODqmIfWriter & writer)
{
  OType * base = this;
  int ptrdepth = 0;

  while (auto * ptrtype = dynamic_cast<OTypePointer *>(base))
  {
    ++ptrdepth;
    if (ptrdepth > 3)
    {
      return writer.Fail(format("DQM interface supports only up to 3 pointer levels: {}", name));
    }
    base = ptrtype->basetype;
  }

  switch (ptrdepth)
  {
    case 0:  return base ? writer.AddTypeSpecRec(DQMIF_TYPE_SPEC, base->name)
                          : writer.Fail("Can not write null type spec");
    case 1:  return writer.AddTypeSpecRec(DQMIF_TYPE_SPEC_PTR1, base ? base->name : "void");
    case 2:  return writer.AddTypeSpecRec(DQMIF_TYPE_SPEC_PTR2, base ? base->name : "void");
    case 3:  return writer.AddTypeSpecRec(DQMIF_TYPE_SPEC_PTR3, base ? base->name : "void");
  }

  return writer.Fail(format("Invalid pointer depth while writing type spec: {}", name));
}

bool OType::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (!writer.AddRecStr(DQMIF_TYPE_BEGIN, name)) return false;
  if (!WriteDqmIfTypeSpec(writer)) return false;
  return writer.AddRecEmpty(DQMIF_TYPE_END);
}

bool OTypeAlias::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (!ptype)
  {
    return writer.Fail(format("Type alias {} has no target type", name));
  }
  if (!writer.AddRecStr(DQMIF_TYPE_BEGIN, name)) return false;
  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  return writer.AddRecEmpty(DQMIF_TYPE_END);
}

OValue * OTypePointer::CreateValue()
{
  return new OValuePointer(this, false);
}

LlConst * OValuePointer::CreateLlConst()
{
  if (!is_null)
  {
    return nullptr;
  }

  return llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
}

bool OValuePointer::CalculateConstant(OExpr * expr, bool emit_errors)
{
  is_null = false;

  if (dynamic_cast<ONullLit *>(expr))
  {
    is_null = true;
    return true;
  }

  if (emit_errors)
  {
    g_compiler->Error(DQERR_CONSTEXPR_INVALID_FOR, ptype->name);
  }
  return false;
}

bool OValue::WriteDqmIfValue(ODqmIfWriter & writer)
{
  return writer.Fail(format("Unsupported constant value type in DQM interface: {}", ptype ? ptype->name : "?"));
}

bool OValuePointer::WriteDqmIfValue(ODqmIfWriter & writer)
{
  if (!is_null)
  {
    return writer.Fail("Only null pointer constants are supported in DQM interface generation");
  }
  return writer.AddRecU64(DQMIF_VALUE_INLINE, 0);
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

bool OValSym::WriteDqmIfAttributes(ODqmIfWriter & writer, uint64_t aextra_flags)
{
  uint64_t flags = aextra_flags;
  if (attr_is_overload) flags |= 1u << 0;
  if (attr_is_override) flags |= 1u << 1;
  if (attr_is_virtual)  flags |= 1u << 2;
  if (attr_is_volatile) flags |= 1u << 3;
  if (is_ref_alias)     flags |= 1u << 4;
  if (ref_nullable)     flags |= 1u << 5;

  if (flags && !writer.AddRecU64(DQMIF_ATTR_FLAGS, flags)) return false;
  if (attr_align && !writer.AddRecI32(DQMIF_ATTR_ALIGN_VALUE, int32_t(attr_align))) return false;
  if (!attr_section_name.empty()
      && !writer.AddRecStr(DQMIF_ATTR_SECTION_NAME, attr_section_name)) return false;

  return true;
}

bool OCompoundType::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  TDqmIfRecId begin_rec = (is_object ? DQMIF_OBJ_BEGIN : DQMIF_STRUCT_BEGIN);
  TDqmIfRecId end_rec   = (is_object ? DQMIF_OBJ_END   : DQMIF_STRUCT_END);

  if (!writer.AddRecStr(begin_rec, name)) return false;

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
    if (!member->ptype->WriteDqmIfTypeSpec(writer)) return false;
    if (!writer.AddRecEmpty(DQMIF_FIELD_END)) return false;
  }

  if (is_object)
  {
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
  }

  return writer.AddRecEmpty(end_rec);
}

void OValSym::GenGlobalDecl(bool apublic, OValue * ainitval)
{
  if (VSK_VARIABLE == kind)
  {
    LlLinkType  linktype =
      (apublic ? LlLinkType::ExternalLinkage
               : LlLinkType::InternalLinkage);

    LlType *          ll_type  = ptype->GetLlType();
    LlConst *         ll_init_val = (ainitval ? ainitval->GetLlConst()
                                              : llvm::Constant::getNullValue(ll_type));

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

bool OValSym::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (VSK_VARIABLE != kind)
  {
    return writer.Fail(format("Unsupported value symbol in DQM interface: {}", name));
  }
  if (!ptype)
  {
    return writer.Fail(format("Variable {} has no type", name));
  }

  if (!writer.AddRecStr(DQMIF_VAR_BEGIN, name)) return false;
  if (!WriteDqmIfAttributes(writer)) return false;
  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  return writer.AddRecEmpty(DQMIF_VAR_END);
}

bool OValSymConst::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (!ptype)
  {
    return writer.Fail(format("Constant {} has no type", name));
  }
  if (!pvalue)
  {
    return writer.Fail(format("Constant {} has no value", name));
  }

  if (!writer.AddRecStr(DQMIF_CONST_BEGIN, name)) return false;
  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  if (!pvalue->WriteDqmIfValue(writer)) return false;
  return writer.AddRecEmpty(DQMIF_CONST_END);
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
