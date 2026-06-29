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

#include <algorithm>
#include "dqc_ast.h"
#include <limits>
#include <stdexcept>
#include "string.h"
#include <format>

#include "symbols.h"
#include "dqc_base.h"

#include "dqm_if.h"
#include "expressions.h"
#include "otype_array.h"
#include "otype_anyvalue.h"
#include "otype_cstring.h"
#include "otype_func.h"
#include "otype_int.h"
#include "otype_compound.h"
#include "dqc.h"
#include "errorcodes.h"
#include <llvm/IR/GlobalVariable.h>

using namespace std;



bool OModuleUse::SymbolSelected(const string & aname) const
{
  if (MUM_ALL == merge_mode)
  {
    return true;
  }
  if (MUM_ONLY == merge_mode)
  {
    return symbol_names.end() != find(symbol_names.begin(), symbol_names.end(), aname);
  }
  if (MUM_EXCLUDE == merge_mode)
  {
    return symbol_names.end() == find(symbol_names.begin(), symbol_names.end(), aname);
  }
  return false;
}

bool OModuleUse::ValidateSymbolNames() const
{
  if (!module)
  {
    return false;
  }
  if (MUM_ONLY != merge_mode && MUM_EXCLUDE != merge_mode)
  {
    return true;
  }

  for (const string & name : symbol_names)
  {
    if (!module->scope_pub->FindType(name, nullptr, false)
        && !module->scope_pub->FindValSym(name, nullptr, false))
    {
      g_compiler->Error(DQERR_USE_SYMBOL_UNKNOWN, module->name, name);
      return false;
    }
  }
  return true;
}

void OModuleUse::CopySelectedSymbolsTo(OScope * adst) const
{
  if (!adst || !module)
  {
    return;
  }

  OScope * srcscope = module->scope_pub;
  for (auto & it : srcscope->typesyms)
  {
    if (SymbolSelected(it.first))
    {
      adst->typesyms[it.first] = it.second;
    }
  }
  for (auto & it : srcscope->valsyms)
  {
    if (SymbolSelected(it.first))
    {
      adst->valsyms[it.first] = it.second;
    }
  }
}

void OModuleUse::FillScope() const
{
  if (!module || !scope_use)
  {
    return;
  }
  CopySelectedSymbolsTo(scope_use);
}

vector<string> OModuleUse::EffectiveSymbolNames() const
{
  vector<string> result;
  if (!module)
  {
    return result;
  }

  auto add_name = [&result](const string & name)
  {
    if (result.end() == find(result.begin(), result.end(), name))
    {
      result.push_back(name);
    }
  };

  for (auto & it : module->scope_pub->typesyms)
  {
    if (SymbolSelected(it.first))
    {
      add_name(it.first);
    }
  }
  for (auto & it : module->scope_pub->valsyms)
  {
    if (SymbolSelected(it.first))
    {
      add_name(it.first);
    }
  }
  return result;
}



