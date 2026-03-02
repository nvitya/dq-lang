/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dq_module.h
 * authors: nvitya
 * created: 2026-02-07
 * brief:   DQ Module Classes
 */

#pragma once

#include <string>
#include <format>
#include "symbols.h"
#include "scope_builtins.h"

using namespace std;

enum EDeclKind
{
  DK_TYPE,
  DK_VALSYM
};

class ODecl  // module top level declaration
{
public:
  EDeclKind   kind;
  bool        ispublic;
  OValue *    initvalue = nullptr;  // valid for the variable declarations only

  union
  {
    OType *      ptype;
    OValSym *    pvalsym;
  };

  ODecl(bool aispublic, OType * t)
  :
    kind(DK_TYPE),
    ispublic(aispublic),
    ptype(t)
  {
  }

  ODecl(bool aispublic, OValSym * v)
  :
    kind(DK_VALSYM),
    ispublic(aispublic),
    pvalsym(v)
  {
    initvalue = v->ptype->CreateValue();
  }

  virtual ~ODecl()
  {
    delete initvalue;
  }
};

class OModule
{
public:
  //string           name;

  vector<ODecl *>  declarations;

  OScope *         scope_pub;
  OScope *         scope_priv;

  LlDiScope *      di_scope = nullptr;

  OModule()
  {
    scope_pub  = new OScope(g_builtins, "module_pub");
    scope_priv = new OScope(scope_pub,  "module_priv");
  }

  virtual ~OModule()
  {
    delete scope_priv;
    delete scope_pub;
  }

  ODecl * DeclareType(bool apublic, OType * atype);
  ODecl * DeclareValSym(bool apublic, OValSym * avalsym);

  bool TypeDeclared(const string aname, OType ** rtype = nullptr);
  bool ValSymDeclared(const string aname, OValSym ** rvalsym = nullptr);
};

extern OModule *  g_module;

void init_dq_module();