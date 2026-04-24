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
#include "dqc.h"
#include "ll_defs.h"

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

OFuncParam * OTypeFunc::AddParam(const string aname, OType * atype, EParamMode amode)
{
  OFuncParam * result = new OFuncParam(aname, atype, amode);
  params.push_back(result);
  return result;
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

void OValSymOverloadSet::AddFunc(OValSymFunc * afunc)
{
  if (!afunc)
  {
    return;
  }

  afunc->generated_linkage_name = name + "__ovl" + to_string(funcs.size());
  funcs.push_back(afunc);
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
    ll_rettype = ResolvedRetType()->GetLlType();
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

  llvm::GlobalValue::LinkageTypes  linktype =
    (apublic ? llvm::GlobalValue::LinkageTypes::ExternalLinkage
              : llvm::GlobalValue::LinkageTypes::InternalLinkage);

  LlFuncType *  ll_functype = (LlFuncType *)(ptype->GetLlType());  // calls CreateLlType()

  string ll_name = generated_linkage_name;
  if (ll_name.empty())
  {
    ll_name = (external_linkage_name.empty() ? name : external_linkage_name);
  }
  ll_func = LlFunction::Create(ll_functype, linktype, ll_name, ll_module);
  if (!attr_section_name.empty())
  {
    ll_func->setSection(attr_section_name);
  }

  //ll_functions[ptfunc->name] = ll_func;
}

void OValSymFunc::GenerateFuncBody()
{
  if (is_external)
  {
    return;  // external functions have no body to generate
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
    ll_rettype = vsresult->ptype->GetLlType();
    vsresult->ll_value = ll_builder.CreateAlloca(ll_rettype, nullptr, "result");
    ll_builder.CreateStore(llvm::Constant::getNullValue(ll_rettype), vsresult->ll_value);
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
    vsarg->ll_value = ll_builder.CreateAlloca(fpar->GetLlArgType()->GetLlType(), nullptr, fpar->name);
    ll_builder.CreateStore(&arg, vsarg->ll_value);
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

  // STATEMENTS
  body->Generate();

  // Add implicit return
  if (!ll_builder.GetInsertBlock()->getTerminator())
  {
    GenerateFuncRet();
  }

  verifyFunction(*ll_func);
}

void OValSymFunc::GenerateFuncRet()
{
  LlType * ll_rettype = nullptr;
  if (vsresult)
  {
    ll_rettype = vsresult->ptype->GetLlType();
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

      if (FPM_REF == param->mode)       result += "ref ";
      else if (FPM_REFIN == param->mode)  result += "refin ";
      else if (FPM_REFOUT == param->mode) result += "refout ";
      else if (FPM_REFNULL == param->mode) result += "refnull ";

      result += param->name;
      result += " : ";
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

static OExpr * UnwrapFuncRefConstExpr(OExpr * expr)
{
  OExpr * current = expr;
  while (auto * conv = dynamic_cast<OExprTypeConv *>(current))
  {
    current = conv->src;
  }
  return current;
}

OTypeFuncRef::OTypeFuncRef(OTypeFunc * afunctype, const string & aname)
:
  super((aname.empty() ? FuncTypeName(afunctype) : aname), TK_FUNCREF),
  functype(afunctype)
{
  bytesize = TARGET_PTRSIZE;
}

OTypeFuncRef::~OTypeFuncRef()
{
  delete functype;
  functype = nullptr;
}

LlType * OTypeFuncRef::CreateLlType()
{
  return llvm::PointerType::get(ll_ctx, 0);
}

LlDiType * OTypeFuncRef::CreateDiType()
{
  return di_builder->createPointerType(
      functype ? functype->GetDiType() : nullptr,
      bytesize * 8
  );
}

OValue * OTypeFuncRef::CreateValue()
{
  return new OValueFuncRef(this, true);
}

LlValue * OTypeFuncRef::GenerateConversion(OScope * scope, OExpr * src)
{
  if (!src)
  {
    return nullptr;
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

bool OValueFuncRef::CalculateConstant(OExpr * expr, bool emit_errors)
{
  is_null = true;
  target_func = nullptr;

  OExpr * plain = UnwrapFuncRefConstExpr(expr);
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
