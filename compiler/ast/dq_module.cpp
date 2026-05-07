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

#include <algorithm>

#include "dq_module.h"
#include "named_scopes.h"

OModule *  g_module = nullptr;

void init_dq_module()
{
  g_module = new OModule();
}

ODecl * OModule::DeclareType(bool apublic, OType * atype)
{
  ODecl * result = new ODecl(apublic, atype);
  declarations.push_back(result);

  if (apublic)
  {
    AddPublicType(atype);
  }
  else
  {
    scope_priv->DefineType(atype);
  }

  return result;
}

ODecl * OModule::DeclareValSym(bool apublic, OValSym * avalsym)
{
  ODecl * result = new ODecl(apublic, avalsym);
  declarations.push_back(result);

  if (apublic)
  {
    AddPublicValSym(avalsym);
  }
  else
  {
    scope_priv->DefineValSym(avalsym);
  }

  return result;
}

ODecl * OModule::DeclareHiddenValSym(bool apublic, OValSym * avalsym)
{
  ODecl * result = new ODecl(apublic, avalsym);
  declarations.push_back(result);
  return result;
}

bool OModule::UseCompiledModule(const string & module_path, const string & namespace_name, const string & artifact_path)
{
  OModuleIntf * intf = new OModuleIntf(scope_pub->parent_scope, module_path);
  if (!intf->ReadInterface(artifact_path))
  {
    delete intf;
    return false;
  }

  intf->scope_pub->parent_scope = scope_pub->parent_scope;
  scope_pub->parent_scope = intf->scope_pub;
  g_namespaces[namespace_name] = intf->scope_pub;

  used_modules.push_back(intf);
  if (link_module_artifacts.end() == find(link_module_artifacts.begin(), link_module_artifacts.end(), artifact_path))
  {
    link_module_artifacts.push_back(artifact_path);
  }

  return true;
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
