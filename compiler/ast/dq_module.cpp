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
#include "dqc.h"
#include "errorcodes.h"

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
    if (scope_priv->DefineType(atype) == atype && !atype->module)
    {
      atype->module = this;
    }
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
    if (scope_priv->DefineValSym(avalsym) == avalsym && !avalsym->module)
    {
      avalsym->module = this;
    }
  }

  return result;
}

ODecl * OModule::DeclareHiddenValSym(bool apublic, OValSym * avalsym)
{
  ODecl * result = new ODecl(apublic, avalsym);
  declarations.push_back(result);
  return result;
}

static void FillUseScope(OModuleUse * ause)
{
  if (!ause || !ause->module || !ause->scope_use)
  {
    return;
  }
  ause->CopySelectedSymbolsTo(ause->scope_use);
}

bool OModule::UseCompiledModule(const string & module_path, const string & namespace_name,
                                const string & interface_artifact_path, const string & link_artifact_path,
                                OScope * amerge_scope, bool ais_private,
                                EModuleUseMergeMode amerge_mode, const vector<string> & asymbol_names,
                                bool areexport)
{
  OModuleIntf * intf = nullptr;
  for (OModuleIntf * loaded_intf : loaded_modules)
  {
    if (loaded_intf && (loaded_intf->name == module_path))
    {
      intf = loaded_intf;
      break;
    }
  }

  if (!intf)
  {
    intf = new OModuleIntf(scope_pub->parent_scope, module_path);
    if (!intf->ReadInterface(interface_artifact_path, true, true)
        && ((interface_artifact_path == link_artifact_path) || !intf->ReadInterface(link_artifact_path)))
    {
      delete intf;
      return false;
    }
    loaded_modules.push_back(intf);
  }

  OModuleUse * use = new OModuleUse(intf, ais_private, amerge_mode, asymbol_names, areexport);
  if (!use->ValidateSymbolNames())
  {
    delete use;
    return false;
  }

  used_modules.push_back(use);

  if ((MUM_ALL == amerge_mode || MUM_ONLY == amerge_mode || MUM_EXCLUDE == amerge_mode) && amerge_scope)
  {
    use->scope_use = new OScope(amerge_scope->parent_scope, module_path + ".use");
    FillUseScope(use);
    amerge_scope->parent_scope = use->scope_use;
  }
  g_namespaces[namespace_name] = intf->scope_pub;

  if (link_module_artifacts.end() == find(link_module_artifacts.begin(), link_module_artifacts.end(), link_artifact_path))
  {
    link_module_artifacts.push_back(link_artifact_path);
  }
  for (const string & reexport_artifact : intf->reexport_artifacts)
  {
    if (link_module_artifacts.end() == find(link_module_artifacts.begin(), link_module_artifacts.end(), reexport_artifact))
    {
      link_module_artifacts.push_back(reexport_artifact);
    }
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
