/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_func.cpp
 * authors: nvitya
 * created: 2026-02-07
 * brief:   function type
 */

#include "otype_func.h"
#include "dqc_ast.h"
#include "dqm_if.h"
#include "dqc.h"
#include "dq_module.h"
#include "errorcodes.h"
#include "ll_defs.h"
#include "otype_compound.h"
#include "otype_string.h"
#include "otype_anyvalue.h"
#include "otype_array.h"

ESpecialFuncKind SpecialFuncKindFromName(const string & aname)
{
  if ("Main" == aname)
  {
    return SFK_MAIN;
  }
  if ("ModuleInit" == aname)
  {
    return SFK_MODULE_INIT;
  }
  return SFK_NONE;
}

EObjectSpecFuncKind ObjectSpecFuncKindFromName(const string & aname)
{
  if ("Create" == aname)
  {
    return OSF_CREATE;
  }
  if ("Destroy" == aname)
  {
    return OSF_DESTROY;
  }
  return OSF_NONE;
}

const char * SpecialFuncKindName(ESpecialFuncKind akind)
{
  switch (akind)
  {
    case SFK_MAIN:         return "Main";
    case SFK_MODULE_INIT:  return "ModuleInit";
    case SFK_NONE:         break;
  }
  return "None";
}

void OValSymFunc::ApplyAttributes(OAttr * attr, EAttrTarget atarget)
{
  super::ApplyAttributes(attr, atarget);

  if (!attr || !attr->flags)
  {
    return;
  }

  if ((ATGT_FUNCTION == atarget) && attr->IsSet(ATTF_EXTERNAL))
  {
    is_external = true;
    external_linkage_name = attr->external_linkage_name;
  }
}

bool OValSymFunc::WriteDqmIfFunction(ODqmIfWriter & writer, bool amethod)
{
  OTypeFunc * sigtype = dynamic_cast<OTypeFunc *>(ptype);
  if (!sigtype)
  {
    return writer.Fail(format("Function {} has no function signature type", name));
  }

  if (!writer.AddRecStr(amethod ? DQMIF_METHOD_BEGIN : DQMIF_FUNC_BEGIN, name)) return false;

  uint64_t flags = 0;
  if (is_external) flags |= 1u << 6;
  if (!WriteDqmIfAttributes(writer, flags)) return false;
  if (!external_linkage_name.empty()
      && !writer.AddRecStr(DQMIF_ATTR_EXT_LINK_NAME, external_linkage_name)) return false;
  if (IsSpecial() && !writer.AddRecU8(DQMIF_FUNC_SPECIAL_KIND, uint8_t(special_kind))) return false;

  bool skip_receiver = amethod && owner_compound_type && !sigtype->params.empty()
      && ("__this" == sigtype->params[0]->name);
  if (!sigtype->WriteDqmIfSignatureRecords(writer, skip_receiver)) return false;

  return writer.AddRecEmpty(amethod ? DQMIF_METHOD_END : DQMIF_FUNC_END);
}

bool OValSymFunc::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  return WriteDqmIfFunction(writer, false);
}

OFuncParam * OTypeFunc::AddParam(const string aname, OType * atype, EParamMode amode)
{
  OFuncParam * result = new OFuncParam(aname, atype, amode);
  params.push_back(result);
  return result;
}

bool OFuncParam::WriteDqmIf(ODqmIfWriter & writer) const
{
  if (!ptype)
  {
    return writer.Fail(format("Function parameter {} has no type", name));
  }

  if (!writer.AddRecStr(DQMIF_FUNC_PARAM_BEGIN, name)) return false;

  switch (mode)
  {
    case FPM_VALUE:    break;
    case FPM_REF:      if (!writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REF)) return false; break;
    case FPM_REFIN:    if (!writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REFIN)) return false; break;
    case FPM_REFOUT:   if (!writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REFOUT)) return false; break;
    case FPM_REFNULL:  if (!writer.AddRecEmpty(DQMIF_FUNC_PARAM_MODE_REFNULL)) return false; break;
  }

  if (!ptype->WriteDqmIfTypeSpec(writer)) return false;
  if (defvalue && defvalue->pvalue && !defvalue->pvalue->WriteDqmIfValue(writer)) return false;
  return writer.AddRecEmpty(DQMIF_FUNC_PARAM_END);
}

bool OTypeFunc::WriteDqmIfSignatureRecords(ODqmIfWriter & writer, bool askip_first_param) const
{
  if (rettype && TK_VOID != rettype->kind)
  {
    if (!writer.AddRecEmpty(DQMIF_FUNC_RETVAL)) return false;
    if (!rettype->WriteDqmIfTypeSpec(writer)) return false;
  }

  for (size_t i = 0; i < params.size(); ++i)
  {
    OFuncParam * param = params[i];
    if (!param)
    {
      return writer.Fail(format("Function type {} has a null parameter", name));
    }
    if (askip_first_param && (0 == i))
    {
      continue;
    }
    if (!param->WriteDqmIf(writer)) return false;
  }

  if (has_varargs && !writer.AddRecEmpty(DQMIF_FUNC_PARAM_VARARGS)) return false;
  return true;
}

bool OTypeFunc::ParNameValid(const string aname)
{
  if (g_compiler->ReservedWord(aname))
  {
    return false;
  }

  // search for existing parameters
  for (OFuncParam * fp : params)
  {
    if (fp->name == aname)
    {
      return false;
    }
  }
  return true;
}

