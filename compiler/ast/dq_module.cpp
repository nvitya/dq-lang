/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
#include "module_path.h"
#include "otype_func.h"
#include "otype_compound.h"
#include "statements.h"

static constexpr const char * DQ_MODULE_INIT_GUARD_NAME = "__dq_module_init_done";
static constexpr const char * DQ_APP_INIT_FUNC_NAME = "__dq_app_module_init";
static constexpr const char * DQ_APP_INIT_LINKAGE_NAME = "dq_module_init";

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
    if (AddPublicType(atype))
    {
      scope_local->typesyms[atype->name] = atype;
    }
  }
  else
  {
    if (scope_priv->DefineType(atype) == atype)
    {
      if (!atype->module)
      {
        atype->module = this;
      }
      scope_local->typesyms[atype->name] = atype;
    }
  }

  return result;
}

ODecl * OModule::DeclareValSym(bool apublic, OValSym * avalsym)
{
  bool effective_public = apublic;
  if (auto * fn = dynamic_cast<OValSymFunc *>(avalsym))
  {
    effective_public = effective_public || fn->IsSpecial();
  }

  ODecl * result = new ODecl(effective_public, avalsym);
  declarations.push_back(result);

  if (effective_public)
  {
    if (AddPublicValSym(avalsym))
    {
      scope_local->valsyms[avalsym->name] = avalsym;
    }
  }
  else
  {
    if (scope_priv->DefineValSym(avalsym) == avalsym)
    {
      if (!avalsym->module)
      {
        avalsym->module = this;
      }
      scope_local->valsyms[avalsym->name] = avalsym;
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

void OModule::DiscardTypeDeclaration(OType * atype)
{
  if (!atype)
  {
    return;
  }

  auto erase_from_scope = [atype](OScope * scope)
  {
    if (!scope)
    {
      return;
    }
    auto it = scope->typesyms.find(atype->name);
    if (scope->typesyms.end() != it && it->second == atype)
    {
      scope->typesyms.erase(it);
    }
  };

  for (auto it = declarations.begin(); declarations.end() != it; )
  {
    ODecl * decl = *it;
    if (decl && DK_TYPE == decl->kind && decl->ptype == atype)
    {
      delete decl;
      it = declarations.erase(it);
    }
    else
    {
      ++it;
    }
  }

  for (auto it = OModuleBase::declarations.begin(); OModuleBase::declarations.end() != it; )
  {
    OIntfDecl * decl = *it;
    if (decl && IDK_TYPE == decl->kind && decl->ptype == atype)
    {
      delete decl;
      it = OModuleBase::declarations.erase(it);
    }
    else
    {
      ++it;
    }
  }

  erase_from_scope(scope_priv);
  erase_from_scope(scope_pub);
  erase_from_scope(scope_local);
  if (atype->module == this)
  {
    atype->module = nullptr;
  }
}

void OModule::RegisterSpecialFunction(OValSymFunc * afunc)
{
  if (!afunc)
  {
    return;
  }

  if (SFK_MAIN == afunc->special_kind)
  {
    app_main_func = afunc;
    return;
  }

  if (SFK_MODULE_INIT != afunc->special_kind)
  {
    return;
  }

  module_init_func = afunc;
  module_init_linkage_name = afunc->GetLinkageName(true, 'F');

  if (!module_init_guard)
  {
    module_init_guard = new OValSym(afunc->scpos, DQ_MODULE_INIT_GUARD_NAME, g_builtins->type_bool, VSK_VARIABLE);
    module_init_guard->scpos.Assign(afunc->scpos);
    DeclareHiddenValSym(false, module_init_guard);
  }
}

OValSymFunc * OModule::FindSpecialFunction(ESpecialFuncKind akind) const
{
  if (SFK_MAIN == akind)
  {
    return app_main_func;
  }
  if (SFK_MODULE_INIT == akind)
  {
    return module_init_func;
  }
  return nullptr;
}

vector<OValSymFunc *> OModule::ModuleInitCallList(bool include_self) const
{
  vector<OValSymFunc *> result;
  vector<string> seen_linkage_names;

  auto add_func = [&](OValSymFunc * fn)
  {
    if (!fn)
    {
      return;
    }
    string linkage_name = fn->attr_has_linkage_name ? fn->attr_linkage_name : fn->GetLinkageName(true, 'F');
    if (seen_linkage_names.end() != find(seen_linkage_names.begin(), seen_linkage_names.end(), linkage_name))
    {
      return;
    }
    seen_linkage_names.push_back(linkage_name);
    result.push_back(fn);
  };

  for (OModuleUse * use : used_modules)
  {
    OModuleIntf * intf = dynamic_cast<OModuleIntf *>(use ? use->module : nullptr);
    if (intf)
    {
      add_func(intf->module_init_func);
    }
  }

  if (include_self)
  {
    add_func(module_init_func);
  }

  return result;
}

void OModule::FinalizeModuleInitFunc()
{
  if (!module_init_func || module_init_prefix_added)
  {
    return;
  }

  vector<OValSymFunc *> init_calls = ModuleInitCallList(false);
  module_init_func->body->stlist.insert(module_init_func->body->stlist.begin(),
      new OStmtModuleInitCalls(module_init_func->scpos, module_init_guard, init_calls));

  vector<OStmt *> global_object_init_stmts;
  for (ODecl * decl : declarations)
  {
    if (!decl || (DK_VALSYM != decl->kind))
    {
      continue;
    }
    OValSym * vs = decl->pvalsym;
    auto * objsym = dynamic_cast<OVsObject *>(vs);
    if (objsym && objsym->IsFixedObjectStorage())
    {
      global_object_init_stmts.push_back(new OStmtConstructFixedObject(vs->scpos, vs));
    }
    else if (vs && vs->ptype && (TK_DYN_ARRAY == vs->ptype->ResolveAlias()->kind))
    {
      global_object_init_stmts.push_back(new OStmtConstructDynArray(vs->scpos, vs));
    }
  }
  module_init_func->body->stlist.insert(module_init_func->body->stlist.begin() + 1,
      global_object_init_stmts.begin(), global_object_init_stmts.end());

  module_init_prefix_added = true;
}

OValSymFunc * OModule::EnsureModuleInitFunc(OScPosition & scpos)
{
  if (module_init_func)
  {
    return module_init_func;
  }

  OTypeFunc * sigtype = new OTypeFunc("ModuleInit");
  module_init_func = new OValSymFunc(scpos, "ModuleInit", sigtype, scope_priv);
  module_init_func->scpos.Assign(scpos);
  module_init_func->scpos_endfunc.Assign(scpos);
  module_init_func->has_body = true;
  module_init_func->special_kind = SFK_MODULE_INIT;
  module_init_linkage_name = module_init_func->GetLinkageName(true, 'F');
  DeclareHiddenValSym(true, module_init_func);

  if (!module_init_guard)
  {
    module_init_guard = new OValSym(scpos, DQ_MODULE_INIT_GUARD_NAME, g_builtins->type_bool, VSK_VARIABLE);
    module_init_guard->scpos.Assign(scpos);
    DeclareHiddenValSym(false, module_init_guard);
  }

  return module_init_func;
}

OValSymFunc * OModule::EnsureAppInitFunc(OScPosition & scpos)
{
  if (app_init_func)
  {
    return app_init_func;
  }

  OTypeFunc * sigtype = new OTypeFunc(DQ_APP_INIT_FUNC_NAME);
  app_init_func = new OValSymFunc(scpos, DQ_APP_INIT_FUNC_NAME, sigtype, scope_priv);
  app_init_func->scpos.Assign(scpos);
  app_init_func->scpos_endfunc.Assign(scpos);
  app_init_func->has_body = true;
  app_init_func->attr_has_linkage_name = true;
  app_init_func->attr_linkage_name = DQ_APP_INIT_LINKAGE_NAME;
  app_init_func->body->AddStatement(new OStmtModuleInitCalls(scpos, nullptr, ModuleInitCallList(true)));
  DeclareHiddenValSym(true, app_init_func);
  return app_init_func;
}

bool OModule::UseCompiledModule(const string & module_path, const string & namespace_name,
                                const string & interface_artifact_path, const string & link_artifact_path,
                                OScope * amerge_scope, bool ais_private,
                                EModuleUseMergeMode amerge_mode, const vector<string> & asymbol_names,
                                bool areexport)
{
  last_interface_load_error.clear();
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
      last_interface_load_error = intf->last_interface_error;
      delete intf;
      return false;
    }
    loaded_modules.push_back(intf);
  }

  OModuleUse * use = new OModuleUse(intf, namespace_name, ais_private, amerge_mode, asymbol_names, areexport);
  if (!use->ValidateSymbolNames())
  {
    delete use;
    return false;
  }

  used_modules.push_back(use);

  if ((MUM_ALL == amerge_mode || MUM_ONLY == amerge_mode || MUM_EXCLUDE == amerge_mode) && amerge_scope)
  {
    use->scope_use = new OScope(amerge_scope->parent_scope, module_path + ".use");
    use->FillScope();
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
  for (const string & link_dep : intf->link_dependencies)
  {
    filesystem::path dep_artifact;
    if (OModulePath::ResolveCanonicalArtifact(link_dep, module_path, link_artifact_path, dep_artifact))
    {
      string dep_artifact_name = dep_artifact.string();
      if (link_module_artifacts.end() == find(link_module_artifacts.begin(), link_module_artifacts.end(), dep_artifact_name))
      {
        link_module_artifacts.push_back(dep_artifact_name);
      }
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