OType::~OType()
{
  delete ptr_type;
  delete slice_type;
  delete dyn_array_type;
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

OTypeDynArray * OType::GetDynArrayType()
{
  if (!dyn_array_type)
  {
    dyn_array_type = new OTypeDynArray(this);
  }
  return dyn_array_type;
}

OValSym * OType::CreateValSym(OScPosition & apos, const string aname)
{
  OValSym * result = new OValSym(apos, aname, this);
  return result;
}

static bool WriteDqmIfTypeSpecInner(ODqmIfWriter & writer, OType * atype)
{
  if (!atype)
  {
    return writer.Fail("Can not write null type spec");
  }

  if (auto * ptrtype = dynamic_cast<OTypePointer *>(atype))
  {
    if (ptrtype->IsNullPointer())
    {
      return writer.Fail("Can not write null pointer as a DQM interface type spec");
    }
    if (!ptrtype->IsTypedPointer())
    {
      return writer.AddTypeSpecRec(DQMIF_TYPE_SPEC_NAME, ptrtype->name);
    }
    if (!writer.AddRecEmpty(DQMIF_TYPE_SPEC_PTR)) return false;
    return WriteDqmIfTypeSpecInner(writer, ptrtype->basetype);
  }

  if (auto * arrtype = dynamic_cast<OTypeArray *>(atype))
  {
    if (arrtype->arraylength > uint32_t(numeric_limits<int32_t>::max()))
    {
      return writer.Fail(format("Array type {} is too large for DQM interface: {}",
          arrtype->name, arrtype->arraylength));
    }
    if (!writer.AddRecI32(DQMIF_TYPE_SPEC_ARRAY_BEGIN, int32_t(arrtype->arraylength))) return false;
    if (!WriteDqmIfTypeSpecInner(writer, arrtype->elemtype)) return false;
    return writer.AddRecEmpty(DQMIF_TYPE_SPEC_ARRAY_END);
  }

  if (auto * slicetype = dynamic_cast<OTypeArraySlice *>(atype))
  {
    if (!writer.AddRecEmpty(DQMIF_TYPE_SPEC_SLICE_BEGIN)) return false;
    if (!WriteDqmIfTypeSpecInner(writer, slicetype->elemtype)) return false;
    return writer.AddRecEmpty(DQMIF_TYPE_SPEC_SLICE_END);
  }

  if (auto * dyntype = dynamic_cast<OTypeDynArray *>(atype))
  {
    if (!writer.AddRecEmpty(DQMIF_TYPE_SPEC_DYN_ARRAY_BEGIN)) return false;
    if (!WriteDqmIfTypeSpecInner(writer, dyntype->elemtype)) return false;
    return writer.AddRecEmpty(DQMIF_TYPE_SPEC_DYN_ARRAY_END);
  }

  if ((TK_FUNCTION == atype->kind) || (TK_FUNCREF == atype->kind))
  {
    return atype->WriteDqmIfTypeSpec(writer);
  }

  return writer.AddTypeSpecRec(DQMIF_TYPE_SPEC_NAME, atype->name);
}

bool OType::WriteDqmIfTypeSpec(ODqmIfWriter & writer)
{
  if (auto * ptrtype = dynamic_cast<OTypePointer *>(this))
  {
    if (ptrtype->IsNullPointer())
    {
      return writer.Fail("Can not write null pointer as a DQM interface type spec");
    }
    if (!ptrtype->IsTypedPointer())
    {
      return writer.AddTypeSpecRec(DQMIF_TYPE_SPEC_SIMPLE, ptrtype->name);
    }
  }

  if ((TK_POINTER == kind) || (TK_ARRAY == kind) || (TK_ARRAY_SLICE == kind) || (TK_DYN_ARRAY == kind))
  {
    if (!writer.AddRecEmpty(DQMIF_TYPE_SPEC_BEGIN)) return false;
    if (!WriteDqmIfTypeSpecInner(writer, this)) return false;
    return writer.AddRecEmpty(DQMIF_TYPE_SPEC_END);
  }

  return writer.AddTypeSpecRec(DQMIF_TYPE_SPEC_SIMPLE, name);
}

bool OType::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  if (!writer.AddRecStr(DQMIF_TYPE_BEGIN, name)) return false;
  if (!WriteDqmIfTypeSpec(writer)) return false;
  return writer.AddRecEmpty(DQMIF_TYPE_END);
}