size_t OTypeFunc::RequiredParamCount() const
{
  size_t result = 0;
  for (OFuncParam * fp : params)
  {
    if (fp->defvalue)
    {
      break;
    }
    ++result;
  }
  return result;
}

OType * OTypeFunc::ResolvedRetType() const
{
  return (rettype ? rettype->ResolveAlias() : nullptr);
}

OType * OTypeFunc::GetLlRetType() const
{
  auto * compound = dynamic_cast<OCompoundType *>(ResolvedRetType());
  return ((compound && compound->IsObject()) ? rettype->GetPointerType() : rettype);
}

bool OTypeFunc::WriteDqmIfTypeSpec(ODqmIfWriter & writer)
{
  if (!writer.AddRecEmpty(DQMIF_TYPE_SPEC_FUNCREF)) return false;
  if (!WriteDqmIfSignatureRecords(writer)) return false;
  return writer.AddRecEmpty(DQMIF_TYPE_SPEC_END);
}

bool OTypeFunc::MatchesOverloadDeclIdentity(const OTypeFunc * other) const
{
  if (!other)
  {
    return false;
  }

  if (has_varargs != other->has_varargs)
  {
    return false;
  }

  if (params.size() != other->params.size())
  {
    return false;
  }

  if (ResolvedRetType() != other->ResolvedRetType())
  {
    return false;
  }

  for (size_t i = 0; i < params.size(); ++i)
  {
    OFuncParam * left = params[i];
    OFuncParam * right = other->params[i];
    if (!left || !right)
    {
      return false;
    }

    if (!left->ptype || !right->ptype)
    {
      return false;
    }

    if (left->ptype->ResolveAlias() != right->ptype->ResolveAlias())
    {
      return false;
    }
  }

  return true;
}

bool OTypeFunc::MatchesSignature(const OTypeFunc * other) const
{
  if (!other)
  {
    return false;
  }

  if (has_varargs != other->has_varargs)
  {
    return false;
  }

  if (params.size() != other->params.size())
  {
    return false;
  }

  if (ResolvedRetType() != other->ResolvedRetType())
  {
    return false;
  }

  for (size_t i = 0; i < params.size(); ++i)
  {
    OFuncParam * left = params[i];
    OFuncParam * right = other->params[i];
    if (!left || !right)
    {
      return false;
    }

    if (left->mode != right->mode)
    {
      return false;
    }

    if (!left->ptype || !right->ptype)
    {
      return false;
    }

    if (left->ptype->ResolveAlias() != right->ptype->ResolveAlias())
    {
      return false;
    }
  }

  return true;
}

void OTypeFunc::MergeForwardDeclFrom(OTypeFunc * other, bool copy_param_names)
{
  if (!other)
  {
    return;
  }

  size_t cnt = min(params.size(), other->params.size());
  for (size_t i = 0; i < cnt; ++i)
  {
    OFuncParam * dpar = params[i];
    OFuncParam * spar = other->params[i];
    if (!dpar || !spar)
    {
      continue;
    }

    if (copy_param_names)
    {
      dpar->name = spar->name;
    }

    if (!dpar->defvalue && spar->defvalue)
    {
      dpar->defvalue = spar->defvalue;
      spar->defvalue = nullptr;
    }
  }
}

bool OTypeFunc::SameRefBindingType(OType * dsttype, OType * srctype)
{
  OType * resolved_dst = (dsttype ? dsttype->ResolveAlias() : nullptr);
  OType * resolved_src = (srctype ? srctype->ResolveAlias() : nullptr);
  auto * ptrdst = dynamic_cast<OTypePointer *>(resolved_dst);
  if (ptrdst && ptrdst->IsOpaquePointer())
  {
    return resolved_src && (TK_POINTER == resolved_src->kind);
  }
  auto * dst_compound = dynamic_cast<OCompoundType *>(resolved_dst);
  auto * src_compound = dynamic_cast<OCompoundType *>(resolved_src);
  if (dst_compound && src_compound)
  {
    return src_compound->IsSameOrDerivedFrom(dst_compound);
  }
  return (resolved_dst && resolved_src && (resolved_dst == resolved_src));
}

