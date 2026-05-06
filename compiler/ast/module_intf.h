/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    module_intf.h
 * authors: nvitya
 * created: 2026-05-04
 * brief:   DQ Module Interface Class
 */

#pragma once

#include <string>
#include <format>
#include <vector>
#include "symbols.h"

using namespace std;

enum EIntfDeclKind
{
  IDK_TYPE,
  IDK_VALSYM
};

class OIntfDecl
{
public:
  EIntfDeclKind  kind;

  union
  {
    OType *      ptype;
    OValSym *    pvalsym;
  };

  OIntfDecl(OType * atype)
  :
    kind(IDK_TYPE),
    ptype(atype)
  {
  }

  OIntfDecl(OValSym * avalsym)
  :
    kind(IDK_VALSYM),
    pvalsym(avalsym)
  {
  }
};

class OModuleIntf
{
public:
  string           name;
  OScope *         scope_pub;
  vector<OIntfDecl *>  declarations;

  OModuleIntf(OScope * aparent, const string aname)
  :
    name(aname)
  {
    scope_pub = new OScope(aparent, aname);
  }

  virtual ~OModuleIntf()
  {
    for (OIntfDecl * decl : declarations)
    {
      delete decl;
    }
    delete scope_pub;
  }

  OIntfDecl * AddPublicType(OType * atype);
  OIntfDecl * AddPublicValSym(OValSym * avalsym);

  bool WriteInterface(const string & filename, const string & source_filename);
  bool ReadInterface(const string & filename);
};

bool DumpModuleInterface(const string & filename);