bool OType::ContainsManagedStorage() const
{
  return (TK_OBJECT == kind) || (TK_DYNSTR == kind) || (TK_DYN_ARRAY == kind);
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
  return new OValuePointer(this, true);
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

  if (TK_OBJECT == srctype->kind)
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















uint32_t AlignUpU32(uint32_t avalue, uint32_t aalign)
{
  if (aalign <= 1)
  {
    return avalue;
  }
  uint64_t value = avalue;
  uint64_t align = aalign;
  return uint32_t(((value + align - 1) / align) * align);
}

uint32_t EffectiveStorageAlign(OType * atype, uint32_t aattr_align)
{
  if (!atype)
  {
    return max<uint32_t>(1, aattr_align);
  }
  atype->EnsureLayout();
  return max<uint32_t>(max<uint32_t>(1, atype->alignsize), aattr_align);
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
  if (attr_is_external) flags |= 1u << 6;
  if (attr_has_linkage_name) flags |= 1u << 7;
  auto * objsym = dynamic_cast<OVsObject *>(this);
  if (objsym && objsym->IsObjectReference())   flags |= 1u << 8;
  if (objsym && objsym->IsFixedObjectStorage()) flags |= 1u << 9;
  if (MV_PRIVATE == member_visibility)   flags |= 1u << 10;
  if (MV_PROTECTED == member_visibility) flags |= 1u << 11;
  if (attr_is_abstract) flags |= 1u << 12;
  if (attr_is_final)    flags |= 1u << 13;

  if (flags && !writer.AddRecU64(DQMIF_ATTR_FLAGS, flags)) return false;
  if (attr_align && !writer.AddRecI32(DQMIF_ATTR_ALIGN_VALUE, int32_t(attr_align))) return false;
  if (!attr_external_linkage_name.empty()
      && !writer.AddRecStr(DQMIF_ATTR_EXT_LINK_NAME, attr_external_linkage_name)) return false;
  if (!attr_section_name.empty()
      && !writer.AddRecStr(DQMIF_ATTR_SECTION_NAME, attr_section_name)) return false;
  if (attr_has_linkage_name
      && !writer.AddRecStr(DQMIF_ATTR_LINK_NAME, attr_linkage_name)) return false;

  return true;
}



void OValSym::GenGlobalDecl(bool apublic, OValue * ainitval)
{
  if (attr_is_external)
  {
    GenGlobalImportDecl();
    return;
  }

  if (VSK_VARIABLE == kind)
  {
    LlLinkType  linktype =
      ((apublic || attr_has_linkage_name) ? LlLinkType::ExternalLinkage
                                          : LlLinkType::InternalLinkage);

    OType *           storage_type = GetStorageType();
    LlType *          ll_type  = storage_type->GetLlType();
    LlConst *         ll_init_val = (ainitval ? ainitval->GetLlConst()
                                              : llvm::Constant::getNullValue(ll_type));
    string            ll_name = GetLinkageName(apublic, 'V');

    llvm::GlobalVariable * gv = new llvm::GlobalVariable(*ll_module, ll_type, false, linktype, ll_init_val, ll_name);
    ll_value = gv;
    gv->setAlignment(llvm::Align(EffectiveStorageAlign(storage_type, attr_align)));
    if (!attr_section_name.empty())
    {
      gv->setSection(attr_section_name);
    }

    if (g_opt.dbg_info)
    {
      llvm::DIGlobalVariableExpression * debug_expr = di_builder->createGlobalVariableExpression(
          di_unit,            // The scope (usually the compile unit)
          name,               // The name in the source code
          ll_name,            // The linkage name (mangled name, if applicable)
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
        ((apublic || attr_has_linkage_name) ? LlLinkType::ExternalLinkage
                                            : LlLinkType::InternalLinkage);

      LlType * ll_type = ptype->GetLlType();
      LlConst * ll_init_val = vsconst->pvalue->GetLlConst();
      string ll_name = GetLinkageName(apublic, 'C');

      llvm::GlobalVariable * gv =
          new llvm::GlobalVariable(*ll_module, ll_type, true, linktype, ll_init_val, ll_name);
      ll_value = gv;
      gv->setAlignment(llvm::Align(EffectiveStorageAlign(ptype, attr_align)));
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

void OValSym::GenGlobalImportDecl()
{
  if (ll_value)
  {
    return;
  }

  if (VSK_VARIABLE == kind)
  {
    OType *  storage_type = GetStorageType();
    LlType * ll_type = storage_type->GetLlType();
    string   ll_name = (attr_external_linkage_name.empty() ? GetLinkageName(true, 'V') : attr_external_linkage_name);
    if (llvm::GlobalValue * existing = ll_module->getNamedValue(ll_name))
    {
      ll_value = existing;
      return;
    }

    llvm::GlobalVariable * gv =
        new llvm::GlobalVariable(*ll_module, ll_type, false, LlLinkType::ExternalLinkage, nullptr, ll_name);
    ll_value = gv;
    gv->setAlignment(llvm::Align(EffectiveStorageAlign(storage_type, attr_align)));
    if (!attr_section_name.empty())
    {
      gv->setSection(attr_section_name);
    }
  }
  else if ((VSK_CONST == kind) && (TK_ARRAY == ptype->kind))
  {
    LlType * ll_type = ptype->GetLlType();
    string   ll_name = GetLinkageName(true, 'C');
    if (llvm::GlobalValue * existing = ll_module->getNamedValue(ll_name))
    {
      ll_value = existing;
      return;
    }

    llvm::GlobalVariable * gv =
        new llvm::GlobalVariable(*ll_module, ll_type, true, LlLinkType::ExternalLinkage, nullptr, ll_name);
    ll_value = gv;
    gv->setAlignment(llvm::Align(EffectiveStorageAlign(ptype, attr_align)));
    if (!attr_section_name.empty())
    {
      gv->setSection(attr_section_name);
    }
  }
}

string OValSym::GetLinkageName(bool apublic, char atype_prefix, const string & asymbol_name) const
{
  string symbol_name = (asymbol_name.empty() ? name : asymbol_name);
  if (attr_has_linkage_name)
  {
    return attr_linkage_name;
  }

  if (!apublic)
  {
    return symbol_name;
  }

  string module_name = owner_module_name.empty() ? (module ? module->name : (g_module ? g_module->name : "")) : owner_module_name;
  return OModuleIntf::LinkerSymbolNameForModule(atype_prefix, module_name, symbol_name);
}

OValSym::~OValSym()
{
  OExpr::DeleteTree(field_init_expr);
  field_init_expr = nullptr;
}

bool OValSym::IsObjectType() const
{
  return dynamic_cast<OTypeObject *>(ptype ? ptype->ResolveAlias() : nullptr);
}

bool OValSym::GenerateFieldInitStore(OScope * scope, LlValue * ll_field_addr)
{
  if (!field_init_expr)
  {
    return false;
  }

  OType * storage_type = GetStorageType()->ResolveAlias();
  if (TK_ANYVALUE == storage_type->kind)
  {
    return GenerateAnyValueAssignExpr(scope, ll_field_addr, field_init_expr);
  }

  if (auto * cstrtype = dynamic_cast<OTypeCString *>(storage_type))
  {
    return cstrtype->GenerateStore(scope, ll_field_addr, field_init_expr);
  }

  LlValue * ll_value = field_init_expr->Generate(scope);
  ll_builder.CreateStore(ll_value, ll_field_addr);
  return true;
}

OType * OValSym::GetStorageType() const
{
  return (IsRefLike() ? ptype->GetPointerType() : ptype);
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
  if (!WriteDqmIfAttributes(writer)) return false;
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

  if ((ATGT_FUNCTION == atarget) || (ATGT_GLOBAL_VAR == atarget) || (ATGT_GLOBAL_CONST == atarget)
      || (ATGT_STRUCT_MEMBER == atarget))
  {
    if (attr->IsSet(ATTF_ALIGN))
    {
      attr_align = attr->align_value;
    }
  }

  if (ATGT_GLOBAL_VAR == atarget)
  {
    if (attr->IsSet(ATTF_EXTERNAL))
    {
      attr_is_external = true;
      attr_external_linkage_name = attr->external_linkage_name;
    }
  }

  if ((ATGT_FUNCTION == atarget) || (ATGT_GLOBAL_VAR == atarget) || (ATGT_GLOBAL_CONST == atarget))
  {
    if (attr->IsSet(ATTF_SECTION))
    {
      attr_section_name = attr->section_name;
    }
    if (attr->IsSet(ATTF_EXPORT))
    {
      attr_has_linkage_name = true;
      attr_linkage_name = attr->export_linkage_name;
    }
    else if (attr->IsSet(ATTF_CEXPORT))
    {
      attr_has_linkage_name = true;
      attr_linkage_name = name;
    }
  }

  if (ATGT_FUNCTION == atarget)
  {
    attr_is_overload = attr->IsSet(ATTF_OVERLOAD);
    attr_is_override = attr->IsSet(ATTF_OVERRIDE);
    attr_is_virtual  = attr->IsSet(ATTF_VIRTUAL);
    attr_is_abstract = attr->IsSet(ATTF_ABSTRACT);
    attr_is_final    = attr->IsSet(ATTF_FINAL);
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


bool OTypePointer::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_POINTER != tks)
  {
    if (is_explicit_cast && (TK_INT == tks))
    {
      OTypeInt * intsrc = static_cast<OTypeInt *>(resolved_src);
      int64_t const_value = 0;
      bool is_const = g_compiler->TryCalculateIntConstant(src, const_value);
      if (!g_compiler->IsPointerWidthIntegerType(resolved_src))
      {
        if (!is_const)
        {
          if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_CAST_PTR_WIDTH_MISM, resolved_src->name);
          return false;
        }

        if (!g_compiler->FitsPointerWidthConstant(intsrc, const_value))
        {
          if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->ErrorTxt(DQERR_CAST_PTR_CONST_RANGE, to_string(const_value));
          return false;
        }
      }

      *rexpr = new OExprTypeConv(this, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if (TK_INT == tks)
    {
      uint8_t charlit = 0;
      if (IsCCharPointerType(this) && IsCharLiteralExpr(src, charlit))
      {
        *rexpr = new OCharLitToCStringPtrExpr(charlit);
        return true;
      }
    }

    if (is_explicit_cast && (TK_OBJECT == tks))
    {
      *rexpr = new OExprTypeConv(this, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    return OType::ConvertFromExpr(rexpr, aflags);
  }

  OTypePointer * ptrsrc = dynamic_cast<OTypePointer *>(resolved_src);

  if (is_explicit_cast)
  {
    *rexpr = new OExprTypeConv(this, src);
    FoldExprTreeAfterTypeRewrite(rexpr);
    return true;
  }

  uint8_t charlit = 0;
  if (IsCCharPointerType(this) && IsCharLiteralExpr(src, charlit))
  {
    *rexpr = new OCharLitToCStringPtrExpr(charlit);
    return true;
  }

  if (!ptrsrc)
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_PTR_TYPEMISM, this->name, resolved_src->name);
    return false;
  }

  if (!g_compiler->CanAssignPointerImplicitly(this, ptrsrc))
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_PTR_TYPEMISM, this->name, ptrsrc->name);
    return false;
  }

  return true;
}

int OTypePointer::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);

  if (TK_POINTER != tks)
  {
    if (is_explicit_cast && (TK_INT == tks))
    {
      OTypeInt * intsrc = static_cast<OTypeInt *>(resolved_src);
      int64_t const_value = 0;
      bool is_const = g_compiler->TryCalculateIntConstant(expr, const_value);
      if (!g_compiler->IsPointerWidthIntegerType(resolved_src))
      {
        if (!is_const || !g_compiler->FitsPointerWidthConstant(intsrc, const_value)) return -1;
      }
      return 1;
    }
    if (TK_INT == tks)
    {
      uint8_t charlit = 0;
      return (IsCCharPointerType(this) && IsCharLiteralExpr(expr, charlit)) ? 1 : -1;
    }
    if (is_explicit_cast && (TK_OBJECT == tks)) return 1;
    return OType::GetConversionCostFromExpr(expr, aflags);
  }

  OTypePointer * ptrsrc = dynamic_cast<OTypePointer *>(resolved_src);
  if (is_explicit_cast) return 1;

  uint8_t charlit = 0;
  if (IsCCharPointerType(this) && IsCharLiteralExpr(expr, charlit)) return 1;
  if (!ptrsrc) return -1;
  return (g_compiler->CanAssignPointerImplicitly(this, ptrsrc) ? 0 : -1);
}

bool OType::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  bool is_explicit_cast = (aflags & EXPCF_EXPLICIT_CAST);
  
  if (this->kind == resolved_src->kind) return true;
  
  if (is_explicit_cast)
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      g_compiler->Error(DQERR_CAST_INVALID, resolved_src->name, this->name);
    }
  }
  else
  {
    if (aflags & EXPCF_GENERATE_ERRORS)
    {
      g_compiler->Error(DQERR_TYPEMISM_STMT_ASSIGN, "Assignment", this->name, resolved_src->name);
    }
  }
  return false;
}

int OType::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  if (this->kind == resolved_src->kind) return 0;
  return -1;
}

bool OTypeAlias::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  return (ptype ? ptype->ConvertFromExpr(rexpr, aflags) : false);
}

int OTypeAlias::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  return (ptype ? ptype->GetConversionCostFromExpr(expr, aflags) : -1);
}