bool OTypeFunc::AnalyzeCallCandidate(const vector<TFuncCallArgMatch> & callargs,
                                     TFuncCallMatchScore & rscore) const
{
  rscore = TFuncCallMatchScore();

  if (callargs.size() < RequiredParamCount())
  {
    return false;
  }

  if (!has_varargs && (callargs.size() > params.size()))
  {
    return false;
  }

  for (size_t i = 0; i < callargs.size(); ++i)
  {
    const TFuncCallArgMatch & callarg = callargs[i];
    if (!callarg.expr || callarg.has_init_diags)
    {
      return false;
    }

    if (i >= params.size())
    {
      rscore.uses_varargs = true;
      continue;
    }

    OFuncParam * fparam = params[i];
    if (!fparam->IsRefLike())
    {
      int conv_cost = g_compiler->GetAssignTypeConversionCost(fparam->ptype, callarg.expr, EXPCF_ALLOW_LAZY_CSTRING | EXPCF_ALLOW_ARRAY_LITERAL_SLICE);
      if (conv_cost < 0)
      {
        return false;
      }

      rscore.conversions += conv_cost;
      continue;
    }

    bool is_null_arg = dynamic_cast<ONullLit *>(callarg.expr);
    if (is_null_arg)
    {
      if (FPM_REFNULL != fparam->mode)
      {
        return false;
      }

      continue;
    }

    OLValueExpr * arglval = dynamic_cast<OLValueExpr *>(callarg.expr);
    OValSym * rootvalsym = (arglval ? g_compiler->GetAssignRootValSym(arglval) : nullptr);
    bool bind_ok = (arglval != nullptr);
    if (bind_ok && rootvalsym)
    {
      if ((VSK_CONST == rootvalsym->kind) || !rootvalsym->IsRefWriteable())
      {
        bind_ok = false;
      }
    }

    if (!bind_ok || !SameRefBindingType(fparam->ptype, callarg.expr->ptype))
    {
      return false;
    }
  }

  for (size_t i = callargs.size(); i < params.size(); ++i)
  {
    if (!params[i]->defvalue)
    {
      return false;
    }

    ++rscore.defaults;
  }

  return true;
}

int OTypeFunc::CompareCallCandidateScore(const TFuncCallMatchScore & left,
                                         const TFuncCallMatchScore & right)
{
  if (left.conversions != right.conversions)
  {
    return (left.conversions < right.conversions ? -1 : 1);
  }

  if (left.defaults != right.defaults)
  {
    return (left.defaults < right.defaults ? -1 : 1);
  }

  if (left.uses_varargs != right.uses_varargs)
  {
    return (left.uses_varargs ? 1 : -1);
  }

  return 0;
}

void OValSymOverloadSet::AddFunc(OValSymFunc * afunc)
{
  if (!afunc)
  {
    return;
  }

  string prefix = (generated_linkage_prefix.empty() ? name : generated_linkage_prefix);
  afunc->generated_linkage_name = prefix + "__ovl" + to_string(funcs.size());
  funcs.push_back(afunc);
}

bool OValSymOverloadSet::WriteDqmIfDecl(ODqmIfWriter & writer)
{
  for (OValSymFunc * fn : funcs)
  {
    if (!fn)
    {
      return writer.Fail(format("Overload set {} has a null function", name));
    }
    if (!fn->WriteDqmIfFunction(writer, false)) return false;
  }
  return true;
}

bool OValSymOverloadSet::WriteDqmIfMethods(ODqmIfWriter & writer)
{
  for (OValSymFunc * fn : funcs)
  {
    if (!fn)
    {
      return writer.Fail(format("Overload set {} has a null method", name));
    }
    if (!fn->WriteDqmIfFunction(writer, true)) return false;
  }
  return true;
}

OType * OValSymOverloadSet::ResolvedRetType() const
{
  if (funcs.empty())
  {
    return nullptr;
  }

  OTypeFunc * ftype = dynamic_cast<OTypeFunc *>(funcs[0] ? funcs[0]->ptype : nullptr);
  return (ftype ? ftype->ResolvedRetType() : nullptr);
}

bool OValSymOverloadSet::HasMatchingReturnType(const OTypeFunc * atype) const
{
  if (!atype)
  {
    return false;
  }

  return (ResolvedRetType() == atype->ResolvedRetType());
}

bool OValSymOverloadSet::HasMatchingOverloadDecl(const OTypeFunc * atype) const
{
  return (nullptr != FindMatchingOverloadDecl(atype));
}

bool OValSymOverloadSet::HasMatchingSignature(const OTypeFunc * atype) const
{
  for (OValSymFunc * fn : funcs)
  {
    OTypeFunc * ftype = dynamic_cast<OTypeFunc *>(fn ? fn->ptype : nullptr);
    if (ftype && ftype->MatchesSignature(atype))
    {
      return true;
    }
  }

  return false;
}

OValSymFunc * OValSymOverloadSet::FindMatchingOverloadDecl(const OTypeFunc * atype) const
{
  if (!atype)
  {
    return nullptr;
  }

  for (OValSymFunc * fn : funcs)
  {
    OTypeFunc * ftype = dynamic_cast<OTypeFunc *>(fn ? fn->ptype : nullptr);
    if (ftype && ftype->MatchesOverloadDeclIdentity(atype))
    {
      return fn;
    }
  }

  return nullptr;
}

EOverloadFuncRefMatch OValSymOverloadSet::FindMatchingSignature(const OTypeFunc * atype, OValSymFunc *& rfunc) const
{
  rfunc = nullptr;
  if (!atype)
  {
    return OFRM_NOT_OVERLOAD;
  }

  for (OValSymFunc * fn : funcs)
  {
    OTypeFunc * ftype = dynamic_cast<OTypeFunc *>(fn ? fn->ptype : nullptr);
    if (ftype && atype->MatchesSignature(ftype))
    {
      if (rfunc)
      {
        rfunc = nullptr;
        return OFRM_AMBIGUOUS;
      }

      rfunc = fn;
    }
  }

  return (rfunc ? OFRM_UNIQUE_MATCH : OFRM_NO_MATCH);
}

void OValSymOverloadSet::ValidateForwardDecls() const
{
  for (OValSymFunc * fn : funcs)
  {
    if (fn)
    {
      fn->ValidateForwardDecl();
    }
  }
}

