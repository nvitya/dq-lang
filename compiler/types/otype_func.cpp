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

OType * OTypeFunc::ResolvedRetType() const
{
  return (rettype ? rettype->ResolveAlias() : nullptr);
}

LlType * OTypeFunc::CreateLlType()  // do not call GetLlType() until the function arguments fully prepared
{
  vector<LlType *> ll_partypes;
  for (OFuncParam * fpar : params)
  {
    ll_partypes.push_back(fpar->ptype->GetLlType());
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
    di_param_types.push_back(fpar->ptype->GetDiType());
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

  ll_func     = LlFunction::Create(ll_functype, linktype, name, ll_module);

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
    ll_builder.CreateStore(llvm::ConstantInt::get(ll_rettype, 0), vsresult->ll_value);
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
    vsarg->ll_value = ll_builder.CreateAlloca(fpar->ptype->GetLlType(), nullptr, fpar->name);
    ll_builder.CreateStore(&arg, vsarg->ll_value);
    if (g_opt.dbg_info)
    {
      llvm::DILocalVariable * di_var = di_builder->createParameterVariable(
          di_func, fpar->name, i + 1, vsarg->scpos.scfile->di_file, vsarg->scpos.line,
          fpar->ptype->GetDiType()
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
