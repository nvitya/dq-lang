/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_function.h
 * authors: nvitya
 * created: 2026-02-07
 * brief:   function type
 */

#pragma once

#include <vector>
#include "symbols.h"
#include "expressions.h"
#include "statements.h"

using namespace std;

class OValSymFunc;

enum EParamMode
{
  FPM_VALUE,
  FPM_REF,
  FPM_IN,
  FPM_OUT
};

class OFuncParam
{
public:
  string              name;
  OType *             ptype;
  EParamMode          mode;
  OValSym *           defvalue = nullptr;

  OFuncParam(const string aname, OType * atype, EParamMode amode = FPM_VALUE)
  :
    name(aname),
    ptype(atype),
    mode(amode)
  {
  }

  virtual ~ OFuncParam() {}
};

// A function signature is a type (TK_FUNCTION).
// It describes the parameter types, return type, calling convention, etc.
// This is what you need when you declare type Callback = function(x: int) -> int;
// or check assignment compatibility between function pointers.

class OTypeFunc : public OType
{
private:
  using        super = OType;

public:
  OType *               rettype;
  vector<OFuncParam *>  params;

  OTypeFunc(const string aname, OType * arettype = nullptr)
  :
    super(aname, TK_FUNCTION),
    rettype(arettype)
  {
  }

  ~OTypeFunc()
  {
    for (OFuncParam * fp : params)
    {
      delete fp;
    }
  }

  OFuncParam *  AddParam(const string aname, OType * atype, EParamMode amode = FPM_VALUE);
  bool          ParNameValid(const string aname);

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
};


// A function declaration (a named function in a module or object) is a value symbol (SK_FUNCTION)
// It isa named entity in a scope that can be called, referenced, assigned to a function pointer, etc.
// Its .type member points to a function signature type of OTypeFunc.

class OValSymFunc : public OValSym
{
private:
  using              super = OValSym;

public:
  vector<OValSym *>  args;  // will be filled in GenerateFuncBody()
  OValSym *          vsresult = nullptr;
  OScPosition        scpos_endfunc;
  OStmtBlock *       body;

  LlFunction *       ll_func = nullptr;
  LlDiSubPrg *       di_func = nullptr;

  OValSymFunc(OScPosition & apos, const string aname, OTypeFunc * atype, OScope * aparentscope = nullptr)
  :
    super(apos, aname, atype, VSK_FUNCTION)
  {
    body  = new OStmtBlock(aparentscope, "function_"+aname);

    if (atype->rettype)
    {
      vsresult = atype->CreateValSym(apos, "result");
      body->scope->DefineValSym(vsresult);
    }
  }

  virtual ~OValSymFunc()
  {
    for (OValSym * a : args)
    {
      delete a;
    }
    if (vsresult)  delete vsresult;
    delete body;
  }

  void GenGlobalDecl(bool apublic, OValue * ainitval = nullptr) override;

  void GenerateFuncBody();
  void GenerateFuncRet();
};