void ValidateModuleForwardFuncDecls(OModule * module)
{
  if (!module)
  {
    return;
  }

  for (ODecl * decl : module->declarations)
  {
    if (!decl || (DK_VALSYM != decl->kind))
    {
      continue;
    }

    OValSym * vs = decl->pvalsym;
    if (auto * vsfunc = dynamic_cast<OValSymFunc *>(vs))
    {
      vsfunc->ValidateForwardDecl();
    }
    else if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
    {
      ovset->ValidateForwardDecls();
    }
  }
}

LlType * OTypeFunc::CreateLlType()  // do not call GetLlType() until the function arguments fully prepared
{
  vector<LlType *> ll_partypes;
  for (OFuncParam * fpar : params)
  {
    ll_partypes.push_back(fpar->GetLlArgType()->GetLlType());
  }
  LlType *  ll_rettype;
  if (rettype)
  {
    ll_rettype = GetLlRetType()->GetLlType();
  }
  else
  {
    ll_rettype = llvm::Type::getVoidTy(ll_ctx);
  }
  return LlFuncType::get(ll_rettype, ll_partypes, has_varargs);
}

LlDiType * OTypeFunc::CreateDiType()
{
  vector<llvm::Metadata *> di_param_types;

  if (rettype)
  {
    di_param_types.push_back(ResolvedRetType()->GetDiType());
  }
  else
  {
    di_param_types.push_back(nullptr);
  }

#if 0
  // 'this' parameter for methods
  if (hasThis)
      paramTypes.push_back(getDebugType("ptr"));
#endif

  // Regular parameters
  for (OFuncParam * fpar : params)
  {
    di_param_types.push_back(fpar->GetLlArgType()->GetDiType());
  }

  return di_builder->createSubroutineType(di_builder->getOrCreateTypeArray(di_param_types));
}

void OValSymFunc::GenGlobalDecl(bool apublic, OValue * ainitval)
{
  //print("Found function declaration \"{}\"\n", ptfunc->name);

  llvm::GlobalValue::LinkageTypes  linktype;
  if (is_external) {
    linktype = llvm::GlobalValue::LinkageTypes::ExternalLinkage;
  } else if (attr_has_linkage_name) {
    linktype = llvm::GlobalValue::LinkageTypes::ExternalLinkage;
  } else {
    linktype = (apublic ? llvm::GlobalValue::LinkageTypes::ExternalLinkage
                        : llvm::GlobalValue::LinkageTypes::InternalLinkage);
  }

  LlFuncType *  ll_functype = (LlFuncType *)(ptype->GetLlType());  // calls CreateLlType()

  string ll_name;
  if (is_external)
  {
    ll_name = (external_linkage_name.empty() ? name : external_linkage_name);
  }
  else
  {
    string symbol_name = generated_linkage_name.empty() ? name : generated_linkage_name;
    ll_name = GetLinkageName(apublic, 'F', symbol_name);
  }

  if (apublic && (is_external || !has_body))
  {
    if (LlFunction * existing = ll_module->getFunction(ll_name))
    {
      ll_func = existing;
      return;
    }
  }

  ll_func = LlFunction::Create(ll_functype, linktype, ll_name, ll_module);
  if (!attr_section_name.empty())
  {
    ll_func->setSection(attr_section_name);
  }
  if (attr_is_always_inline)
  {
    ll_func->addFnAttr(llvm::Attribute::AlwaysInline);
  }
  else if (attr_is_inline)
  {
    ll_func->addFnAttr(llvm::Attribute::InlineHint);
  }
  if (attr_is_noinline)
  {
    ll_func->addFnAttr(llvm::Attribute::NoInline);
  }

  //ll_functions[ptfunc->name] = ll_func;
}

bool OValSymFunc::CheckForwardDeclMatch(OValSymFunc * other) const
{
  OTypeFunc * this_type = dynamic_cast<OTypeFunc *>(ptype);
  OTypeFunc * other_type = dynamic_cast<OTypeFunc *>(other ? other->ptype : nullptr);
  if (!this_type || !other_type)
  {
    return false;
  }

  bool this_special = (object_specfunc_kind != OSF_NONE) || (special_kind != SFK_NONE);
  bool other_special = (other->object_specfunc_kind != OSF_NONE) || (other->special_kind != SFK_NONE);

  if (this_special != other_special)
  {
    if (this_special)
    {
      g_compiler->ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, "Missing '*' for special function implementation");
    }
    else
    {
      g_compiler->ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, "Unexpected '*' for regular function implementation");
    }
    return false;
  }

  if (object_specfunc_kind != other->object_specfunc_kind || special_kind != other->special_kind)
  {
    g_compiler->ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, "Special function kind mismatch");
    return false;
  }

  if (this_type->MatchesSignature(other_type))
  {
    return true;
  }

  g_compiler->Error(DQERR_FUNCSIG_TYPEMISM, FuncTypeName(this_type), FuncTypeName(other_type));
  return false;
}

