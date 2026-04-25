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

#include "dq_module.h"

OModule *  g_module = nullptr;

void init_dq_module()
{
  g_module = new OModule();
}

ODecl * OModule::DeclareType(bool apublic, OType * atype)
{
  ODecl * result = new ODecl(apublic, atype);
  declarations.push_back(result);

  OScope * pscope = (apublic ? scope_pub : scope_priv);
  pscope->DefineType(atype);

  return result;
}

ODecl * OModule::DeclareValSym(bool apublic, OValSym * avalsym)
{
  ODecl * result = new ODecl(apublic, avalsym);
  declarations.push_back(result);

  OScope * pscope = (apublic ? scope_pub : scope_priv);
  pscope->DefineValSym(avalsym);

  return result;
}

ODecl * OModule::DeclareHiddenValSym(bool apublic, OValSym * avalsym)
{
  ODecl * result = new ODecl(apublic, avalsym);
  declarations.push_back(result);
  return result;
}

bool OModule::TypeDeclared(const string aname, OType ** rtype)
{
  OType * found;
  found = scope_priv->FindType(aname, nullptr, false);
  if (found)
  {
    *rtype = found;
    return true;
  }
  found = scope_pub->FindType(aname, nullptr, false);
  if (found)
  {
    *rtype = found;
    return true;
  }
  return false;
}

bool OModule::ValSymDeclared(const string aname, OValSym ** rvalsym)
{
  OValSym * found;
  found = scope_priv->FindValSym(aname, nullptr, false);
  if (found)
  {
    *rvalsym = found;
    return true;
  }
  found = scope_pub->FindValSym(aname, nullptr, false);
  if (found)
  {
    *rvalsym = found;
    return true;
  }
  return false;
}
