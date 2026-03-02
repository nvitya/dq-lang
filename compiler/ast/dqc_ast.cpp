/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_ast.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include <print>
#include <format>

#include "dqc_ast.h"

ODqCompAst::ODqCompAst()
{
}

ODqCompAst::~ODqCompAst()
{
}

ODecl * ODqCompAst::AddDeclVar(OScPosition & scpos, string aid, OType * atype)
{
  OValSym * pvalsym = new OValSym(scpos, aid, atype, VSK_VARIABLE);
  pvalsym->scpos.Assign(scpos);

  ODecl * result = g_module->DeclareValSym(section_public, pvalsym);

  // TODO: add initialization

  print("{}: ", scpos.Format());
  print("AddVarDecl(): var {} : {}", aid, atype->name);
  print("\n");

  return result;
}

ODecl * ODqCompAst::AddDeclConst(OScPosition & scpos, string aid, OType * atype, OValue * avalue)
{
  OValSym * pvalsym = new OValSymConst(scpos, aid, atype, avalue);
  pvalsym->scpos.Assign(scpos);

  ODecl * result = g_module->DeclareValSym(section_public, pvalsym);

  print("{}: ", scpos.Format());
  print("AddConstDecl(): var {} : {}", aid, atype->name);
  print("\n");

  return result;
}

ODecl * ODqCompAst::AddDeclFunc(OScPosition & scpos, OValSymFunc * avsfunc)
{
  ODecl * result = g_module->DeclareValSym(section_public, avsfunc);

  print("{}: ", scpos.Format());
  print("AddDeclFunc(): {}", avsfunc->name);

  avsfunc->scpos.Assign(scpos);

  OTypeFunc * tfunc = (OTypeFunc *)(avsfunc->ptype);

  // push the parameters into the scope
  for (OFuncParam * fp : tfunc->params)
  {
    OValSym * vsarg = new OValSym(scpos, fp->name, fp->ptype, VSK_PARAMETER);
    avsfunc->args.push_back(vsarg);
    avsfunc->body->scope->DefineValSym(vsarg);
  }

  // add the implicit result variable
  if (tfunc->rettype)
  {
    avsfunc->vsresult = new OValSym(scpos, "result", tfunc->rettype, VSK_VARIABLE);
    avsfunc->body->scope->DefineValSym(avsfunc->vsresult);
  }

  print("\n");

  return result;
}