void OValSymFunc::MergeForwardDeclFrom(OValSymFunc * other, bool copy_param_names)
{
  OTypeFunc * this_type = dynamic_cast<OTypeFunc *>(ptype);
  OTypeFunc * other_type = dynamic_cast<OTypeFunc *>(other ? other->ptype : nullptr);
  if (!this_type || !other_type)
  {
    return;
  }

  this_type->MergeForwardDeclFrom(other_type, copy_param_names);

  is_external = other->is_external;
  external_linkage_name = other->external_linkage_name;
  if (other->attr_align)
  {
    attr_align = other->attr_align;
  }
  if (!other->attr_section_name.empty())
  {
    attr_section_name = other->attr_section_name;
  }
  attr_is_override = attr_is_override || other->attr_is_override;
  attr_is_virtual  = attr_is_virtual || other->attr_is_virtual;
  attr_is_abstract = attr_is_abstract || other->attr_is_abstract;
  attr_is_final    = attr_is_final || other->attr_is_final;
  attr_is_inline   = attr_is_inline || other->attr_is_inline;
  attr_is_always_inline = attr_is_always_inline || other->attr_is_always_inline;
  attr_is_noinline = attr_is_noinline || other->attr_is_noinline;
  special_kind = other->special_kind;
}

void OValSymFunc::ValidateForwardDecl() const
{
  if (IsForwardDecl() && !attr_is_abstract)
  {
    g_compiler->Error(DQERR_FUNC_FORWARD_NOT_DEFINED, name, const_cast<OScPosition *>(&scpos));
  }
}

void OValSymFunc::ResetBodyScope(OScope * aparentscope)
{
  for (OValSym * a : args)
  {
    delete a;
  }
  args.clear();

  if (vsresult)
  {
    delete vsresult;
    vsresult = nullptr;
  }

  delete body;
  body = new OStmtBlock(aparentscope, "function_"+name);
  has_body = false;
}

void OValSymFunc::GenerateFuncBody()
{
  if (is_external || !has_body)
  {
    return;  // external functions and unresolved forward declarations have no body to generate
  }

  // Get the pre-declared function
  if (!ll_func)
  {
    throw logic_error("GenerateFuncBody: ll_func declaration is missing");
  }

  OTypeFunc * tfunc = (OTypeFunc *)ptype;

  if (g_opt.dbg_info)
  {
    llvm::DISubroutineType * di_func_type = static_cast<llvm::DISubroutineType *>(tfunc->GetDiType()); // debugFuncType = getDebugFuncType(f);
    di_func = di_builder->createFunction(
        scpos.scfile->di_file, name, name, scpos.scfile->di_file, scpos.line,
        di_func_type, scpos.line,
        llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagDefinition
    );
    body->scope->di_scope = di_func;
    ll_func->setSubprogram(di_func);
  }

  // Create entry block and generate body
  auto * entry = LlBasicBlock::Create(ll_ctx, "entry", ll_func);
  ll_builder.SetInsertPoint(entry);
  if (g_opt.dbg_info)
  {
    ll_builder.SetCurrentDebugLocation(llvm::DILocation::get(ll_ctx, scpos.line, scpos.col, di_func));
  }

  // Create implicit 'result' variable for functions with return type
  LlType * ll_rettype = nullptr;
  if (vsresult)
  {
    ll_rettype = tfunc->GetLlRetType()->GetLlType();
    auto * result_alloca = CreateEntryBlockAlloca(ll_rettype, nullptr, "result");
    result_alloca->setAlignment(llvm::Align(EffectiveStorageAlign(tfunc->GetLlRetType())));
    vsresult->ll_value = result_alloca;
    ll_builder.CreateStore(llvm::Constant::getNullValue(ll_rettype), vsresult->ll_value);
    if (vsresult->ptype && TK_ANYVALUE == vsresult->ptype->ResolveAlias()->kind)
    {
      GenerateAnyValueCreate(body->scope, vsresult->ll_value);
    }
    if (g_opt.dbg_info)
    {
      llvm::DILocalVariable * di_var = di_builder->createAutoVariable(
          di_func, vsresult->name, scpos.scfile->di_file, scpos.line, vsresult->ptype->GetDiType() );
      di_builder->insertDeclare(vsresult->ll_value, di_var, di_builder->createExpression(),
          llvm::DILocation::get(ll_ctx, scpos.line, scpos.col, di_func), ll_builder.GetInsertBlock() );
    }
  }

  // Create allocas for parameters
  int i = 0;
  for (auto & arg : ll_func->args())
  {
    OFuncParam *  fpar  = tfunc->params[i];
    OValSym *     vsarg = args[i];

    arg.setName(fpar->name);
    OType * ll_arg_type = fpar->GetLlArgType();
    auto * arg_alloca = CreateEntryBlockAlloca(ll_arg_type->GetLlType(), nullptr, fpar->name);
    arg_alloca->setAlignment(llvm::Align(EffectiveStorageAlign(ll_arg_type)));
    vsarg->ll_value = arg_alloca;
    if (!fpar->IsRefLike() && fpar->ptype && TK_ANYVALUE == fpar->ptype->ResolveAlias()->kind)
    {
      LlValue * raw_arg_alloca = CreateEntryBlockAlloca(ll_arg_type->GetLlType(), nullptr, fpar->name + ".raw");
      ll_builder.CreateStore(&arg, raw_arg_alloca);
      GenerateAnyValueCreate(body->scope, vsarg->ll_value);
      GenerateAnyValueCopy(body->scope, vsarg->ll_value, raw_arg_alloca);
    }
    else
    {
      ll_builder.CreateStore(&arg, vsarg->ll_value);
    }
    if (!fpar->IsRefLike() && fpar->ptype && TK_DYNSTR == fpar->ptype->ResolveAlias()->kind)
    {
      GenerateStringIncRef(body->scope, vsarg->ll_value);
    }
    if (g_opt.dbg_info)
    {
      llvm::DILocalVariable * di_var = di_builder->createParameterVariable(
          di_func, fpar->name, i + 1, vsarg->scpos.scfile->di_file, vsarg->scpos.line,
          fpar->GetLlArgType()->GetDiType()
      );
      di_builder->insertDeclare(vsarg->ll_value, di_var,
          di_builder->createExpression(),
          llvm::DILocation::get(ll_ctx, vsarg->scpos.line, vsarg->scpos.col, di_func),
          ll_builder.GetInsertBlock()
      );
    }
    ++i;
  }

  auto * owner_object = dynamic_cast<OTypeObject *>(owner_compound_type);

  if (OSF_CREATE == object_specfunc_kind && owner_object && receiver_arg)
  {
    OLValueVar this_expr(receiver_arg);
    owner_object->GenerateVTableStore(this_expr.GenerateAddress(body->scope));
  }

  if (!owner_object || !owner_object->base_type)
  {
    if (OSF_CREATE == object_specfunc_kind && owner_object && receiver_arg)
    {
      OLValueVar this_expr(receiver_arg);
      owner_object->GenerateFieldInitializers(body->scope, this_expr.GenerateAddress(body->scope));
    }
  }

  // STATEMENTS
  body->Generate();

  // Add implicit return
  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    bool has_inherited_destroy = owner_object && owner_object->base_type
        && owner_object->GetBaseObject()->FindSpecialMethod(OSF_DESTROY);
    if (!owner_object || !owner_object->base_type
        || (OSF_DESTROY == object_specfunc_kind && !has_inherited_destroy))
    {
      if (OSF_DESTROY == object_specfunc_kind && owner_object && receiver_arg)
      {
        OLValueVar this_expr(receiver_arg);
        owner_object->GenerateFieldDestructors(body->scope, this_expr.GenerateAddress(body->scope));
      }
    }
    GenerateFuncRet();
  }

  verifyFunction(*ll_func);
}

