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
#include <vector>
#include "symbols.h"
#include "scope_builtins.h"
#include "module_intf.h"

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
    initvalue = (v && v->ptype ? v->ptype->CreateValue() : nullptr);
  }

  virtual ~ODecl()
  {
    delete initvalue;
  }
};

class OModule : public OModuleIntf
{
private:
  typedef OModuleIntf  super;

public:
  vector<ODecl *>  declarations;
  vector<OModuleIntf *>  loaded_modules;
  vector<string>   link_module_artifacts;

  OScope *         scope_priv;

  LlDiScope *      di_scope = nullptr;

  OModule()
  :
    super(g_builtins, "module_pub")
  {
    scope_priv = new OScope(scope_pub,  "module_priv");
  }

  virtual ~OModule()
  {
    for (OModuleIntf * intf : loaded_modules)
    {
      delete intf;
    }
    delete scope_priv;
  }

  ODecl * DeclareType(bool apublic, OType * atype);
  ODecl * DeclareValSym(bool apublic, OValSym * avalsym);
  ODecl * DeclareHiddenValSym(bool apublic, OValSym * avalsym);
  bool UseCompiledModule(const string & module_path, const string & namespace_name,
                         const string & artifact_path, OScope * amerge_scope, bool ais_private,
                         EModuleUseMergeMode amerge_mode = MUM_ALL,
                         const vector<string> & asymbol_names = {});

  bool TypeDeclared(const string aname, OType ** rtype = nullptr);
  bool ValSymDeclared(const string aname, OValSym ** rvalsym = nullptr);
};

extern OModule *  g_module;

void init_dq_module();