void OValSymFunc::GenerateFuncRet()
{
  LlType * ll_rettype = nullptr;
  if (vsresult)
  {
    auto * tfunc = static_cast<OTypeFunc *>(ptype);
    ll_rettype = tfunc->GetLlRetType()->GetLlType();
  }

  if (g_opt.dbg_info)  // this jumps to the "endfunc"
  {
    ll_builder.SetCurrentDebugLocation(llvm::DILocation::get(ll_ctx, scpos_endfunc.line, scpos_endfunc.col, di_func));
  }

  if (!ll_rettype)
  {
    ll_builder.CreateRetVoid();
  }
  else
  {
    // Return the value of 'result'
    LlValue * ll_result = ll_builder.CreateLoad(ll_rettype, vsresult->ll_value, "result");
    ll_builder.CreateRet(ll_result);
  }
}

string FuncTypeName(OTypeFunc * sigtype)  // argument can be nullptr too
{
  string result = "function(";
  bool first = true;
  if (sigtype)
  {
    for (OFuncParam * param : sigtype->params)
    {
      if (!first)
      {
        result += ", ";
      }

      result += param->name;
      result += " : ";
      if (FPM_REF == param->mode)       result += "ref ";
      else if (FPM_REFIN == param->mode)  result += "refin ";
      else if (FPM_REFOUT == param->mode) result += "refout ";
      else if (FPM_REFNULL == param->mode) result += "refnull ";
      result += (param->ptype ? param->ptype->name : "?");
      first = false;
    }

    if (sigtype->has_varargs)
    {
      if (!first)
      {
        result += ", ";
      }
      result += "...";
    }
  }

  result += ")";

  if (sigtype && sigtype->rettype)
  {
    result += " -> ";
    result += sigtype->rettype->name;
  }

  return result;
}

string FuncRefTypeName(OTypeFunc * sigtype, bool object_ref)
{
  string result = FuncTypeName(sigtype);
  if (object_ref)
  {
    result += " of object";
  }
  return result;
}

OExpr * OValueFuncRef::UnwrapConstExpr(OExpr * expr) const
{
  OExpr * current = expr;
  while (auto * conv = dynamic_cast<OExprTypeConv *>(current))
  {
    current = conv->src;
  }
  return current;
}

OTypeFuncRef::OTypeFuncRef(OTypeFunc * afunctype, const string & aname, bool aobject_ref)
:
  super((aname.empty() ? FuncRefTypeName(afunctype, aobject_ref) : aname), TK_FUNCREF),
  functype(afunctype),
  object_ref(aobject_ref)
{
  bytesize = (object_ref ? 2 * TARGET_PTRSIZE : TARGET_PTRSIZE);
  alignsize = TARGET_PTRSIZE;
}

OTypeFuncRef::~OTypeFuncRef()
{
  delete functype;
  functype = nullptr;
}

bool OTypeFuncRef::WriteDqmIfTypeSpec(ODqmIfWriter & writer)
{
  if (!functype)
  {
    return writer.Fail(format("Function reference type {} has no signature", name));
  }
  if (!writer.AddRecEmpty(object_ref ? DQMIF_TYPE_SPEC_OBJFUNCREF : DQMIF_TYPE_SPEC_FUNCREF)) return false;
  if (!functype->WriteDqmIfSignatureRecords(writer)) return false;
  return writer.AddRecEmpty(DQMIF_TYPE_SPEC_END);
}

LlType * OTypeFuncRef::CreateLlType()
{
  if (object_ref)
  {
    LlType * ll_ptr = llvm::PointerType::get(ll_ctx, 0);
    return llvm::StructType::get(ll_ctx, {ll_ptr, ll_ptr});
  }
  return llvm::PointerType::get(ll_ctx, 0);
}

LlDiType * OTypeFuncRef::CreateDiType()
{
  if (object_ref)
  {
    return di_builder->createStructType(
        nullptr,
        name,
        nullptr,
        0,
        bytesize * 8,
        alignsize * 8,
        llvm::DINode::FlagZero,
        nullptr,
        di_builder->getOrCreateArray({})
    );
  }

  return di_builder->createPointerType(
      functype ? functype->GetDiType() : nullptr,
      bytesize * 8
  );
}

OValue * OTypeFuncRef::CreateValue()
{
  return new OValueFuncRef(this, true);
}

bool OTypeFuncRef::CanAccept(OType * srctype) const
{
  if (!srctype)
  {
    return false;
  }

  OType * resolved_src = srctype->ResolveAlias();
  if (!resolved_src)
  {
    return false;
  }

  if (auto * src_cb = dynamic_cast<OTypeFuncRef *>(resolved_src))
  {
    return object_ref == src_cb->object_ref
        && functype && src_cb->functype && functype->MatchesSignature(src_cb->functype);
  }

  if (auto * src_ptr = dynamic_cast<OTypePointer *>(resolved_src))
  {
    return src_ptr->IsNullPointer();
  }

  if (auto * src_func = dynamic_cast<OTypeFunc *>(resolved_src))
  {
    return !object_ref && functype && functype->MatchesSignature(src_func);
  }

  return false;
}

bool OTypeFuncRef::CanAcceptMethod(OValSymFunc * srcfunc) const
{
  if (!object_ref || !functype || !srcfunc || !srcfunc->owner_compound_type)
  {
    return false;
  }

  OTypeFunc * srcsig = dynamic_cast<OTypeFunc *>(srcfunc->ptype);
  if (!srcsig || srcsig->params.empty())
  {
    return false;
  }

  if (functype->has_varargs != srcsig->has_varargs)
  {
    return false;
  }

  if (functype->ResolvedRetType() != srcsig->ResolvedRetType())
  {
    return false;
  }

  if (functype->params.size() + 1 != srcsig->params.size())
  {
    return false;
  }

  for (size_t i = 0; i < functype->params.size(); ++i)
  {
    OFuncParam * dst = functype->params[i];
    OFuncParam * src = srcsig->params[i + 1];
    if (!dst || !src || !dst->ptype || !src->ptype)
    {
      return false;
    }
    if (dst->mode != src->mode)
    {
      return false;
    }
    if (dst->ptype->ResolveAlias() != src->ptype->ResolveAlias())
    {
      return false;
    }
  }

  return true;
}

EOverloadFuncRefMatch OTypeFuncRef::FindAcceptingOverload(OExpr * src, OValSymFunc *& rfunc) const
{
  rfunc = nullptr;

  if (object_ref)
  {
    auto * bound_ov = dynamic_cast<OBoundMethodOverloadExpr *>(src);
    OValSymOverloadSet * ovset = (bound_ov ? bound_ov->ovset : nullptr);
    if (!ovset || !functype)
    {
      return OFRM_NOT_OVERLOAD;
    }

    bool found = false;
    for (OValSymFunc * fn : ovset->funcs)
    {
      if (!CanAcceptMethod(fn))
      {
        continue;
      }
      if (found)
      {
        rfunc = nullptr;
        return OFRM_AMBIGUOUS;
      }
      rfunc = fn;
      found = true;
    }
    return (found ? OFRM_UNIQUE_MATCH : OFRM_NO_MATCH);
  }

  auto * varref = dynamic_cast<OLValueVar *>(src);
  auto * ovset = dynamic_cast<OValSymOverloadSet *>(varref ? varref->pvalsym : nullptr);
  if (!ovset || !functype)
  {
    return OFRM_NOT_OVERLOAD;
  }

  return ovset->FindMatchingSignature(functype, rfunc);
}

LlValue * OTypeFuncRef::GenerateConversion(OScope * scope, OExpr * src)
{
  if (!src)
  {
    return nullptr;
  }

  if (object_ref)
  {
    auto * src_ptr = dynamic_cast<OTypePointer *>(src->ResolvedType());
    if (src_ptr && src_ptr->IsNullPointer())
    {
      return llvm::ConstantAggregateZero::get(GetLlType());
    }
    if (dynamic_cast<OBoundMethodExpr *>(src) || dynamic_cast<OBoundMethodOverloadExpr *>(src))
    {
      src->ptype = this;
      return src->Generate(scope);
    }
    return src->Generate(scope);
  }

  if (auto * varref = dynamic_cast<OLValueVar *>(src))
  {
    if (auto * vsfunc = dynamic_cast<OValSymFunc *>(varref->pvalsym))
    {
      if (!vsfunc->ll_func)
      {
        throw logic_error("FuncRef conversion target function is not prepared in LLVM");
      }
      return vsfunc->ll_func;
    }
  }

  return src->Generate(scope);
}

LlConst * OValueFuncRef::CreateLlConst()
{
  auto * fref_type = dynamic_cast<OTypeFuncRef *>(ptype ? ptype->ResolveAlias() : nullptr);
  if (fref_type && fref_type->object_ref)
  {
    if (!is_null)
    {
      throw logic_error("Object FuncRef constants only support null values");
    }
    return llvm::ConstantAggregateZero::get(fref_type->GetLlType());
  }

  if (is_null)
  {
    return llvm::ConstantPointerNull::get(llvm::PointerType::get(ll_ctx, 0));
  }

  if (!target_func || !target_func->ll_func)
  {
    throw logic_error("FuncRef constant function target is not prepared in LLVM");
  }

  return target_func->ll_func;
}

bool OValueFuncRef::WriteDqmIfValue(ODqmIfWriter & writer)
{
  if (!is_null)
  {
    return writer.Fail("Only null function reference constants are supported in DQM interface generation");
  }
  return writer.AddRecU64(DQMIF_VALUE_INLINE, 0);
}

bool OValueFuncRef::CalculateConstant(OExpr * expr, bool emit_errors)
{
  is_null = true;
  target_func = nullptr;

  OExpr * plain = UnwrapConstExpr(expr);
  if (!plain)
  {
    if (emit_errors)
    {
      g_compiler->Error(DQERR_CONSTEXPR_INVALID_FOR, ptype->name);
    }
    return false;
  }

  if (dynamic_cast<ONullLit *>(plain))
  {
    return true;
  }

  if (auto * varref = dynamic_cast<OLValueVar *>(plain))
  {
    target_func = dynamic_cast<OValSymFunc *>(varref->pvalsym);
    if (target_func)
    {
      is_null = false;
      return true;
    }
  }

  if (emit_errors)
  {
    g_compiler->Error(DQERR_CONSTEXPR_INVALID_FOR, ptype->name);
  }
  return false;
}


bool OTypeFuncRef::ConvertFromExpr(OExpr ** rexpr, uint32_t aflags)
{
  OExpr * src = *rexpr;
  OType * resolved_src = src->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (TK_FUNCREF != tks)
  {
    OValSymFunc * matched_func = nullptr;
    EOverloadFuncRefMatch ovmatch = FindAcceptingOverload(src, matched_func);
    if (OFRM_UNIQUE_MATCH == ovmatch)
    {
      if (auto * bound_ov = dynamic_cast<OBoundMethodOverloadExpr *>(src))
      {
        bound_ov->matched_func = matched_func;
        *rexpr = new OExprTypeConv(this, src);
      }
      else
      {
        delete src;
        *rexpr = new OExprTypeConv(this, new OLValueVar(matched_func));
      }
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if ((OFRM_NO_MATCH == ovmatch) || (OFRM_AMBIGUOUS == ovmatch))
    {
      if (aflags & EXPCF_GENERATE_ERRORS)
      {
        auto * srcvar = dynamic_cast<OLValueVar *>(src);
        string srcname = (srcvar && srcvar->pvalsym ? srcvar->pvalsym->name : string("?"));
        if (OFRM_AMBIGUOUS == ovmatch)
        {
          g_compiler->ErrorTxt(DQERR_OVERLOAD_AMBIGUOUS, format("Overloaded function \"{}\" is ambiguous for callback type \"{}\"", srcname, this->name));
        }
        else
        {
          g_compiler->ErrorTxt(DQERR_OVERLOAD_NO_MATCH, format("No overload of function \"{}\" matches callback type \"{}\"", srcname, this->name));
        }
      }
      return false;
    }

    if (CanAccept(resolved_src))
    {
      *rexpr = new OExprTypeConv(this, src);
      FoldExprTreeAfterTypeRewrite(rexpr);
      return true;
    }

    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_FUNCSIG_TYPEMISM, this->name, resolved_src->name);
    return false;
  }

  if (!CanAccept(resolved_src))
  {
    if (aflags & EXPCF_GENERATE_ERRORS) g_compiler->Error(DQERR_FUNCSIG_TYPEMISM, this->name, resolved_src->name);
    return false;
  }

  if (this != resolved_src)
  {
    *rexpr = new OExprTypeConv(this, src);
    FoldExprTreeAfterTypeRewrite(rexpr);
  }

  return true;
}

int OTypeFuncRef::GetConversionCostFromExpr(OExpr * expr, uint32_t aflags)
{
  OType * resolved_src = expr->ResolvedType();
  ETypeKind tks = resolved_src->kind;

  if (TK_FUNCREF != tks)
  {
    OValSymFunc * matched_func = nullptr;
    EOverloadFuncRefMatch ovmatch = FindAcceptingOverload(expr, matched_func);
    if (OFRM_UNIQUE_MATCH == ovmatch) return 1;
    if ((OFRM_NO_MATCH == ovmatch) || (OFRM_AMBIGUOUS == ovmatch)) return -1;
    return (CanAccept(resolved_src) ? 1 : -1);
  }

  if (!CanAccept(resolved_src)) return -1;
  return ((this == resolved_src) ? 0 : 1);
}
