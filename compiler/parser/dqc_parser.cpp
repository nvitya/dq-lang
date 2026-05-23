/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_parser.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include <print>
#include <format>
#include <filesystem>
#include <ranges>
#include <utility>

#include "dq_module.h"
#include "dqc_parser.h"
#include "otype_func.h"
#include "otype_array.h"
#include "otype_cstring.h"
#include "otype_int.h"
#include "otype_object.h"
#include "named_scopes.h"
#include "scope_defines.h"
#include "expressions.h"
#include "statements.h"
#include "module_path.h"

using namespace std;

bool ODqCompParser::SupportsFuncParamDefaultType(OType * ptype)
{
  OType * resolved = (ptype ? ptype->ResolveAlias() : nullptr);
  if (!resolved)
  {
    return false;
  }

  if ((TK_INT == resolved->kind) || (TK_FLOAT == resolved->kind)
      || (TK_BOOL == resolved->kind) || (TK_ARRAY == resolved->kind)
      || (TK_POINTER == resolved->kind))
  {
    return true;
  }

  if (TK_STRING == resolved->kind)
  {
    OTypeCString * cstrtype = dynamic_cast<OTypeCString *>(resolved);
    return cstrtype && (cstrtype->maxlen > 0);
  }

  return false;
}

bool ODqCompParser::ParseParamModeKeyword(const string & sid, EParamMode & rmode)
{
  if ("ref" == sid)
  {
    rmode = FPM_REF;
    return true;
  }

  if ("refin" == sid)
  {
    rmode = FPM_REFIN;
    return true;
  }

  if ("refout" == sid)
  {
    rmode = FPM_REFOUT;
    return true;
  }

  if ("refnull" == sid)
  {
    rmode = FPM_REFNULL;
    return true;
  }

  return false;
}

filesystem::path ODqCompParser::CurrentSourcePath() const
{
  if (scf && scf->curfile && !scf->curfile->fullpath.empty())
  {
    return scf->curfile->fullpath;
  }
  return "";
}

ODqCompParser::ODqCompParser()
{
  attr = new OAttr();
}

ODqCompParser::~ODqCompParser()
{
  delete attr;
}

void ODqCompParser::RecoverFailedFunctionDecl()
{
  scf->SkipWhite();
  if (scf->CheckSymbol(";"))
  {
    return;
  }

  if (scf->CheckSymbol(":", false))
  {
    scf->CheckSymbol(":");
    scf->SearchPattern("endfunc", true);
    return;
  }

  if (scf->CheckSymbol("{", false))
  {
    scf->CheckSymbol("{");
    scf->SearchPattern("}", true);
    return;
  }

  SkipToModuleStatementStart();
}

bool ODqCompParser::ParseAttrIntArg(const string & attrname, int64_t & rvalue, bool positive_only)
{
  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, attrname);
    return false;
  }

  scf->SkipWhite();
  if (!scf->ReadInt64Value(rvalue))
  {
    Error(DQERR_ATTR_ARG_INT, attrname);
    return false;
  }

  if (positive_only && (rvalue <= 0))
  {
    Error(DQERR_ATTR_ARG_POSITIVE_INT, attrname);
    return false;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_AFTER, attrname);
    return false;
  }

  return true;
}

bool ODqCompParser::ParseAttrStringArg(const string & attrname, string & rvalue)
{
  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, attrname);
    return false;
  }

  scf->SkipWhite();
  if (!scf->ReadQuotedString(rvalue))
  {
    Error(DQERR_ATTR_ARG_STRING, attrname);
    return false;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_AFTER, attrname);
    return false;
  }

  return true;
}

bool ODqCompParser::ParseSingleAttribute(const string & attrname)
{
  scf->SkipWhite();

  if ("external" == attrname)
  {
    attr->SetFlag(ATTF_EXTERNAL);
    attr->external_linkage_name = "";
    if (scf->CheckSymbol("(", false))
    {
      if (!ParseAttrStringArg(attrname, attr->external_linkage_name))
      {
        return false;
      }
    }
    return true;
  }

  if ("export" == attrname)
  {
    attr->SetFlag(ATTF_EXPORT);
    return ParseAttrStringArg(attrname, attr->export_linkage_name);
  }

  if ("cexport" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_CEXPORT);
    attr->export_linkage_name = "";
    return true;
  }

  if ("align" == attrname)
  {
    attr->SetFlag(ATTF_ALIGN);
    return ParseAttrIntArg(attrname, attr->align_value, true);
  }

  if ("packed" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_PACKED);
    return true;
  }

  if ("section" == attrname)
  {
    attr->SetFlag(ATTF_SECTION);
    return ParseAttrStringArg(attrname, attr->section_name);
  }

  if ("overload" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_OVERLOAD);
    return true;
  }

  if ("override" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_OVERRIDE);
    return true;
  }

  if ("virtual" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_VIRTUAL);
    return true;
  }

  if ("abstract" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_ABSTRACT);
    return true;
  }

  if ("final" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_FINAL);
    return true;
  }

  if ("volatile" == attrname)
  {
    if (scf->CheckSymbol("(", false))
    {
      Error(DQERR_ATTR_PAREN_NOT_ALLOWED, attrname);
      return false;
    }
    attr->SetFlag(ATTF_VOLATILE);
    return true;
  }

  Error(DQERR_ATTR_UNKNOWN, attrname);
  return false;
}

bool ODqCompParser::ParseAttributeBlock()
{
  OScPosition attrpos;

  scf->SaveCurPos(attrpos);
  if (!scf->CheckSymbol("[["))
  {
    return true;
  }

  attr->scpos = attrpos;

  while (!scf->Eof())
  {
    string attrname;

    scf->SkipWhite();
    if (!scf->ReadIdentifier(attrname))
    {
      Error(DQERR_ATTR_NAME_EXPECTED);
      SkipToSymbol("]]");
      return false;
    }

    if (!ParseSingleAttribute(attrname))
    {
      SkipToSymbol("]]");
      return false;
    }

    scf->SkipWhite();
    if (scf->CheckSymbol("]]"))
    {
      return true;
    }

    if (!scf->CheckSymbol(","))
    {
      Error(DQERR_ATTR_SEPARATOR);
      SkipToSymbol("]]");
      return false;
    }
  }

  Error(DQERR_MISSING_ATTR_CLOSE_AFTER, "attribute list");
  return false;
}

bool ODqCompParser::ParseAttributes(bool areset)
{
  if (areset && attr)
  {
    attr->Reset();
  }

  while (!scf->Eof())
  {
    scf->SkipWhite();
    if (!scf->CheckSymbol("[[", false))
    {
      break;
    }

    if (!ParseAttributeBlock())
    {
      return false;
    }
  }

  return true;
}

void ODqCompParser::ParseModule()
{
  string sid;

  section_public = true;
  cur_mod_scope = g_module->scope_pub;
  curscope = cur_mod_scope;

  while (not scf->Eof())
  {
    scf->SkipWhite(); // jumps to the first normal token
    if (scf->Eof())
    {
      break; // end of module
    }

    OModulePath current_module;
    string module_error;
    if (current_module.InitCurrent(CurrentSourcePath(), module_error))
    {
      g_module->name = current_module.module_id;
      if (!g_opt.module_use_stack.empty())
      {
        g_opt.module_use_stack.back() = current_module.module_id;
      }
    }

    scf->SaveCurPos(scpos_statement_start);  // to display the statement start

    if (!ParseAttributes(true))
    {
      SkipToModuleStatementStart();
      continue;
    }

    // module root starters
    if (not scf->ReadIdentifier(sid))
    {
      RootStatementError(DQERR_MODULE_STATEMENT_EXPECTED, &scpos_statement_start);
      continue;
    }

    // The module root statement must start with a keyword like
    //   use, module, var, type, function, implementation

    if ("implementation" == sid)
    {
      if (g_opt.ifgen)
      {
        break;  // do not parse the implementation part in --ifgen mode
      }

      section_public = false;
      cur_mod_scope = g_module->scope_priv;
      curscope = cur_mod_scope;
    }
    else if ("use" == sid)
    {
      ParseUseStatement();
    }
    else if ("var" == sid) // global variable definition
    {
      ParseStmtVar(true);
    }
    else if ("const" == sid) // global constant definition
    {
      ParseStmtConst(true);
    }
    else if ("type" == sid)
    {
      ParseRootTypeDecl();
    }
    else if ("function" == sid)
    {
      ParseFunction();
      curscope = cur_mod_scope;
      curblock = nullptr;
    }
    else if ("struct" == sid)
    {
      ParseStructDecl();
    }
    else if ("object" == sid)
    {
      ParseObjectDecl();
    }
    else  // unknown
    {
      RootStatementError(DQERR_MODULE_STATEMENT_UNKNOWN, sid, &scpos_statement_start);
    }
  }

  if (!g_opt.ifgen)
  {
    g_module->FinalizeModuleInitFunc();
    ValidateModuleForwardFuncDecls(g_module);
  }

  if (g_opt.verblevel >= VERBLEVEL_DEBUG)
  {
    printf("ParseModule finished.");
  }
}

bool ODqCompParser::CheckSpecialReservedRootName(const string & aname)
{
  if (SFK_NONE == SpecialFuncKindFromName(aname))
  {
    return true;
  }

  OScPosition namepos(scf->curfile, scf->prevp);
  RootStatementError(DQERR_SPECIAL_FUNC_RESERVED, aname, &namepos);
  return false;
}

bool ODqCompParser::ParseUseModulePath(OModulePath & rpath)
{
  string path_text;
  string sid;

  scf->SkipWhite();
  if (scf->CheckSymbol("^/"))
  {
    path_text = "^/";
  }
  else
  {
    while (scf->CheckSymbol("../"))
    {
      path_text += "../";
    }
    if (path_text.empty() && scf->CheckSymbol("./"))
    {
      path_text = "./";
    }
  }

  if (!scf->ReadIdentifier(sid))
  {
    RootStatementError(DQERR_ID_EXP_AFTER, "use");
    return false;
  }
  path_text += sid;

  while (true)
  {
    scf->SkipWhite();
    if (!scf->CheckSymbol("/"))
    {
      break;
    }

    if (!scf->ReadIdentifier(sid))
    {
      RootStatementError(DQERR_ID_EXP_AFTER, "/");
      return false;
    }

    path_text += "/" + sid;
  }

  string path_error;
  if (!rpath.ParseUsePath(path_text, path_error))
  {
    OScPosition errpos;
    errpos.Assign(scpos_statement_start);
    errpos.RecalcLineCol();
    Error(DQERR_MODULE_PATH_INVALID, path_text, &errpos);
    return false;
  }
  return true;
}

void ODqCompParser::ParseUseStatement()
{
  while (true)  // for multiple use_blocks
  {
    OModulePath use_path;
    if (!ParseUseModulePath(use_path))
    {
      return;
    }

    string namespace_name;
    string sid;
    EModuleUseMergeMode merge_mode = MUM_ALL;
    vector<string> symbol_names;
    bool reexport = false;
    namespace_name = use_path.namespace_name;

    while (true) // cycle to get multiple use modifiers
    {
      scf->SkipWhite();
      if (scf->CheckSymbol(";", false) or scf->CheckSymbol(",", false))  // detect the end of the use block
      {
        break;
      }

      if (scf->CheckSymbol("--"))
      {
        if ((MUM_ALL != merge_mode) || reexport)
        {
          OScPosition errpos;
          errpos.Assign(scpos_statement_start);
          CheckStatementClose();
          errpos.RecalcLineCol();
          Error(DQERR_USE_MODIFIER_CONFLICT, "-- with only/exclude/reexport", &errpos);
          return;
        }
        merge_mode = MUM_NONE;
        continue;
      }

      if (!scf->ReadIdentifier(sid))
      {
        if (!CheckStatementClose())
        {
          return;
        }
        break;
      }

      if ("as" == sid)
      {
        scf->SkipWhite();
        if (!scf->ReadIdentifier(namespace_name))
        {
          RootStatementError(DQERR_ID_EXP_AFTER, "as");
          return;
        }
      }
      else if ("only" == sid)
      {
        if (MUM_ALL != merge_mode)
        {
          vector<string> dummy_names;
          ParseUseSymbolList("only", dummy_names);
          CheckStatementClose();
          Error(DQERR_USE_MODIFIER_CONFLICT, "only with --/exclude", &scpos_statement_start);
          return;
        }
        merge_mode = MUM_ONLY;
        if (!ParseUseSymbolList("only", symbol_names))
        {
          return;
        }
      }
      else if ("exclude" == sid)
      {
        if (MUM_ALL != merge_mode)
        {
          vector<string> dummy_names;
          ParseUseSymbolList("exclude", dummy_names);
          CheckStatementClose();
          Error(DQERR_USE_MODIFIER_CONFLICT, "exclude with --/only", &scpos_statement_start);
          return;
        }
        merge_mode = MUM_EXCLUDE;
        if (!ParseUseSymbolList("exclude", symbol_names))
        {
          return;
        }
      }
      else if ("reexport" == sid)
      {
        if (!section_public)
        {
          OScPosition errpos;
          errpos.Assign(scpos_statement_start);
          CheckStatementClose();
          errpos.RecalcLineCol();
          Error(DQERR_USE_MODIFIER_CONFLICT, "reexport in implementation section", &errpos);
          return;
        }
        if (MUM_NONE == merge_mode)
        {
          OScPosition errpos;
          errpos.Assign(scpos_statement_start);
          CheckStatementClose();
          errpos.RecalcLineCol();
          Error(DQERR_USE_MODIFIER_CONFLICT, "reexport with --", &errpos);
          return;
        }
        reexport = true;
      }
      else
      {
        StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "previous statement");
        return;
      }
    }

    bool more_use_blocks = false;
    if (scf->CheckSymbol(",", true))
    {
      more_use_blocks = true;
    }
    else if (!CheckStatementClose())
    {
      return;
    }

    if (g_namespaces.end() != g_namespaces.find(namespace_name))
    {
      Error(DQERR_USE_NAMESPACE_CONFLICT, namespace_name, &scpos_statement_start);
      return;
    }

    OModulePath current_module;
    string path_error;
    if (!current_module.InitCurrent(CurrentSourcePath(), path_error))
    {
      OScPosition errpos;
      errpos.Assign(scpos_statement_start);
      errpos.RecalcLineCol();
      Error(DQERR_MODULE_ROOT_INVALID, path_error, &errpos);
      return;
    }

    if (!use_path.ResolveFrom(current_module, path_error))
    {
      OScPosition errpos;
      errpos.Assign(scpos_statement_start);
      errpos.RecalcLineCol();
      if (use_path.IsLocalReference())
      {
        Error(DQERR_MODULE_PATH_ABOVE_ROOT, use_path.source_text, current_module.root_dir.string(), &errpos);
      }
      else
      {
        Error(DQERR_PACKAGE_NOT_FOUND, path_error, &errpos);
      }
      return;
    }

    // process the use block

    const string module_path = use_path.module_id;
    filesystem::path source_path = use_path.source_path;
    filesystem::path artifact_path = use_path.artifact_path;

    OModuleIntf artifact_intf(g_builtins, module_path);
    bool in_module_stack = artifact_intf.IsInModuleUseStack(module_path);
    if (in_module_stack && section_public)
    {
      Error(DQERR_USE_CYCLE, artifact_intf.FormatModuleCycle(module_path), &scpos_statement_start);
      return;
    }

    auto report_artifact_error = [&](const SModuleArtifactEnsureResult & result) -> void
    {
      if (EModuleArtifactEnsureError::SOURCE_MISSING == result.error)
      {
        OScPosition errpos;
        errpos.Assign(scpos_statement_start);
        errpos.RecalcLineCol();
        Error(DQERR_MODULE_NOT_FOUND, module_path, source_path.string(), &errpos);
      }
      else if (EModuleArtifactEnsureError::ARTIFACT_MISSING == result.error)
      {
        Error(DQERR_USE_ARTIFACT_MISSING, module_path, artifact_path.string(), &scpos_statement_start);
      }
      else
      {
        Error(DQERR_USE_REGEN_FAILED, module_path, source_path.string(), result.reason, &scpos_statement_start);
      }
    };

    SModuleArtifactEnsureResult artifact_result =
      artifact_intf.EnsureFreshInterfaceArtifact(use_path, in_module_stack);
    if (!artifact_result.Ok())
    {
      report_artifact_error(artifact_result);
      return;
    }

    filesystem::path interface_load_path = artifact_result.interface_load_path;

    if (!g_opt.ifgen && !in_module_stack)
    {
      artifact_result = artifact_intf.EnsureFreshCompiledArtifact(use_path);
      if (!artifact_result.Ok())
      {
        report_artifact_error(artifact_result);
        return;
      }
      interface_load_path = artifact_result.interface_load_path;
    }

    int prev_errorcnt = errorcnt;
    if (!g_module->UseCompiledModule(module_path, namespace_name, interface_load_path.string(),
                                    artifact_path.string(), cur_mod_scope, !section_public, merge_mode,
                                    symbol_names, reexport))
    {
      if (prev_errorcnt == errorcnt)
      {
        Error(DQERR_USE_INTERFACE_LOAD, interface_load_path.string(), &scpos_statement_start);
      }
      return;
    }

    // handling multiple use blocks:

    if (more_use_blocks)
    {
      continue;
    }

    break;
  }  // while repeated use blocks

}

bool ODqCompParser::ParseUseSymbolList(const string & amodifier, vector<string> & rsymbol_names)
{
  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, amodifier);
    return false;
  }

  while (true)
  {
    string name;
    scf->SkipWhite();
    if (!scf->ReadIdentifier(name))
    {
      Error(DQERR_ID_EXP_AFTER, amodifier);
      return false;
    }

    scf->SkipWhite();
    string sid;
    if (scf->ReadIdentifier(sid, false) && ("as" == sid))
    {
      Error(DQERR_NOT_IMPLEMENTED_YET, "symbol aliases in use only/exclude", &scf->prevpos);
      return false;
    }

    rsymbol_names.push_back(name);

    scf->SkipWhite();
    if (scf->CheckSymbol(")"))
    {
      return true;
    }
    if (!scf->CheckSymbol(","))
    {
      Error(DQERR_MISSING_COMMA_IN, amodifier);
      return false;
    }
  }
}

void ODqCompParser::ParseStmtVar(bool arootstmt)
{
  // syntax form: "var identifier : type [ = expression];"
  // note: "var" is already consumed

  string     sid;
  string     stype;
  OValSym *  pvalsym;
  OType *    ptype = nullptr;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    StatementError(DQERR_ID_EXP_AFTER, "var");
    return;
  }
  if (arootstmt && !CheckSpecialReservedRootName(sid))
  {
    return;
  }

  pvalsym = curscope->FindValSym(sid, nullptr, false);  // do not search in the parent scopes this time !
  if (pvalsym)
  {
    StatementError(DQERR_VS_ALREADY_DECL_TYPE, sid, pvalsym->ptype->name, &scf->prevpos);
    return;
  }

  OExpr * initexpr = nullptr;
  bool zero_init = false;
  bool fixed_object = false;
  bool fixed_ctor_call_at_decl = false;
  vector<OExpr *> fixed_ctor_args;
  scf->SkipWhite();
  if (scf->CheckSymbol("<-"))
  {
    fixed_object = true;
    ptype = ParseTypeSpec();
    if (not ptype)
    {
      SkipToModuleStatementStart();
      return;
    }

    OTypeObject * object_type = dynamic_cast<OTypeObject *>(ptype->ResolveAlias());
    if (!object_type)
    {
      StatementError(DQERR_TYPE_EXPECTED, "object", ptype->name);
      return;
    }

    scf->SkipWhite();
    if (scf->CheckSymbol("("))
    {
      fixed_ctor_call_at_decl = true;
      vector<TRawCallArg> rawargs;
      if (!ParseRawCallArguments(sid, rawargs))
      {
        return;
      }
      for (TRawCallArg & rawarg : rawargs)
      {
        fixed_ctor_args.push_back(rawarg.expr);
        rawarg.expr = nullptr;
      }
      FreeRawCallArguments(rawargs);
    }
  }
  else if (scf->CheckSymbol(":"))
  {
    ptype = ParseTypeSpec();
    if (not ptype)
    {
      SkipToModuleStatementStart();
      return;
    }
  }
  else if (!arootstmt && scf->CheckSymbol("="))
  {
    scf->SkipWhite();
    initexpr = ParseExpression();
    if (!initexpr)
    {
      return;
    }

    auto * newexpr = dynamic_cast<ONewExpr *>(initexpr);
    if (!newexpr)
    {
      delete initexpr;
      StatementError(DQERR_TYPE_SPECIFIER_EXP_AFTER, sid);
      return;
    }
    OTypeObject * new_object_type = dynamic_cast<OTypeObject *>(newexpr->alloc_type ? newexpr->alloc_type->ResolveAlias() : nullptr);
    ptype = (new_object_type ? newexpr->alloc_type : newexpr->ptype);
  }
  else
  {
    StatementError(DQERR_TYPE_SPECIFIER_EXP_AFTER, sid);
    return;
  }

  scf->SkipWhite();
  if (scf->CheckSymbol("="))  // variable initializer specified
  {
    scf->SkipWhite();
    // Check for {} zero-initializer (for compound types)
    if (scf->CheckSymbol("{"))
    {
      scf->SkipWhite();
      if (not scf->CheckSymbol("}"))
      {
        ErrorTxt(DQERR_EXPR_INITVALUE, "\"}\" expected for zero-initializer");
        return;
      }
      zero_init = true;
    }
    else
    {
      initexpr = ParseExpression();
    }
  }

  if (arootstmt)
  {
    if (!ParseAttributes(false))
    {
      delete initexpr;
      SkipToModuleStatementStart();
      return;
    }
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "variable declaration");
  }

  OTypeObject * decl_object_type = dynamic_cast<OTypeObject *>(ptype ? ptype->ResolveAlias() : nullptr);
  if (fixed_object && decl_object_type && decl_object_type->is_abstract)
  {
    StatementError(DQERR_NOT_SUPPORTED, format("constructing abstract object \"{}\"", decl_object_type->name));
    return;
  }
  if (zero_init && decl_object_type && !fixed_object)
  {
    StatementError(DQERR_NOT_SUPPORTED, "value-style object zero-initialization");
    return;
  }
  if (fixed_object && fixed_ctor_call_at_decl)
  {
    OValSymFunc * ctor = nullptr;
    if (!CheckObjectCtorArgs(decl_object_type, fixed_ctor_args, ctor))
    {
      return;
    }
  }

  if (initexpr and (not CheckAssignType(ptype, &initexpr, "Assignment")))  // might add implicit conversion
  {
    // error message is already provided.
    delete initexpr;
    return;
  }

  if (arootstmt)
  {
    ODecl * vdecl = AddDeclVar(scpos_statement_start, sid, ptype);
    if (fixed_object)
    {
      auto * objsym = dynamic_cast<OVsObject *>(vdecl->pvalsym);
      if (!objsym)
      {
        throw logic_error(format("Fixed object variable \"{}\" was not created as OVsObject", sid));
      }
      objsym->SetObjectStorage(OSK_OBJECT_FIXED);
      objsym->SetObjectCtorArgs(std::move(fixed_ctor_args));
      objsym->SetObjectCtorCallAtDecl(fixed_ctor_call_at_decl);
      g_module->EnsureModuleInitFunc(scpos_statement_start);
    }
    else if (auto * objsym = dynamic_cast<OVsObject *>(vdecl->pvalsym))
    {
      objsym->SetObjectStorage(OSK_OBJECT_REF);
    }
    vdecl->pvalsym->ApplyAttributes(attr, ATGT_GLOBAL_VAR);
    if (initexpr)
    {
      if (not vdecl->initvalue->CalculateConstant(initexpr))
      {
        // the error message is already generated
      }
      delete initexpr;
    }

    // global variables are always initialized with 0 / null
    vdecl->pvalsym->initialized = true;
  }
  else
  {
    pvalsym = ptype->CreateValSym(scpos_statement_start, sid);
    if (fixed_object)
    {
      auto * objsym = dynamic_cast<OVsObject *>(pvalsym);
      if (!objsym)
      {
        throw logic_error(format("Fixed object variable \"{}\" was not created as OVsObject", sid));
      }
      objsym->SetObjectStorage(OSK_OBJECT_FIXED);
      objsym->SetObjectCtorArgs(std::move(fixed_ctor_args));
      objsym->SetObjectCtorCallAtDecl(fixed_ctor_call_at_decl);
      pvalsym->initialized = true;
    }
    else if (auto * objsym = dynamic_cast<OVsObject *>(pvalsym))
    {
      objsym->SetObjectStorage(OSK_OBJECT_REF);
    }
    if (zero_init)  pvalsym->initialized = true;
    curscope->DefineValSym(pvalsym);
    curblock->AddStatement(new OStmtVarDecl(scpos_statement_start, pvalsym, initexpr));
  }
}

void ODqCompParser::ParseStmtRef()
{
  string     sid;
  OValSym *  pvalsym = nullptr;
  OType *    ptype = nullptr;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    StatementError(DQERR_ID_EXP_AFTER, "ref");
    return;
  }

  pvalsym = curscope->FindValSym(sid, nullptr, false);
  if (pvalsym)
  {
    StatementError(DQERR_VS_ALREADY_DECL_TYPE, sid, pvalsym->ptype->name, &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (scf->CheckSymbol(":"))
  {
    ptype = ParseTypeSpec();
    if (!ptype)
    {
      return;
    }
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol("="))
  {
    StatementError(DQERR_REF_LOCAL_INIT_REQUIRED, sid);
    return;
  }

  scf->SkipWhite();
  OExpr * bindexpr = ParseExpression();
  if (!bindexpr)
  {
    return;
  }

  OLValueExpr * bindlval = dynamic_cast<OLValueExpr *>(bindexpr);
  OValSym * rootvalsym = (bindlval ? GetAssignRootValSym(bindlval) : nullptr);
  if (!bindlval || (rootvalsym && (VSK_CONST == rootvalsym->kind || !rootvalsym->IsRefWriteable())))
  {
    delete bindexpr;
    StatementError(DQERR_REF_LOCAL_BIND_TARGET, sid);
    return;
  }

  if (!ptype)
  {
    ptype = bindexpr->ptype;
    if (!ptype)
    {
      delete bindexpr;
      StatementError(DQERR_REF_LOCAL_TYPE_INFER, sid);
      return;
    }
  }
  else if (!OTypeFunc::SameRefBindingType(ptype, bindexpr->ptype))
  {
    string srcname = (bindexpr->ptype ? bindexpr->ptype->name : "?");
    delete bindexpr;
    StatementError(DQERR_REF_LOCAL_TYPE_MISM, sid, ptype->name, srcname);
    return;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "ref declaration");
  }

  pvalsym = ptype->CreateValSym(scpos_statement_start, sid);
  pvalsym->param_mode = FPM_REF;
  pvalsym->is_ref_alias = true;
  pvalsym->initialized = true;
  if (auto * objsym = dynamic_cast<OVsObject *>(pvalsym))
  {
    objsym->SetObjectStorage(OSK_PLAIN);
  }
  curscope->DefineValSym(pvalsym);
  curblock->AddStatement(new OStmtVarDecl(scpos_statement_start, pvalsym, new OAddrOfExpr(bindlval)));
}


void ODqCompParser::ParseStmtConst(bool arootstmt)
{
  // syntax form: "const identifier : type [ = initial value];"
  // note: "const" is already consumed

  string       sid;
  OValSym *    pvalsym;
  OType *      ptype;
  OScPosition  expos;

  auto emit_error = [&](const TDiagDefErr & adiag, string_view par1 = "", string_view par2 = "",
                        OScPosition * scpos = nullptr, bool atryrecover = true)
  {
    if (arootstmt)
    {
      RootStatementError(adiag, par1, par2, scpos, atryrecover);
    }
    else
    {
      StatementError(adiag, par1, par2, scpos, atryrecover);
    }
  };

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    emit_error(DQERR_ID_EXP_AFTER, "const");
    return;
  }
  if (arootstmt && !CheckSpecialReservedRootName(sid))
  {
    return;
  }

  pvalsym = nullptr;
  if (arootstmt)
  {
    g_module->ValSymDeclared(sid, &pvalsym);
  }
  else
  {
    pvalsym = curscope->FindValSym(sid, nullptr, false);  // check only the current scope
  }
  if (pvalsym)
  {
    emit_error(DQERR_VS_ALREADY_DECL_TYPE, sid, pvalsym->ptype->name, &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(":"))
  {
    emit_error(DQERR_TYPE_SPECIFIER_EXP_AFTER, sid);
    return;
  }

  ptype = ParseTypeSpec();
  if (not ptype)
  {
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol("="))  // variable initializer specified
  {
    emit_error(DQERR_MISSING_ASSIGN_FOR, sid);
    return;
  }

  scf->SkipWhite();
  scf->SaveCurPos(expos);
  OExpr * valueexpr = ParseExpression();
  if (not valueexpr)
  {
    delete valueexpr;
    emit_error(DQERR_EXPR_WRONG_VALUE_FOR, sid, "", &expos);
    return;
  }

  OValue * pvalue = ptype->CreateValue();
  if (not pvalue->CalculateConstant(valueexpr))
  {
    emit_error(DQERR_CONSTEXPR_INVALID_FOR, sid, "", &expos);

    delete valueexpr;
    delete pvalue;
    return;
  }

  delete valueexpr;

  if (arootstmt)
  {
    if (!ParseAttributes(false))
    {
      SkipToModuleStatementStart();
      delete pvalue;
      return;
    }
  }

  if (arootstmt)
  {
    ODecl * decl = AddDeclConst(scpos_statement_start, sid, ptype, pvalue);
    decl->pvalsym->ApplyAttributes(attr, ATGT_GLOBAL_CONST);
  }
  else
  {
    curscope->DefineValSym(new OValSymConst(scpos_statement_start, sid, ptype, pvalue));
  }

  if (not CheckStatementClose())
  {
    // error message already generated.
    return;
  }
}

void ODqCompParser::ParseRootTypeDecl()
{
  // syntax form: "type identifier = type;"
  // note: "type" is already consumed

  string sid;
  OType * ptype;
  OType * foundtype = nullptr;

  scf->SkipWhite();
  if (not scf->ReadIdentifier(sid))
  {
    RootStatementError(DQERR_ID_EXP_AFTER, "type");
    return;
  }
  if (!CheckSpecialReservedRootName(sid))
  {
    return;
  }

  if (g_module->TypeDeclared(sid, &foundtype))
  {
    RootStatementError(DQERR_TYPE_ALREADY_DEFINED, sid, &scf->prevpos);
    return;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol("="))
  {
    RootStatementError(DQERR_MISSING_ASSIGN_FOR, sid);
    return;
  }

  ptype = ParseTypeSpec();
  if (not ptype)
  {
    SkipToModuleStatementStart();
    return;
  }

  if (!ParseAttributes(false))
  {
    SkipToModuleStatementStart();
    return;
  }

  if (attr->flags)
  {
    attr->CheckInvalidAttributes(ATGT_NONE);
  }

  g_module->DeclareType(section_public, new OTypeAlias(sid, ptype));

  if (not CheckStatementClose())
  {
    return;
  }
}

void ODqCompParser::ParseStructDecl()
{
  // note: "struct" is already consumed
  // syntax form: "struct Name:\n  field : type;\n  ...\nendstruct"

  string sname;
  scf->SkipWhite();
  if (not scf->ReadIdentifier(sname))
  {
    RootStatementError(DQERR_ID_EXP_AFTER, "struct");
    return;
  }
  if (!CheckSpecialReservedRootName(sname))
  {
    return;
  }

  OCompoundType * ctype = new OCompoundType(sname, cur_mod_scope);
  if (attr->flags)
  {
    attr->CheckInvalidAttributes(ATGT_COMPOUND_TYPE);
    ctype->is_packed = attr->IsSet(ATTF_PACKED);
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(":"))
  {
    Error(DQERR_STMTBLK_START_MISSING);
  }

  OScPosition mempos;
  string membername;

  while (not scf->Eof())
  {
    scf->SkipWhite();

    if (scf->CheckSymbol("endstruct"))
    {
      break;
    }

    if (!ParseAttributes(true))
    {
      SkipCurStatement();
      continue;
    }

    scf->SkipWhite();
    if (scf->CheckSymbol("endstruct"))
    {
      if (attr->flags)
      {
        attr->CheckInvalidAttributes(ATGT_NONE);
      }
      break;
    }

    scf->SaveCurPos(mempos);

    if (not scf->ReadIdentifier(membername))
    {
      StatementError(DQERR_STRUCT_MBID_EXPECTED);
      break;
    }

    scf->SkipWhite();
    if (not scf->CheckSymbol(":"))
    {
      StatementError(DQERR_TYPE_SPECIFIER_EXP_AFTER, membername);
      break;
    }

    OType * mtype = ParseTypeSpec();
    if (not mtype)  break;

    if (!ParseAttributes(false))
    {
      SkipCurStatement();
      continue;
    }

    scf->SkipWhite();
    if (not scf->CheckSymbol(";"))
    {
      StatementError(DQERR_MISSING_SEMICOLON_AFTER, "member definition");
      break;
    }

    OValSym * mvsym = mtype->CreateValSym(mempos, membername);
    mvsym->initialized = true;  // struct members are always accessible
    mvsym->ApplyAttributes(attr, ATGT_STRUCT_MEMBER);
    ctype->AddMember(mvsym);
  }

  ctype->EnsureLayout();

  g_module->DeclareType(section_public, ctype);
}

void ODqCompParser::InjectObjectReceiver(OValSymFunc * vsfunc, OCompoundType * ctype)
{
  if (!vsfunc || !ctype)
  {
    return;
  }

  OTypeFunc * tfunc = dynamic_cast<OTypeFunc *>(vsfunc->ptype);
  if (!tfunc)
  {
    return;
  }

  if (!tfunc->ParNameValid("__this"))
  {
    Error(DQERR_FUNCPAR_NAME_INVALID, "__this");
  }

  tfunc->params.insert(tfunc->params.begin(), new OFuncParam("__this", ctype, FPM_REF));
}

bool ODqCompParser::FinishFunctionDecl(OValSymFunc * vsfunc, OScope * decl_scope, OScope * body_parent_scope,
                                       bool ahidden_decl, bool aallow_external, const string & aowner_desc)
{
  if (!vsfunc || !decl_scope || !body_parent_scope)
  {
    return false;
  }

  OTypeFunc * tfunc = dynamic_cast<OTypeFunc *>(vsfunc->ptype);
  if (!tfunc)
  {
    return false;
  }

  if (!ParseAttributes(false))
  {
    SkipToModuleStatementStart();
    curvsfunc = nullptr;
    delete vsfunc;
    return false;
  }

  vsfunc->ApplyAttributes(attr, ATGT_FUNCTION);

  if (vsfunc->owner_compound_type)
  {
    auto * owner_object = dynamic_cast<OTypeObject *>(vsfunc->owner_compound_type);
    if (    ((OSF_CREATE == vsfunc->object_specfunc_kind) or (OSF_DESTROY == vsfunc->object_specfunc_kind))
        and (vsfunc->attr_is_virtual || vsfunc->attr_is_override || vsfunc->attr_is_abstract || vsfunc->attr_is_final))
    {
      ErrorTxt(DQERR_OBJ_SPEC_FUNC_INVALID, "object lifecycle functions cannot be virtual, override, abstract, or final");
      RecoverFailedFunctionDecl();
      curvsfunc = nullptr;
      delete vsfunc;
      return false;
    }

    OValSymFunc * base_virtual = (owner_object ? owner_object->FindVirtualBaseMethod(vsfunc) : nullptr);
    if (vsfunc->attr_is_override)
    {
      if (!base_virtual)
      {
        ErrorTxt(DQERR_OBJ_FUNC_OVERRIDE, format("method \"{}\" is marked override but no inherited virtual method matches", vsfunc->name));
        RecoverFailedFunctionDecl();
        curvsfunc = nullptr;
        delete vsfunc;
        return false;
      }
      if (base_virtual->attr_is_final)
      {
        ErrorTxt(DQERR_OBJ_FUNC_OVERRIDE, format("method \"{}\" overrides final inherited method", vsfunc->name));
        RecoverFailedFunctionDecl();
        curvsfunc = nullptr;
        delete vsfunc;
        return false;
      }
      vsfunc->attr_is_virtual = true;
    }
    else if (base_virtual)
    {
      ErrorTxt(DQERR_OBJ_FUNC_OVERRIDE, format("method \"{}\" overrides an inherited virtual method but is missing [[override]]", vsfunc->name));
      RecoverFailedFunctionDecl();
      curvsfunc = nullptr;
      delete vsfunc;
      return false;
    }
    if (vsfunc->attr_is_abstract && !vsfunc->attr_is_virtual)
    {
      ErrorTxt(DQERR_OBJ_FUNC_ABSTRACT, format("abstract method \"{}\" must also be virtual", vsfunc->name));
      RecoverFailedFunctionDecl();
      curvsfunc = nullptr;
      delete vsfunc;
      return false;
    }
  }

  auto cleanup_new_func = [&]()
  {
    if (curvsfunc == vsfunc)
    {
      curvsfunc = nullptr;
    }
    delete vsfunc;
    vsfunc = nullptr;
  };

  if (vsfunc->IsSpecial())
  {
    if (vsfunc->owner_compound_type)
    {
      ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, "special functions must be module-level declarations");
      RecoverFailedFunctionDecl();
      cleanup_new_func();
      return false;
    }

    if (((OSF_CREATE != vsfunc->object_specfunc_kind) && vsfunc->attr_is_overload) || vsfunc->is_external)
    {
      ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, "only Create special functions can be overloaded; special functions cannot be external");
      RecoverFailedFunctionDecl();
      cleanup_new_func();
      return false;
    }

    OValSymFunc * existing_special = g_module->FindSpecialFunction(vsfunc->special_kind);
    if (existing_special && !existing_special->IsForwardDecl())
    {
      Error(DQERR_SPECIAL_FUNC_DUPLICATE, SpecialFuncKindName(vsfunc->special_kind));
      RecoverFailedFunctionDecl();
      cleanup_new_func();
      return false;
    }

    if (!SpecialFunctionSignatureIsValid(vsfunc))
    {
      Error(DQERR_SPECIAL_FUNC_SIGNATURE, SpecialFuncKindName(vsfunc->special_kind));
      RecoverFailedFunctionDecl();
      cleanup_new_func();
      return false;
    }

    if (SFK_MAIN == vsfunc->special_kind)
    {
      vsfunc->attr_has_linkage_name = true;
      vsfunc->attr_linkage_name = "dq_main";
    }
  }

  if (vsfunc->is_external && !aallow_external)
  {
    Error(DQERR_FUNC_NO_BODY_ALLOWED_AFTER, aowner_desc);
    RecoverFailedFunctionDecl();
    cleanup_new_func();
    return false;
  }

  if (tfunc->has_varargs && !vsfunc->is_external)
  {
    Error(DQERR_VARARGS_NOT_ALLOWED);
  }

  scf->SkipWhite();
  bool is_declaration_only = scf->CheckSymbol(";", false);

  auto consume_declaration_semicolon = [&](const string & what)
  {
    scf->SkipWhite();
    if (not scf->CheckSymbol(";"))
    {
      Error(DQERR_FUNC_NO_BODY_ALLOWED_AFTER, what);
    }
  };

  auto read_function_body = [&](OValSymFunc * bodyfunc)
  {
    curvsfunc = bodyfunc;
    ReadStatementBlock(bodyfunc->body, "endfunc");

    bodyfunc->scpos_endfunc = scf->prevpos;
    bodyfunc->has_body = true;

    if (bodyfunc->vsresult and not bodyfunc->vsresult->initialized)
    {
      Error(DQERR_FUNC_RESULT_NOT_SET, bodyfunc->name, &bodyfunc->scpos_endfunc);
    }

    ValidateConstructorEmbeddedObjects(bodyfunc);

    curvsfunc = nullptr;
  };

  auto declare_function = [&](OValSymFunc * fn)
  {
    if (ahidden_decl)
    {
      decl_scope->DefineValSym(fn);
      g_module->DeclareHiddenValSym(fn->owner_compound_type != nullptr, fn);
      PrepareFuncDecl(scpos_statement_start, fn);
    }
    else
    {
      AddDeclFunc(scpos_statement_start, fn);
    }
  };

  auto declare_overload_set = [&](OValSymOverloadSet * ovset)
  {
    if (ahidden_decl)
    {
      decl_scope->DefineValSym(ovset);
      g_module->DeclareHiddenValSym(ovset->owner_compound_type != nullptr, ovset);
    }
    else
    {
      AddDeclOverloadSet(scpos_statement_start, ovset);
    }
  };

  auto fill_forward_decl = [&](OValSymFunc * fwdfunc) -> bool
  {
    if (!fwdfunc || !vsfunc)
    {
      return false;
    }

    if (!fwdfunc->CheckForwardDeclMatch(vsfunc))
    {
      RecoverFailedFunctionDecl();
      cleanup_new_func();
      return true;
    }

    if (vsfunc->is_external)
    {
      fwdfunc->MergeForwardDeclFrom(vsfunc, false);
      consume_declaration_semicolon("external function declaration");
      cleanup_new_func();
      return true;
    }

    if (is_declaration_only)
    {
      fwdfunc->MergeForwardDeclFrom(vsfunc, false);
      consume_declaration_semicolon("function declaration");
      cleanup_new_func();
      return true;
    }

    fwdfunc->MergeForwardDeclFrom(vsfunc, true);
    fwdfunc->ResetBodyScope(body_parent_scope);
    PrepareFuncDecl(scpos_statement_start, fwdfunc);
    cleanup_new_func();
    read_function_body(fwdfunc);
    return true;
  };

  OValSym * existing = decl_scope->FindValSym(vsfunc->name, nullptr, false);
  if (!existing && !section_public)
  {
    OValSym * public_existing = g_module->scope_pub->FindValSym(vsfunc->name, nullptr, false);
    if (vsfunc->attr_is_overload)
    {
      if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(public_existing))
      {
        OValSymFunc * matching_decl = ovset->FindMatchingOverloadDecl(tfunc);
        if (matching_decl && matching_decl->IsForwardDecl())
        {
          existing = ovset;
        }
      }
    }
    else if (auto * public_func = dynamic_cast<OValSymFunc *>(public_existing))
    {
      if (public_func->IsForwardDecl())
      {
        existing = public_func;
      }
    }
  }

  if (vsfunc->attr_is_overload)
  {
    OValSymOverloadSet * ovset = dynamic_cast<OValSymOverloadSet *>(existing);
    if (!existing)
    {
      ovset = new OValSymOverloadSet(scpos_statement_start, vsfunc->name, g_builtins->type_func);
      ovset->owner_compound_type = vsfunc->owner_compound_type;
      ovset->generated_linkage_prefix = vsfunc->generated_linkage_name;
      ovset->member_visibility = vsfunc->member_visibility;
      PrepareFuncDecl(scpos_statement_start, vsfunc);
      ovset->AddFunc(vsfunc);
      declare_overload_set(ovset);
    }
    else if (ovset)
    {
      if (!ovset->HasMatchingReturnType(static_cast<OTypeFunc *>(vsfunc->ptype)))
      {
        Error(DQERR_OVERLOAD_RETURN_TYPE, vsfunc->name);
        RecoverFailedFunctionDecl();
        cleanup_new_func();
        return false;
      }

      OValSymFunc * matching_decl = ovset->FindMatchingOverloadDecl(static_cast<OTypeFunc *>(vsfunc->ptype));
      if (matching_decl)
      {
        if (matching_decl->IsForwardDecl())
        {
          return fill_forward_decl(matching_decl);
        }

        Error(DQERR_OVERLOAD_DUP_SIGNATURE, vsfunc->name);
        RecoverFailedFunctionDecl();
        cleanup_new_func();
        return false;
      }

      PrepareFuncDecl(scpos_statement_start, vsfunc);
      ovset->AddFunc(vsfunc);
    }
    else
    {
      Error(DQERR_OVERLOAD_MIXED_DECL, vsfunc->name);
      RecoverFailedFunctionDecl();
      cleanup_new_func();
      return false;
    }
  }
  else
  {
    if (dynamic_cast<OValSymOverloadSet *>(existing))
    {
      Error(DQERR_OVERLOAD_MIXED_DECL, vsfunc->name);
      RecoverFailedFunctionDecl();
      cleanup_new_func();
      return false;
    }

    if (auto * existing_func = dynamic_cast<OValSymFunc *>(existing))
    {
      if (existing_func->IsForwardDecl())
      {
        return fill_forward_decl(existing_func);
      }

      Error(DQERR_VS_ALREADY_DECL_SCOPE, vsfunc->name, decl_scope->debugname);
      RecoverFailedFunctionDecl();
      cleanup_new_func();
      return false;
    }

    if (existing)
    {
      Error(DQERR_VS_ALREADY_DECL_SCOPE, vsfunc->name, decl_scope->debugname);
      RecoverFailedFunctionDecl();
      cleanup_new_func();
      return false;
    }

    declare_function(vsfunc);
  }

  if (vsfunc->is_external)
  {
    consume_declaration_semicolon("external function declaration");
    curvsfunc = nullptr;
    return true;
  }

  if (is_declaration_only)
  {
    consume_declaration_semicolon("function declaration");
    curvsfunc = nullptr;
    return true;
  }

  read_function_body(vsfunc);
  return true;
}

bool ODqCompParser::SpecialFunctionSignatureIsValid(OValSymFunc * vsfunc)
{
  OTypeFunc * tfunc = dynamic_cast<OTypeFunc *>(vsfunc ? vsfunc->ptype : nullptr);
  if (!vsfunc || !tfunc || !tfunc->params.empty() || tfunc->has_varargs)
  {
    return false;
  }

  if (SFK_MAIN == vsfunc->special_kind)
  {
    return tfunc->rettype && (tfunc->rettype->ResolveAlias() == g_builtins->native_int);
  }

  if (SFK_MODULE_INIT == vsfunc->special_kind)
  {
    OType * rettype = tfunc->rettype ? tfunc->rettype->ResolveAlias() : nullptr;
    return !rettype || (TK_VOID == rettype->kind);
  }

  return false;
}

bool ODqCompParser::ReadObjectMethod(OTypeObject * object_type, EMemberVisibility avisibility)
{
  string sid;

  scf->SkipWhite();
  bool specfunc_decl = scf->CheckSymbol("*");

  if (not scf->ReadIdentifier(sid))
  {
    Error(DQERR_ID_EXP_AFTER, "function");
    return false;
  }

  EObjectSpecFuncKind objspecfunc_kind = OSF_NONE;
  string method_name = sid;
  if (specfunc_decl)
  {
    if ("Create" == sid)
    {
      objspecfunc_kind = OSF_CREATE;
      method_name = "Create";
    }
    else if ("Destroy" == sid)
    {
      objspecfunc_kind = OSF_DESTROY;
      method_name = "Destroy";
    }
    else
    {
      ErrorTxt(DQERR_OBJ_SPEC_FUNC_INVALID, sid);
      RecoverFailedFunctionDecl();
      return false;
    }
  }

  OTypeFunc    * tfunc  = new OTypeFunc(method_name);

  OValSymFunc  * vsfunc = new OValSymFunc(scpos_statement_start, method_name, tfunc, object_type->Members());
  vsfunc->owner_compound_type = object_type;
  vsfunc->generated_linkage_name = object_type->name + "." + method_name;
  vsfunc->object_specfunc_kind = objspecfunc_kind;
  vsfunc->member_visibility = avisibility;
  curvsfunc = vsfunc;

  ParseFunctionSignature(tfunc, false, method_name, true);
  if ((OSF_DESTROY == objspecfunc_kind) && (!tfunc->params.empty() || tfunc->rettype))
  {
    ErrorTxt(DQERR_SPECIAL_FUNC_SIGNATURE, "Destroy must not have parameters or a return value");
  }
  if ((OSF_CREATE == objspecfunc_kind) && tfunc->rettype)
  {
    ErrorTxt(DQERR_SPECIAL_FUNC_SIGNATURE, "Create must not have a return value");
  }

  InjectObjectReceiver(vsfunc, object_type);

  bool ok = FinishFunctionDecl(vsfunc, object_type->Members(), object_type->Members(), true, false, "object method declaration");
  if (ok && (OSF_NONE != objspecfunc_kind))
  {
    if (OSF_CREATE == objspecfunc_kind)
    {
      object_type->constructors.push_back(vsfunc);
    }
    else
    {
      object_type->destructor = vsfunc;
    }
  }
  return ok;
}

void ODqCompParser::ValidateConstructorEmbeddedObjects(OValSymFunc * vsfunc)
{
  if (!vsfunc || !vsfunc->owner_compound_type || !vsfunc->body)
  {
    return;
  }

  OTypeObject * object_type = dynamic_cast<OTypeObject *>(vsfunc->owner_compound_type);
  if (!object_type)
  {
    return;
  }
  if (object_type->base_type)
  {
    int inherited_lifecycle_count = 0;
    size_t inherited_lifecycle_index = size_t(-1);
    for (size_t i = 0; i < vsfunc->body->stlist.size(); ++i)
    {
      auto * inherited = dynamic_cast<OStmtInheritedCall *>(vsfunc->body->stlist[i]);
      if (inherited && inherited->method
          && inherited->method->object_specfunc_kind == vsfunc->object_specfunc_kind)
      {
        ++inherited_lifecycle_count;
        inherited_lifecycle_index = i;
      }
    }

    if (OSF_CREATE == vsfunc->object_specfunc_kind)
    {
      if (inherited_lifecycle_count != 1 || inherited_lifecycle_index != 0)
      {
        ErrorTxt(DQERR_OBJ_SPEC_FUNC_INVALID,
                 "derived constructors must call inherited Create exactly once as the first statement",
                 &vsfunc->scpos_endfunc);
      }
    }
    else if (OSF_DESTROY == vsfunc->object_specfunc_kind)
    {
      if (inherited_lifecycle_count != 1 || inherited_lifecycle_index + 1 != vsfunc->body->stlist.size())
      {
        ErrorTxt(DQERR_OBJ_SPEC_FUNC_INVALID,
                 "derived destructors must call inherited Destroy exactly once as the last statement",
                 &vsfunc->scpos_endfunc);
      }
    }
  }

  if (OSF_CREATE != vsfunc->object_specfunc_kind)
  {
    return;
  }

  vector<bool> constructed(object_type->member_order.size(), false);

  for (size_t i = 0; i < object_type->member_order.size(); ++i)
  {
    OValSym * member = object_type->member_order[i];
    auto * objmember = dynamic_cast<OVsObject *>(member);
    if (objmember && objmember->IsFixedObjectStorage() && objmember->ObjectCtorCallAtDecl())
    {
      constructed[i] = true;
    }
  }

  for (OStmt * stmt : vsfunc->body->stlist)
  {
    auto * voidcall = dynamic_cast<OStmtVoidCall *>(stmt);
    auto * callexpr = dynamic_cast<OCallExpr *>(voidcall ? voidcall->callexpr : nullptr);
    if (!callexpr || !callexpr->vsfunc || (OSF_CREATE != callexpr->vsfunc->object_specfunc_kind) || callexpr->args.empty())
    {
      continue;
    }

    auto * objaddr = dynamic_cast<OObjectAddrExpr *>(callexpr->args[0]);
    auto * memberref = dynamic_cast<OLValueMember *>(objaddr ? objaddr->target : nullptr);
    if (!memberref || (memberref->structtype != object_type) || (memberref->memberindex >= object_type->member_order.size()))
    {
      continue;
    }

    OValSym * member = object_type->member_order[memberref->memberindex];
    auto * objmember = dynamic_cast<OVsObject *>(member);
    if (!objmember || !objmember->IsFixedObjectStorage())
    {
      continue;
    }

    if (constructed[memberref->memberindex])
    {
      ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, format("embedded object \"{}\" is constructed twice", member->name), &stmt->scpos);
    }
    constructed[memberref->memberindex] = true;
  }

  for (size_t i = 0; i < object_type->member_order.size(); ++i)
  {
    OValSym * member = object_type->member_order[i];
    auto * objmember = dynamic_cast<OVsObject *>(member);
    if (objmember && objmember->IsFixedObjectStorage() && !constructed[i])
    {
      ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, format("embedded object \"{}\" is not constructed", member->name), &vsfunc->scpos_endfunc);
    }
  }
}

OValSymFunc * ODqCompParser::AddGeneratedObjectConstructor(OTypeObject * object_type, OValSymFunc * inherited_ctor,
                                                           OScPosition & scpos, size_t overload_count)
{
  if (!object_type)
  {
    return nullptr;
  }

  OTypeFunc * inherited_sig = dynamic_cast<OTypeFunc *>(inherited_ctor ? inherited_ctor->ptype : nullptr);
  if (inherited_ctor && !inherited_sig)
  {
    return nullptr;
  }

  OTypeFunc * tfunc = new OTypeFunc("Create");
  if (inherited_sig)
  {
    tfunc->has_varargs = inherited_sig->has_varargs;
    for (size_t i = 1; i < inherited_sig->params.size(); ++i)
    {
      OFuncParam * src = inherited_sig->params[i];
      if (!src)
      {
        continue;
      }
      tfunc->AddParam(src->name, src->ptype, src->mode);
    }
  }

  OValSymFunc * ctor = new OValSymFunc(scpos, "Create", tfunc, object_type->Members());
  ctor->owner_compound_type = object_type;
  ctor->generated_linkage_name = object_type->name + ".Create";
  ctor->object_specfunc_kind = OSF_CREATE;
  ctor->member_visibility = inherited_ctor ? inherited_ctor->member_visibility : MV_PUBLIC;
  ctor->has_body = true;
  ctor->scpos_endfunc.Assign(scpos);
  ctor->attr_is_overload = (overload_count > 1);

  InjectObjectReceiver(ctor, object_type);
  PrepareFuncDecl(scpos, ctor);

  if (inherited_ctor)
  {
    vector<OExpr *> inherited_args;
    for (size_t i = 1; i < ctor->args.size(); ++i)
    {
      inherited_args.push_back(new OLValueVar(ctor->args[i]));
    }
    auto * stmt = new OStmtInheritedCall(scpos, ctor, inherited_ctor, inherited_args);
    stmt->emit_derived_field_init = true;
    ctor->body->stlist.push_back(stmt);
  }

  object_type->constructors.push_back(ctor);

  if (overload_count > 1)
  {
    OValSymOverloadSet * ovset = nullptr;
    OValSym * existing = object_type->Members()->FindValSym("Create", nullptr, false);
    if (!existing)
    {
      ovset = new OValSymOverloadSet(scpos, "Create", g_builtins->type_func);
      ovset->owner_compound_type = object_type;
      ovset->generated_linkage_prefix = object_type->name + ".Create";
      ovset->member_visibility = ctor->member_visibility;
      object_type->Members()->DefineValSym(ovset);
      g_module->DeclareHiddenValSym(true, ovset);
    }
    else
    {
      ovset = dynamic_cast<OValSymOverloadSet *>(existing);
    }

    if (ovset)
    {
      ovset->AddFunc(ctor);
    }
    else
    {
      Error(DQERR_OVERLOAD_MIXED_DECL, "Create");
    }
  }
  else
  {
    object_type->Members()->DefineValSym(ctor);
    g_module->DeclareHiddenValSym(true, ctor);
  }

  return ctor;
}

bool ODqCompParser::CheckObjectCtorArgs(OTypeObject * object_type, vector<OExpr *> & rargs, OValSymFunc *& rctor)
{
  rctor = nullptr;
  if (!object_type)
  {
    return false;
  }

  bool ambiguous = false;
  rctor = object_type->FindConstructorForArgs(rargs, &ambiguous);
  if (!rctor)
  {
    Error(ambiguous ? DQERR_OVERLOAD_AMBIGUOUS : DQERR_OVERLOAD_NO_MATCH, "Create");
    return false;
  }

  OTypeFunc * sigtype = dynamic_cast<OTypeFunc *>(rctor->ptype);
  if (!sigtype || (sigtype->params.size() != rargs.size() + 1))
  {
    Error(DQERR_OVERLOAD_NO_MATCH, "Create");
    return false;
  }

  for (size_t i = 0; i < rargs.size(); ++i)
  {
    if (!CheckAssignType(sigtype->params[i + 1]->ptype, &rargs[i], "constructor argument"))
    {
      return false;
    }
  }

  return true;
}

void ODqCompParser::ParseObjectDecl()
{
  // note: "object" is already consumed
  // syntax form: "object Name:\n  field : type;  function Method(...): ... endfunc\nendobj"

  string sname;
  scf->SkipWhite();
  if (not scf->ReadIdentifier(sname))
  {
    RootStatementError(DQERR_ID_EXP_AFTER, "object");
    return;
  }
  if (!CheckSpecialReservedRootName(sname))
  {
    return;
  }

  OTypeObject * object_type = new OTypeObject(sname, cur_mod_scope);
  if (attr->flags)
  {
    attr->CheckInvalidAttributes(ATGT_COMPOUND_TYPE);
    object_type->is_packed = attr->IsSet(ATTF_PACKED);
  }

  scf->SkipWhite();
  if (scf->CheckSymbol("("))
  {
    OType * basetype = ParseTypeSpec();
    OTypeObject * base_object = dynamic_cast<OTypeObject *>(basetype ? basetype->ResolveAlias() : nullptr);
    if (!base_object)
    {
      Error(DQERR_TYPE_EXPECTED, "object", basetype ? basetype->name : "?");
      SkipToModuleStatementStart();
      delete object_type;
      return;
    }
    object_type->base_type = base_object;

    scf->SkipWhite();
    if (scf->CheckSymbol(","))
    {
      ErrorTxt(DQERR_NOT_SUPPORTED, "multiple object inheritance");
      SkipToModuleStatementStart();
      delete object_type;
      return;
    }
    if (!scf->CheckSymbol(")"))
    {
      Error(DQERR_MISSING_CLOSE_PAREN_FOR, "object base");
      SkipToModuleStatementStart();
      delete object_type;
      return;
    }
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(":"))
  {
    Error(DQERR_STMTBLK_START_MISSING);
  }

  OScPosition mempos;
  string membername;
  EMemberVisibility current_visibility = MV_PUBLIC;

  while (not scf->Eof())
  {
    scf->SkipWhite();

    if (scf->CheckSymbol("endobj"))
    {
      break;
    }

    if (!ParseAttributes(true))
    {
      SkipCurStatement();
      continue;
    }

    scf->SkipWhite();
    if (scf->CheckSymbol("endobj"))
    {
      if (attr->flags)
      {
        attr->CheckInvalidAttributes(ATGT_NONE);
      }
      break;
    }

    scf->SaveCurPos(mempos);
    scpos_statement_start = mempos;

    if (scf->CheckSymbol("function"))
    {
      ReadObjectMethod(object_type, current_visibility);
      continue;
    }

    if (not scf->ReadIdentifier(membername))
    {
      StatementError(DQERR_STRUCT_MBID_EXPECTED);
      break;
    }

    if ("private" == membername)
    {
      current_visibility = MV_PRIVATE;
      continue;
    }
    if ("protected" == membername)
    {
      current_visibility = MV_PROTECTED;
      continue;
    }
    if ("public" == membername)
    {
      current_visibility = MV_PUBLIC;
      continue;
    }

    scf->SkipWhite();
    bool fixed_object = false;
    bool fixed_ctor_call_at_decl = false;
    vector<OExpr *> ctor_args;
    OExpr * field_init_expr = nullptr;
    if (scf->CheckSymbol("<-"))
    {
      fixed_object = true;
      OType * named_type = ParseTypeSpec();
      if (!named_type) break;

      OTypeObject * named_object = dynamic_cast<OTypeObject *>(named_type->ResolveAlias());
      if (!named_object)
      {
        Error(DQERR_TYPE_EXPECTED, "object", named_type->name);
        break;
      }
      if (named_object->is_abstract)
      {
        ErrorTxt(DQERR_NOT_SUPPORTED, format("constructing abstract object \"{}\"", named_object->name));
        break;
      }

      scf->SkipWhite();
      if (scf->CheckSymbol("("))
      {
        fixed_ctor_call_at_decl = true;
        vector<TRawCallArg> rawargs;
        if (!ParseRawCallArguments(membername, rawargs))
        {
          break;
        }
        for (TRawCallArg & rawarg : rawargs)
        {
          ctor_args.push_back(rawarg.expr);
          rawarg.expr = nullptr;
        }
        FreeRawCallArguments(rawargs);
      }

      if (!ParseAttributes(false))
      {
        SkipCurStatement();
        continue;
      }

      if (fixed_ctor_call_at_decl)
      {
        OValSymFunc * ctor = nullptr;
        if (!CheckObjectCtorArgs(named_object, ctor_args, ctor))
        {
          break;
        }
      }

      scf->SkipWhite();
      if (not scf->CheckSymbol(";"))
      {
        StatementError(DQERR_MISSING_SEMICOLON_AFTER, "member definition");
        break;
      }

      OValSym * mvsym = named_type->CreateValSym(mempos, membername);
      mvsym->initialized = true;
      auto * objsym = dynamic_cast<OVsObject *>(mvsym);
      if (!objsym)
      {
        throw logic_error(format("Embedded object member \"{}\" was not created as OVsObject", membername));
      }
      objsym->SetObjectStorage(OSK_OBJECT_FIXED);
      objsym->SetObjectCtorArgs(std::move(ctor_args));
      objsym->SetObjectCtorCallAtDecl(fixed_ctor_call_at_decl);
      mvsym->member_visibility = current_visibility;
      mvsym->ApplyAttributes(attr, ATGT_STRUCT_MEMBER);
      object_type->AddMember(mvsym);
      continue;
    }

    if (not scf->CheckSymbol(":"))
    {
      StatementError(DQERR_TYPE_SPECIFIER_EXP_AFTER, membername);
      break;
    }

    OType * mtype = ParseTypeSpec();
    if (not mtype)  break;

    OTypeObject * member_object = dynamic_cast<OTypeObject *>(mtype->ResolveAlias());
    bool object_ref_member = member_object;

    scf->SkipWhite();
    if (scf->CheckSymbol("="))
    {
      field_init_expr = ParseExpression();
      if (!field_init_expr)
      {
        break;
      }
      if (!CheckAssignType(mtype, &field_init_expr, "field initializer"))
      {
        delete field_init_expr;
        break;
      }
    }

    if (!ParseAttributes(false))
    {
      SkipCurStatement();
      continue;
    }

    scf->SkipWhite();
    if (not scf->CheckSymbol(";"))
    {
      StatementError(DQERR_MISSING_SEMICOLON_AFTER, "member definition");
      break;
    }

    OValSym * mvsym = mtype->CreateValSym(mempos, membername);
    mvsym->initialized = true;
    if (object_ref_member)
    {
      auto * objsym = dynamic_cast<OVsObject *>(mvsym);
      if (!objsym)
      {
        throw logic_error(format("Object reference member \"{}\" was not created as OVsObject", membername));
      }
      objsym->SetObjectStorage(OSK_OBJECT_REF);
    }
    mvsym->field_init_expr = field_init_expr;
    mvsym->member_visibility = current_visibility;
    mvsym->ApplyAttributes(attr, ATGT_STRUCT_MEMBER);
    object_type->AddMember(mvsym);
  }

  bool need_generated_ctors = object_type->constructors.empty();
  if (need_generated_ctors && object_type->base_type && !object_type->base_type->constructors.empty())
  {
    size_t ctor_count = object_type->base_type->constructors.size();
    for (size_t i = 0; i < ctor_count; ++i)
    {
      AddGeneratedObjectConstructor(object_type, object_type->base_type->constructors[i],
                                    scpos_statement_start, ctor_count);
    }
    need_generated_ctors = false;
  }

  bool needs_implicit_ctor = need_generated_ctors;
  if (needs_implicit_ctor)
  {
    needs_implicit_ctor = false;
    for (OValSym * member : object_type->member_order)
    {
      auto * objmember = dynamic_cast<OVsObject *>(member);
      if (member && (member->field_init_expr || (objmember && objmember->ObjectCtorCallAtDecl())))
      {
        needs_implicit_ctor = true;
        break;
      }
    }
  }

  if (needs_implicit_ctor)
  {
    if (object_type->base_type)
    {
      if (!object_type->base_type->FindSpecialMethod(OSF_CREATE, 0))
      {
        Error(DQERR_OVERLOAD_NO_MATCH, "Create");
        needs_implicit_ctor = false;
      }
    }
  }

  if (needs_implicit_ctor)
  {
    OValSymFunc * ctor = AddGeneratedObjectConstructor(object_type, nullptr, scpos_statement_start, 1);
    if (object_type->base_type)
    {
      OValSymFunc * inherited_ctor = object_type->base_type->FindSpecialMethod(OSF_CREATE, 0);
      auto * stmt = new OStmtInheritedCall(scpos_statement_start, ctor, inherited_ctor);
      stmt->emit_derived_field_init = true;
      ctor->body->stlist.push_back(stmt);
    }
  }

  object_type->UpdateObjectInheritanceFlags();
  object_type->EnsureLayout();
  g_module->DeclareType(section_public, object_type);
}

void ODqCompParser::ParseQualifiedObjectFunction(const string & object_name)
{
  string method_name;
  scf->SkipWhite();
  if (not scf->ReadIdentifier(method_name))
  {
    Error(DQERR_ID_EXP_AFTER, object_name + ".");
    return;
  }

  OTypeFunc * tfunc = new OTypeFunc(method_name);
  ParseFunctionSignature(tfunc, false, method_name, true);

  OType * foundtype = cur_mod_scope->FindType(object_name);
  if (!foundtype)
  {
    Error(DQERR_TYPE_UNKNOWN, object_name);
    delete tfunc;
    RecoverFailedFunctionDecl();
    return;
  }

  OTypeObject * object_type = dynamic_cast<OTypeObject *>(foundtype->ResolveAlias());
  if (!object_type)
  {
    Error(DQERR_TYPE_EXPECTED, "object", foundtype->name);
    delete tfunc;
    RecoverFailedFunctionDecl();
    return;
  }

  OValSymFunc  * vsfunc = new OValSymFunc(scpos_statement_start, method_name, tfunc, object_type->Members());
  vsfunc->owner_compound_type = object_type;
  vsfunc->generated_linkage_name = object_type->name + "." + method_name;
  curvsfunc = vsfunc;

  InjectObjectReceiver(vsfunc, object_type);
  FinishFunctionDecl(vsfunc, object_type->Members(), object_type->Members(), true, false, "object method declaration");
}

void ODqCompParser::ParseFunction()
{
  // note: "function" is already consumed
  // syntax form: "function identifier[(arglist)] [-> return_type] <statement_block | ;>"
  // statement block must follow, when ';' then it is a forward declaration

  string   sid;

  scf->SkipWhite();
  bool special_decl = scf->CheckSymbol("*");
  if (not scf->ReadIdentifier(sid))
  {
    Error(DQERR_ID_EXP_AFTER, "function");
    return;
  }

  ESpecialFuncKind special_kind = SFK_NONE;
  if (special_decl)
  {
    special_kind = SpecialFuncKindFromName(sid);
    if (SFK_NONE == special_kind)
    {
      Error(DQERR_SPECIAL_FUNC_UNKNOWN, sid);
      RecoverFailedFunctionDecl();
      return;
    }
  }
  else if (SFK_NONE != SpecialFuncKindFromName(sid))
  {
    Error(DQERR_SPECIAL_FUNC_RESERVED, sid);
    RecoverFailedFunctionDecl();
    return;
  }

  scf->SkipWhite();
  if (scf->CheckSymbol("."))
  {
    if (special_decl)
    {
      ErrorTxt(DQERR_SPECIAL_FUNC_INVALID, "special functions are not valid as object methods");
      RecoverFailedFunctionDecl();
      return;
    }
    ParseQualifiedObjectFunction(sid);
    return;
  }

  OTypeFunc    * tfunc  = new OTypeFunc(sid);
  OValSymFunc  * vsfunc = new OValSymFunc(scpos_statement_start, sid, tfunc, cur_mod_scope);
  vsfunc->special_kind = special_kind;
  curvsfunc = vsfunc;

  ParseFunctionSignature(tfunc, false, sid, true);
  FinishFunctionDecl(vsfunc, cur_mod_scope, cur_mod_scope, false, true, "function declaration");
}

void ODqCompParser::ReadStatementBlock(OStmtBlock * stblock, const string blockend, string * rendstr)
{

  OStmtBlock * prev_block = curblock;
  OScope *     prev_scope = curscope;

  curblock = stblock;
  curscope = stblock->scope;

  string block_closer;
  string sid;

  scf->SkipWhite();
  if (scf->CheckSymbol("{"))
  {
    block_closer = "}";
  }
  else if (scf->CheckSymbol(":"))
  {
    block_closer = blockend;
  }
  else
  {
    StatementError(DQERR_STMTBLK_START_MISSING);
    block_closer = blockend;
  }

  bool endfound = false;
  while (not scf->Eof())
  {
    scf->SkipWhite();

    auto split_view = block_closer | views::split('|');
    for (auto chunk : split_view)
    {
      string chunkstr = string(string_view(chunk));
      if (scf->CheckSymbol(chunkstr.c_str()))
      {
        endfound = true;
        if (rendstr)  *rendstr = chunkstr;
        break;
      }
    }

    if (endfound)
    {
      break;  // block closer was found
    }

    if (scf->Eof())
    {
      if (rendstr)  *rendstr = "";
      StatementError(DQERR_STMTBLK_CLOSE_MISSING, block_closer);
      break;
    }

    scf->SaveCurPos(scpos_statement_start);

    if (scf->CheckSymbol(";"))  // empty ";", just ignore it
    {
      Hint(DQHINT_MEANINGLESS_SEMICOLON);
      continue;
    }

    // Try keywords first, use ReadIdentifier for whole word checking

    scf->SaveCurPos(scpos_statement_start);  // we jump back here if the identifier is unknown
    if (scf->ReadIdentifier(sid))
    {
      if ("var" == sid)  // local variable declaration
      {
        ParseStmtVar(false);
        continue;
      }
      else if ("ref" == sid)
      {
        ParseStmtRef();
        continue;
      }
      else if ("refin" == sid || "refout" == sid || "refnull" == sid)
      {
        StatementError(DQERR_REF_LOCAL_MODE_UNSUPPORTED, sid);
        continue;
      }
      else if ("const" == sid)
      {
        ParseStmtConst(false);
        continue;
      }
      else if ("return" == sid)
      {
        ParseStmtReturn();
        continue;
      }
      else if ("break" == sid)
      {
        if (loop_depth < 1)
        {
          StatementError(DQERR_STMT_INVALID, sid);
          continue;
        }
        if (!CheckStatementClose())
        {
          continue;
        }
        curblock->AddStatement(new OBreakStmt(scpos_statement_start));
        continue;
      }
      else if ("continue" == sid)
      {
        if (loop_depth < 1)
        {
          StatementError(DQERR_STMT_INVALID, sid);
          continue;
        }
        if (!CheckStatementClose())
        {
          continue;
        }
        curblock->AddStatement(new OContinueStmt(scpos_statement_start));
        continue;
      }
      else if ("while" == sid)
      {
        ParseStmtWhile();
        continue;
      }
      else if ("for" == sid)
      {
        ParseStmtFor();
        continue;
      }
      else if ("if" == sid)
      {
        ParseStmtIf();
        continue;
      }
      else if ("delete" == sid)
      {
        ParseStmtDelete();
        continue;
      }
      else if ("inherited" == sid)
      {
        ParseStmtInherited();
        continue;
      }
      else if (ReservedWord(sid))
      {
        StatementError(DQERR_STMT_INVALID, sid);
        continue;
      }
      else  // not handled, restore position and go on with expression parsing
      {
        scf->SetCurPos(scpos_statement_start);
      }
    }

    // Now, there are two possibilities: function call or  - assignment
    // Both start with an expression

    int prev_errorcnt = errorcnt;
    suppressed_varinit_diags.clear();
    supress_varinit_check = true;  // do not generate variable not initialized error for the left value
    OExpr * leftexpr = ParseExpression();
    supress_varinit_check = false;
    if (!leftexpr)
    {
      EmitSuppressedVarInitDiags();
      if (prev_errorcnt == errorcnt)  // no error was generated yet ?
      {
        Error(DQERR_EXPR_EXPECTED, &scpos_statement_start);
      }
      SkipToStatementEnd();  // try to find the ";"
      continue;
    }

    scf->SkipWhite();

    // check for assignment operator

    EBinOp binop = ODqCompParser::ParseAssignOp();
    if (int(binop) >= 0)
    {
      scf->SkipWhite();
      prev_errorcnt = errorcnt;
      OExpr * rightexpr = ParseExpression();
      if (!rightexpr)
      {
        if (prev_errorcnt == errorcnt)  // no error was generated yet ?
        {
          Error(DQERR_EXPR_EXPECTED);
        }
        OLValueExpr * lval = dynamic_cast<OLValueExpr *>(leftexpr);
        if (lval)
        {
          EmitFilteredAssignVarInitDiags(lval, binop);
        }
        else
        {
          EmitSuppressedVarInitDiags();
        }
        delete leftexpr;
        SkipToStatementEnd();  // try to find the ";"
        continue;
      }

      scf->SkipWhite();
      if (!scf->CheckSymbol(";"))
      {
        OScPosition scpos;
        scf->SaveCurPos(scpos);
        StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "assignment statement", &scpos);
      }

      OLValueExpr * lval = dynamic_cast<OLValueExpr *>(leftexpr);
      if (!lval)
      {
        EmitSuppressedVarInitDiags();
        Error(DQERR_LVALUE_NOT_WRITEABLE);
        delete leftexpr;
        delete rightexpr;
        continue;
      }

      EmitFilteredAssignVarInitDiags(lval, binop);
      FinalizeStmtAssign(lval, binop, rightexpr);
      continue;
    }

    // the leftexpr should be a callable expression
    bool is_call_stmt = (dynamic_cast<OCallExpr *>(leftexpr) != nullptr)
                     || (dynamic_cast<OIndirectCallExpr *>(leftexpr) != nullptr);
    if (!is_call_stmt)
    {
      EmitSuppressedVarInitDiags();
      StatementError(DQERR_STMT_ASSIGN_OR_FCALL_EXP);
      scf->SkipWhite();
      if (!scf->CheckSymbol(";"))
      {
        SkipToStatementEnd();
      }
      delete leftexpr;
      continue;
    }

    EmitSuppressedVarInitDiags();
    scf->SkipWhite();
    if (!scf->CheckSymbol(";"))
    {
      OScPosition scpos;
      scf->SaveCurPos(scpos);
      StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "function call statement", &scpos);
    }

    FinalizeStmtVoidCall(leftexpr);

  }

  curscope = prev_scope;
  curblock = prev_block;
}

OType * ODqCompParser::ParseTypeSpec(bool aemit_errors)
{
  // Parses type specification after ":"
  // Handles prefix type constructors: ^type, [N]type, []type

  scf->SkipWhite();

  if (scf->CheckSymbol("^"))
  {
    OType * basetype = ParseTypeSpec(aemit_errors);
    if (!basetype)
    {
      return nullptr;
    }
    return basetype->GetPointerType();
  }

  if (scf->CheckSymbol("["))
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("]"))
    {
      OType * elemtype = ParseTypeSpec(aemit_errors);
      if (!elemtype)
      {
        return nullptr;
      }
      return elemtype->GetSliceType();
    }

    if (scf->CheckSymbol("..."))
    {
      if (aemit_errors)
      {
        Error(DQERR_NOT_IMPLEMENTED_YET, "Dynamic array ([...]int)");
      }
      scf->ReadTo("]");
      scf->CheckSymbol("]");
      return nullptr;
    }

    int64_t arrlen;
    if (not scf->ReadInt64Value(arrlen))
    {
      if (aemit_errors)
      {
        Error(DQERR_ARRAY_SIZESPEC);
      }
      return nullptr;
    }
    if (arrlen <= 0)
    {
      if (aemit_errors)
      {
        Error(DQERR_SIZE_SPEC, "Array");
      }
      return nullptr;
    }
    scf->SkipWhite();
    if (not scf->CheckSymbol("]"))
    {
      if (aemit_errors)
      {
        Error(DQERR_MISSING_CLOSE_BRACKET_FOR, "array size");
      }
      return nullptr;
    }

    OType * elemtype = ParseTypeSpec(aemit_errors);
    if (!elemtype)
    {
      return nullptr;
    }
    return elemtype->GetArrayType(uint32_t(arrlen));
  }

  scf->SkipWhite();
  OType * ptype = nullptr;
  string stype;

  if (scf->CheckSymbol("function"))
  {
    OTypeFunc * sigtype = ParseFunctionType(aemit_errors, "function");
    if (!sigtype)
    {
      return nullptr;
    }

    scf->SkipWhite();
    if (scf->CheckSymbol("of"))
    {
      scf->SkipWhite();
      if (scf->CheckSymbol("object"))
      {
        if (aemit_errors)
        {
          Error(DQERR_NOT_IMPLEMENTED_YET, "\"function(...) of object\"");
        }
        delete sigtype;
        return nullptr;
      }

      if (aemit_errors)
      {
        Error(DQERR_NOT_IMPLEMENTED_YET, "\"function(...) of ...\"");
      }
      delete sigtype;
      return nullptr;
    }

    ptype = new OTypeFuncRef(sigtype);
  }
  else if (not scf->ReadIdentifier(stype))
  {
    if (aemit_errors)
    {
      Error(DQERR_TYPE_ID_EXP);
    }
    return nullptr;
  }
  else
  {
    ptype = cur_mod_scope->FindType(stype);
    if (not ptype)
    {
      if (aemit_errors)
      {
        Error(DQERR_TYPE_UNKNOWN, stype, &scf->prevpos);
      }
      return nullptr;
    }
    ptype = ptype->ResolveAlias();
  }

  // cstring[N] handling: [N] means sized cstring, not array
  if (TK_STRING == ptype->kind)
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("[[", false))
    {
      return ptype;
    }
    if (scf->CheckSymbol("["))
    {
      int64_t maxlen;
      if (not scf->ReadInt64Value(maxlen))
      {
        if (aemit_errors)
        {
          Error(DQERR_CSTR_SIZE_EXPECTED);
        }
        return nullptr;
      }
      if (maxlen <= 0)
      {
        if (aemit_errors)
        {
          Error(DQERR_CSTR_SIZE_EXPECTED);
        }
        return nullptr;
      }
      scf->SkipWhite();
      if (not scf->CheckSymbol("]"))
      {
        if (aemit_errors)
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_FOR, "cstring size");
        }
        return nullptr;
      }
      return g_builtins->type_cstring->GetSizedType(uint32_t(maxlen));
    }
    return ptype;  // unsized cstring (for parameters)
  }

  // Array suffixes are no longer part of the type grammar. Keep [[...]]
  // available for attributes after the type specifier.
  scf->SkipWhite();
  if (scf->CheckSymbol("[[", false))
  {
    return ptype;
  }
  if (scf->CheckSymbol("[", false))
  {
    if (aemit_errors)
    {
      Error(DQERR_NOT_SUPPORTED, "Postfix array type syntax; use [N]T or []T");
    }
    return nullptr;
  }

  return ptype;
}

bool ODqCompParser::ParseFunctionSignature(OTypeFunc * tfunc, bool atypespec, const string & aowner_name, bool aemit_errors)
{
  if (!tfunc)
  {
    return false;
  }

  bool berror = false;

  auto fail_or_recover = [&, this]() -> bool
  {
    berror = true;

    if (!scf->ReadTo(",)"))
    {
      return false;
    }

    scf->CheckSymbol(",");
    return true;
  };

  scf->SkipWhite();
  bool has_param_list = scf->CheckSymbol("(");
  if (!has_param_list)
  {
    if (atypespec)
    {
      if (aemit_errors)
      {
        Error(DQERR_MISSING_OPEN_PAREN_AFTER, "function");
      }
      return false;
    }
  }
  else
  {
    bool default_seen = false;

    while (not scf->Eof())
    {
      scf->SkipWhite();
      if (scf->CheckSymbol(")"))
      {
        break;
      }

      if (!tfunc->params.empty())
      {
        if (!scf->CheckSymbol(","))
        {
          if (aemit_errors)
          {
            Error(DQERR_MISSING_COMMA, &scf->prevpos);
          }
          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }
        scf->SkipWhite();
      }

      if (scf->CheckSymbol("..."))
      {
        tfunc->has_varargs = true;
        scf->SkipWhite();
        if (!scf->CheckSymbol(")"))
        {
          if (aemit_errors)
          {
            Error(DQERR_MISSING_CLOSE_PAREN_AFTER, "...");
          }
          if (atypespec)
          {
            return false;
          }
        }
        break;
      }

      string spname;
      if (!scf->ReadIdentifier(spname))
      {
        if (aemit_errors)
        {
          Error(DQERR_FUNCPAR_NAME_EXP, &scf->prevpos);
        }
        if (!fail_or_recover())
        {
          break;
        }
        continue;
      }

      if (!tfunc->ParNameValid(spname))
      {
        if (aemit_errors)
        {
          Error(DQERR_FUNCPAR_NAME_INVALID, spname, &scf->prevpos);
        }
        if (!fail_or_recover())
        {
          break;
        }
        continue;
      }

      scf->SkipWhite();
      if (!scf->CheckSymbol(":"))
      {
        if (aemit_errors)
        {
          Error(DQERR_TYPE_SPECIFIER_EXP_AFTER, spname, &scf->prevpos);
        }
        if (!fail_or_recover())
        {
          break;
        }
        continue;
      }

      EParamMode pmode = FPM_VALUE;
      scf->SkipWhite();
      string mode_keyword;
      if (scf->ReadIdentifier(mode_keyword, false) && ParseParamModeKeyword(mode_keyword, pmode))
      {
        scf->ReadIdentifier(mode_keyword);
      }

      OType * ptype = ParseTypeSpec(aemit_errors);
      if (!ptype)
      {
        if (atypespec)
        {
          return false;
        }

        if (!fail_or_recover())
        {
          break;
        }
        continue;
      }

      OFuncParam * fparam = tfunc->AddParam(spname, ptype, pmode);

      scf->SkipWhite();
      if (scf->CheckSymbol("="))
      {
        if (ParamModeIsRefLike(pmode))
        {
          if (aemit_errors)
          {
            Error(DQERR_FUNCPAR_DEFAULT_REF, spname);
          }
          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        if (!SupportsFuncParamDefaultType(ptype))
        {
          if (aemit_errors)
          {
            Error(DQERR_FUNCPAR_DEFAULT_TYPE, spname, ptype->ResolveAlias()->name);
          }
          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        default_seen = true;

        scf->SkipWhite();
        OScPosition defexpr_pos;
        scf->SaveCurPos(defexpr_pos);

        OExpr * defexpr = ParseExpression();
        if (!defexpr)
        {
          if (atypespec)
          {
            return false;
          }

          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        if (!CheckAssignType(ptype, &defexpr, "Argument"))
        {
          OExpr::DeleteTree(defexpr);
          if (atypespec)
          {
            return false;
          }

          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        OValue * defvalue = ptype->CreateValue();
        if (!defvalue)
        {
          if (aemit_errors)
          {
            Error(DQERR_FUNCPAR_DEFAULT_TYPE, spname, ptype->ResolveAlias()->name, &defexpr_pos);
          }
          OExpr::DeleteTree(defexpr);
          if (atypespec)
          {
            return false;
          }

          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        if (!defvalue->CalculateConstant(defexpr, true))
        {
          delete defvalue;
          OExpr::DeleteTree(defexpr);
          if (atypespec)
          {
            return false;
          }

          if (!fail_or_recover())
          {
            break;
          }
          continue;
        }

        fparam->defvalue = new OValSymConst(defexpr_pos, format("__defarg_{}_{}", aowner_name, spname), ptype, defvalue);
        OExpr::DeleteTree(defexpr);
      }
      else if (default_seen)
      {
        if (aemit_errors)
        {
          Error(DQERR_FUNCPAR_DEFAULT_ORDER, spname);
        }

        if (atypespec)
        {
          return false;
        }
      }
    } // while

    if (berror and atypespec)
    {
      return false;
    }

  }

  if (tfunc->has_varargs && tfunc->params.empty())
  {
    if (aemit_errors)
    {
      Error(DQERR_VARARGS_ALONE);
    }
    if (atypespec)
    {
      return false;
    }
  }

  scf->SkipWhite();
  if (scf->CheckSymbol("->"))
  {
    tfunc->rettype = ParseTypeSpec(aemit_errors);
    if (!tfunc->rettype)
    {
      return false;
    }
  }

  return true;
}

OTypeFunc * ODqCompParser::ParseFunctionType(bool aemit_errors, const string & aowner_name)
{
  OTypeFunc * tfunc = new OTypeFunc(aowner_name);

  if (!ParseFunctionSignature(tfunc, true, aowner_name, aemit_errors))
  {
    delete tfunc;
    return nullptr;
  }

  return tfunc;
}

void ODqCompParser::ParseStmtReturn()
{
  // "return" is already consumed.
  scf->SkipWhite();
  if (scf->CheckSymbol(";"))  // return without value, use the result variable to return
  {
    curblock->AddStatement(new OStmtReturn(scpos_statement_start, nullptr, curvsfunc));
    return;
  }

  if (not curvsfunc->vsresult)
  {
    Error(DQERR_FUNC_RESULT_SPECIFIED, curvsfunc->name);
    return;
  }

  OExpr * expr = ParseExpression();
  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    Error(DQERR_MISSING_SEMICOLON_AFTER, "the return expression");
  }
  if (expr)
  {
    curblock->scope->SetVarInitialized(curvsfunc->vsresult);
    curblock->AddStatement(new OStmtReturn(scpos_statement_start, expr, curvsfunc));
  }
}

void ODqCompParser::ParseStmtDelete()
{
  OValSymFunc * memfree_func = dynamic_cast<OValSymFunc *>(curscope->FindValSym("MemFree"));
  if (!memfree_func)
  {
    Error(DQERR_VS_UNKNOWN, "MemFree");
    SkipToStatementEnd();
    return;
  }

  scf->SkipWhite();
  OExpr * ptrexpr = ParseExpression();
  if (!ptrexpr)
  {
    return;
  }

  OType * ptrtype = ptrexpr->ResolvedType();
  OTypeObject * delete_object_type = dynamic_cast<OTypeObject *>(ptrtype);
  bool deleting_object = delete_object_type;
  bool clear_after_free = deleting_object;
  if (!ptrtype || ((TK_POINTER != ptrtype->kind) && !deleting_object))
  {
    string got = (ptrtype ? ptrtype->name : "?");
    delete ptrexpr;
    Error(DQERR_TYPE_EXPECTED, "pointer", got);
    SkipToStatementEnd();
    return;
  }
  if (deleting_object)
  {
    OLValueExpr * lval = dynamic_cast<OLValueExpr *>(ptrexpr);
    OValSym * rootvalsym = (lval ? GetAssignRootValSym(lval) : nullptr);
    if (!lval)
    {
      delete ptrexpr;
      Error(DQERR_LVALUE_NOT_WRITEABLE);
      SkipToStatementEnd();
      return;
    }
    if (lval->IsFixedObjectStorageExpr())
    {
      delete ptrexpr;
      Error(DQERR_REF_ASSIGN_READONLY, "fixed object");
      SkipToStatementEnd();
      return;
    }
    if (rootvalsym && ((VSK_CONST == rootvalsym->kind) || !rootvalsym->IsRefWriteable()))
    {
      delete ptrexpr;
      Error(DQERR_REF_ASSIGN_READONLY, rootvalsym->name);
      SkipToStatementEnd();
      return;
    }
  }

  scf->SkipWhite();
  if (scf->CheckSymbol("="))
  {
    clear_after_free = true;
    scf->SkipWhite();
    if (!scf->CheckSymbol("null"))
    {
      delete ptrexpr;
      Error(DQERR_EXPR_WRONG_VALUE_FOR, "delete");
      SkipToStatementEnd();
      return;
    }

    OLValueExpr * lval = dynamic_cast<OLValueExpr *>(ptrexpr);
    OValSym * rootvalsym = (lval ? GetAssignRootValSym(lval) : nullptr);
    if (!lval)
    {
      delete ptrexpr;
      Error(DQERR_LVALUE_NOT_WRITEABLE);
      SkipToStatementEnd();
      return;
    }
    if (rootvalsym && VSK_CONST == rootvalsym->kind)
    {
      delete ptrexpr;
      Error(DQERR_TYPE_ASSIGN_TO_CONST, rootvalsym->name);
      SkipToStatementEnd();
      return;
    }
    if (rootvalsym && !rootvalsym->IsRefWriteable())
    {
      delete ptrexpr;
      Error(DQERR_REF_ASSIGN_READONLY, rootvalsym->name);
      SkipToStatementEnd();
      return;
    }
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(";"))
  {
    StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "delete statement");
  }

  auto * delstmt = new OStmtDelete(scpos_statement_start, ptrexpr, clear_after_free, memfree_func);
  if (deleting_object)
  {
    delstmt->object_dtor_func = delete_object_type->FindSpecialMethod(OSF_DESTROY);
  }
  curblock->AddStatement(delstmt);
}

OValSymFunc * ODqCompParser::FindInheritedMethod(const string & method_name, const vector<OExpr *> & args)
{
  auto * owner_object = dynamic_cast<OTypeObject *>(curvsfunc ? curvsfunc->owner_compound_type : nullptr);
  if (!owner_object || !owner_object->base_type)
  {
    return nullptr;
  }

  for (OTypeObject * cur = owner_object->base_type; cur; cur = cur->base_type)
  {
    if ("Create" == method_name)
    {
      OValSymFunc * fn = cur->FindSpecialMethod(OSF_CREATE, args.size());
      return (ObjectMemberAccessAllowed(cur, fn) ? fn : nullptr);
    }
    if ("Destroy" == method_name)
    {
      OValSymFunc * fn = (args.empty() ? cur->FindSpecialMethod(OSF_DESTROY) : nullptr);
      return (ObjectMemberAccessAllowed(cur, fn) ? fn : nullptr);
    }

    OValSym * vs = cur->Members()->FindValSym(method_name, nullptr, false);
    if (auto * fn = dynamic_cast<OValSymFunc *>(vs))
    {
      if (!ObjectMemberAccessAllowed(cur, fn))
      {
        return nullptr;
      }
      OTypeFunc * sig = dynamic_cast<OTypeFunc *>(fn->ptype);
      if (sig && sig->params.size() == args.size() + 1)
      {
        return fn;
      }
    }
    if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(vs))
    {
      if (!ObjectMemberAccessAllowed(cur, ovset))
      {
        return nullptr;
      }
      for (OValSymFunc * fn : ovset->funcs)
      {
        OTypeFunc * sig = dynamic_cast<OTypeFunc *>(fn ? fn->ptype : nullptr);
        if (sig && sig->params.size() == args.size() + 1)
        {
          return fn;
        }
      }
    }
  }
  return nullptr;
}

void ODqCompParser::ParseStmtInherited()
{
  auto * owner_object = dynamic_cast<OTypeObject *>(curvsfunc ? curvsfunc->owner_compound_type : nullptr);
  if (!owner_object || !owner_object->base_type)
  {
    StatementError(DQERR_INHERITED_CALL_INVALID, "inherited is only valid inside a derived object method");
    SkipToStatementEnd();
    return;
  }

  string method_name;
  vector<OExpr *> args;
  scf->SkipWhite();
  if (scf->CheckSymbol(";"))
  {
    if ((OSF_CREATE == curvsfunc->object_specfunc_kind) or (OSF_DESTROY == curvsfunc->object_specfunc_kind))
    {
      ErrorTxt(DQERR_INHERITED_CALL_INVALID, "short form of inherited is not valid in lifecycle methods");
      return;
    }
    method_name = curvsfunc->name;
    for (size_t i = 1; i < curvsfunc->args.size(); ++i)
    {
      args.push_back(new OLValueVar(curvsfunc->args[i]));
    }
  }
  else
  {
    if (!scf->ReadIdentifier(method_name))
    {
      StatementError(DQERR_ID_EXP_AFTER, "inherited");
      SkipToStatementEnd();
      return;
    }
    scf->SkipWhite();
    if (!scf->CheckSymbol("("))
    {
      Error(DQERR_FUNC_CALL_PARENTH, method_name);
      SkipToStatementEnd();
      return;
    }
    vector<TRawCallArg> rawargs;
    if (!ParseRawCallArguments(method_name, rawargs))
    {
      return;
    }
    for (TRawCallArg & rawarg : rawargs)
    {
      args.push_back(rawarg.expr);
      rawarg.expr = nullptr;
    }
    FreeRawCallArguments(rawargs);

    scf->SkipWhite();
    if (!scf->CheckSymbol(";"))
    {
      StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "inherited statement");
    }
  }

  OValSymFunc * method = FindInheritedMethod(method_name, args);
  OTypeFunc * sig = dynamic_cast<OTypeFunc *>(method ? method->ptype : nullptr);
  if (!method || !sig || sig->params.size() != args.size() + 1)
  {
    for (OExpr * arg : args) OExpr::DeleteTree(arg);
    Error(DQERR_OVERLOAD_NO_MATCH, method_name);
    return;
  }
  for (size_t i = 0; i < args.size(); ++i)
  {
    if (!CheckAssignType(sig->params[i + 1]->ptype, &args[i], "inherited argument"))
    {
      for (OExpr * arg : args) OExpr::DeleteTree(arg);
      return;
    }
  }

  auto * stmt = new OStmtInheritedCall(scpos_statement_start, curvsfunc, method, args);
  stmt->emit_derived_field_init = (OSF_CREATE == curvsfunc->object_specfunc_kind && "Create" == method_name);
  stmt->emit_derived_field_destroy = (OSF_DESTROY == curvsfunc->object_specfunc_kind && "Destroy" == method_name);
  curblock->AddStatement(stmt);
}

void ODqCompParser::ParseStmtWhile()
{
  // note: "while" is already consumed
  // syntax form: "while <condition>: <statement_block> endwhile"

  scf->SkipWhite();

  OExpr * cond = ParseExpression();
  if (!cond)
  {
    StatementError(DQERR_CONDEXPR_MISSING_FOR, "while");
    return;
  }

  OStmtWhile * st = new OStmtWhile(scpos_statement_start, cond, curscope);
  curblock->AddStatement(st);

  ++loop_depth;
  ReadStatementBlock(st->body, "endwhile");
  --loop_depth;

  st->body->scope->RevertFirstAssignments();
}

void ODqCompParser::ParseStmtFor()
{
  // note: "for" is already consumed
  // syntax forms:
  //   for i = start to|downto end [step step_expr]: ... endfor
  //   for i = start count|downcount count_expr [step step_expr]: ... endfor
  //   for i = start while condition [step step_expr]: ... endfor
  //   for i : T = start ...

  enum class EForKind
  {
    TO,
    DOWNTO,
    COUNT,
    DOWNCOUNT,
    WHILE
  };

  OScope * saved_scope = curscope;

  auto restore_scope = [&]()
  {
    curscope = saved_scope;
  };

  auto is_integer_type = [](OType * ptype)
  {
    OType * resolved = (ptype ? ptype->ResolveAlias() : nullptr);
    return resolved && (TK_INT == resolved->kind);
  };

  auto is_positive_const_step = [&](OExpr * expr, const string & form)
  {
    OType * exprtype = (expr ? expr->ResolvedType() : nullptr);
    if (!exprtype || (TK_INT != exprtype->kind))
    {
      return true;
    }

    OValueInt value(exprtype, 0);
    if (value.CalculateConstant(expr, false) && (value.value <= 0))
    {
      Error(DQERR_FOR_STEP_POSITIVE, form);
      return false;
    }

    return true;
  };

  auto parse_optional_step = [&]() -> OExpr *
  {
    string sid;
    scf->SkipWhite();
    if (scf->ReadIdentifier(sid, false) && ("step" == sid))
    {
      scf->ReadIdentifier(sid);
      scf->SkipWhite();
      OExpr * stepexpr = ParseExpression();
      if (!stepexpr)
      {
        Error(DQERR_EXPR_EXPECTED);
      }
      return stepexpr;
    }

    return new OIntLit(1);
  };

  auto make_compare = [](OValSym * var, ECompareOp op, OExpr * right) -> OExpr *
  {
    return new OCompareExpr(op, new OLValueVar(var), right);
  };

  string loopvar_name;
  scf->SkipWhite();
  if (!scf->ReadIdentifier(loopvar_name))
  {
    StatementError(DQERR_ID_EXP_AFTER, "for");
    return;
  }

  OStmtFor * st = new OStmtFor(scpos_statement_start, saved_scope);
  curblock->AddStatement(st);

  auto abort_for = [&]()
  {
    restore_scope();
    SkipToSymbol("endfor");
  };

  OType * specified_type = nullptr;
  scf->SkipWhite();
  if (scf->CheckSymbol(":"))
  {
    specified_type = ParseTypeSpec();
    if (!specified_type)
    {
      abort_for();
      return;
    }
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol("="))
  {
    StatementError(DQERR_MISSING_ASSIGN_FOR, loopvar_name);
    abort_for();
    return;
  }

  scf->SkipWhite();
  OExpr * start_expr = ParseExpression();
  if (!start_expr)
  {
    abort_for();
    return;
  }

  OValSym * loopvar = saved_scope->FindValSym(loopvar_name);
  bool declare_loopvar = false;

  if (specified_type)
  {
    if (loopvar)
    {
      if (loopvar->ptype != specified_type)
      {
        Error(DQERR_TYPEMISM_STMT_ASSIGN, "for loop variable", specified_type->name, loopvar->ptype->name);
        OExpr::DeleteTree(start_expr);
        abort_for();
        return;
      }
    }
    else
    {
      loopvar = specified_type->CreateValSym(scpos_statement_start, loopvar_name);
      declare_loopvar = true;
    }
  }
  else if (!loopvar)
  {
    Error(DQERR_VAR_UNKNOWN, loopvar_name);
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (!is_integer_type(loopvar->ptype))
  {
    Error(DQERR_TYPE_EXPECTED, "integer", loopvar->ptype->name);
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (loopvar && (VSK_CONST == loopvar->kind))
  {
    Error(DQERR_TYPE_ASSIGN_TO_CONST, loopvar->name);
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (loopvar && !loopvar->IsRefWriteable())
  {
    Error(DQERR_REF_ASSIGN_READONLY, loopvar->name);
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (!CheckAssignType(loopvar->ptype, &start_expr, "for initializer"))
  {
    OExpr::DeleteTree(start_expr);
    abort_for();
    return;
  }

  if (declare_loopvar)
  {
    st->init->scope->DefineValSym(loopvar);
    st->init->AddStatement(new OStmtVarDecl(scpos_statement_start, loopvar, start_expr));
  }
  else
  {
    st->init->AddStatement(new OStmtAssign(scpos_statement_start, new OLValueVar(loopvar), start_expr));
    st->init->scope->SetVarInitialized(loopvar);
  }

  curscope = st->init->scope;

  string kindstr;
  scf->SkipWhite();
  if (!scf->ReadIdentifier(kindstr))
  {
    Error(DQERR_KW_OR_ID_MISSING);
    abort_for();
    return;
  }

  EForKind kind;
  if      ("to"        == kindstr)  kind = EForKind::TO;
  else if ("downto"    == kindstr)  kind = EForKind::DOWNTO;
  else if ("count"     == kindstr)  kind = EForKind::COUNT;
  else if ("downcount" == kindstr)  kind = EForKind::DOWNCOUNT;
  else if ("while"     == kindstr)  kind = EForKind::WHILE;
  else if ("in"        == kindstr)
  {
    Error(DQERR_NOT_SUPPORTED, "for ... in");
    abort_for();
    return;
  }
  else
  {
    Error(DQERR_KW_OR_ID_MISSING);
    abort_for();
    return;
  }

  OExpr * limit_expr = nullptr;
  OExpr * step_expr = nullptr;

  if (EForKind::WHILE == kind)
  {
    limit_expr = ParseExpression();
    if (!limit_expr)
    {
      StatementError(DQERR_CONDEXPR_MISSING_FOR, "for");
      abort_for();
      return;
    }

    if (TK_BOOL != limit_expr->ResolvedType()->kind)
    {
      Error(DQERR_BOOL_EXPR_EXPECTED, limit_expr->ResolvedType()->name);
    }

    st->condition = limit_expr;
    step_expr = parse_optional_step();
    if (!step_expr)
    {
      abort_for();
      return;
    }
  }
  else
  {
    limit_expr = ParseExpression();
    if (!limit_expr)
    {
      Error(DQERR_EXPR_EXPECTED);
      abort_for();
      return;
    }

    if (!CheckAssignType(loopvar->ptype, &limit_expr, "for limit"))
    {
      OExpr::DeleteTree(limit_expr);
      abort_for();
      return;
    }

    step_expr = parse_optional_step();
    if (!step_expr)
    {
      abort_for();
      return;
    }
    if (!is_positive_const_step(step_expr, kindstr))
    {
      OExpr::DeleteTree(step_expr);
      abort_for();
      return;
    }

    if (EForKind::TO == kind)
    {
      st->condition = make_compare(loopvar, COMPOP_LE, limit_expr);
    }
    else if (EForKind::DOWNTO == kind)
    {
      st->condition = make_compare(loopvar, COMPOP_GE, limit_expr);
    }
    else
    {
      OValSym * countvar = loopvar->ptype->CreateValSym(scpos_statement_start,
          format("__for_count_{}_{}", scpos_statement_start.line, scpos_statement_start.col));
      st->init->scope->DefineValSym(countvar);
      st->init->AddStatement(new OStmtVarDecl(scpos_statement_start, countvar, limit_expr));
      st->condition = make_compare(countvar, COMPOP_GT, new OIntLit(0, loopvar->ptype));
      st->step->AddStatement(new OStmtModifyAssign(scpos_statement_start, new OLValueVar(countvar),
          BINOP_SUB, new OIntLit(1, loopvar->ptype)));
    }
  }

  if (!CheckAssignType(loopvar->ptype, &step_expr, "for step"))
  {
    OExpr::DeleteTree(step_expr);
    abort_for();
    return;
  }

  EBinOp step_op = (EForKind::DOWNTO == kind || EForKind::DOWNCOUNT == kind) ? BINOP_SUB : BINOP_ADD;
  st->step->AddStatement(new OStmtModifyAssign(scpos_statement_start, new OLValueVar(loopvar), step_op, step_expr));

  ++loop_depth;
  ReadStatementBlock(st->body, "endfor");
  --loop_depth;
  st->body->scope->RevertFirstAssignments();

  restore_scope();
}

void ODqCompParser::ParseStmtIf()
{
  // note: "if" is already consumed
  // syntax form: "if <condition>: <stblock> elif <condition>: <stblock> else: <stblock> endif"
  scf->SkipWhite();

  OExpr * cond = ParseExpression();
  if (!cond)
  {
    StatementError(DQERR_CONDEXPR_MISSING_FOR, "if");
    return;
  }

  if (TK_BOOL != cond->ResolvedType()->kind)
  {
    Error(DQERR_BOOL_EXPR_EXPECTED, cond->ResolvedType()->name);
    // continue parsing the if branch for better error recovery
  }

  OStmtIf * st = new OStmtIf(scpos_statement_start, curscope);
  OIfBranch * branch = st->AddBranch(cond);
  curblock->AddStatement(st);

  while (not scf->Eof())
  {
    string endstr = "";
    ReadStatementBlock(branch->body, "endif|elif|else", &endstr);
    branch->body->scope->RevertFirstAssignments();

    if ("endif" == endstr)
    {
      break;  // if closed
    }

    if ("elif" == endstr)
    {
      cond = ParseExpression();
      if (!cond)
      {
        StatementError(DQERR_CONDEXPR_MISSING_FOR, "elif");
        break;
      }
      if (TK_BOOL != cond->ResolvedType()->kind)
      {
        Error(DQERR_BOOL_EXPR_EXPECTED, cond->ResolvedType()->name);
        // continue for better error recovery
      }

      branch = st->AddBranch(cond);
      continue;
    }

    if ("else" == endstr)
    {
      if (st->else_present)
      {
        StatementError(DQERR_MULTIPLE_ELSE);
        break;
      }
      st->else_present = true;
      branch = st->AddBranch(nullptr);
      continue;
    }

    if ("}" == endstr) // for braces mode
    {
      scf->SkipWhite();
      if (scf->CheckSymbol("endif"))
      {
        endstr = "endif";
        continue;
      }
      if (scf->CheckSymbol("elif"))
      {
        endstr = "elif";
        continue;
      }
      if (scf->CheckSymbol("else"))
      {
        endstr = "else";
        continue;
      }
    }

    break;
  }

  // re-set variable initializations when else was present and first-assigns present in all branches
  if (st->else_present)
  {
    OScope * sfirst = st->branches[0]->body->scope;
    for (OValSym * vs : sfirst->firstassign)
    {
      // check if this vs presents in all the other scopes
      bool initok = true;
      for (int bi = 1; bi < st->branches.size(); ++bi)
      {
        OScope * scp = st->branches[bi]->body->scope;
        if (not scp->FirstAssigned(vs))
        {
          initok = false;
          break;
        }
      }
      if (initok)
      {
        vs->initialized = true;
      }
    }
  }
}

OExpr * ODqCompParser::ParseExpression()
{
  OExpr * expr = ParseExprOr();
  // This is the parser-side fold for the full expression tree; later AST helpers only
  // need to fold again when they inject new conversion nodes after parsing.
  OExpr::FoldTree(&expr);
  return expr;
}

OExpr * ODqCompParser::ParseExprOr()
{
  OExpr * left = ParseExprAnd();
  if (!left) return nullptr;

  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("or"))
    {
      OExpr * right = ParseExprAnd();
      if (!right)
      {
        return FreeLeftRight(left, nullptr);
      }
      left = new OLogicalExpr(LOGIOP_OR, left, right);
      continue;
    }

    break;
  }
  return left;
}

OExpr * ODqCompParser::ParseExprAnd()
{
  OExpr * left = ParseExprNot();
  if (!left) return nullptr;

  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("and"))
    {
      OExpr * right = ParseExprNot(); // Fixed recursion to ParseExprNot (was ParseExprAnd in original logic, but usually it chains to next priority or self)
      if (!right)
      {
        return FreeLeftRight(left, nullptr);
      }
      left = new OLogicalExpr(LOGIOP_AND, left, right);
      continue;
    }

    break;
  }
  return left;
}

OExpr * ODqCompParser::ParseExprNot()
{
  scf->SkipWhite();
  if (scf->CheckSymbol("not"))
  {
    OExpr *  val = ParseExprNot();
    if (!val)
    {
      return nullptr;
    }
    return new ONotExpr(val);
  }

  return ParseComparison();
}

OExpr * ODqCompParser::ParseComparison()
{
  OExpr *  left = ParseExprAdd();
  if (!left)
  {
    return nullptr;
  }

  scf->SkipWhite();

  ECompareOp op = COMPOP_NONE;

  // check first the ambigous expression terminators
  if (scf->CheckSymbol("<<=", false) or scf->CheckSymbol(">>=", false))
  {
    return left;
  }

  if      (scf->CheckSymbol("=="))    op = COMPOP_EQ;
  else if (scf->CheckSymbol("!=") or
           scf->CheckSymbol("<>"))    op = COMPOP_NE;
  else if (scf->CheckSymbol("<="))    op = COMPOP_LE;  // <= before <
  else if (scf->CheckSymbol("<"))     op = COMPOP_LT;
  else if (scf->CheckSymbol(">="))    op = COMPOP_GE;  // >= before >
  else if (scf->CheckSymbol(">"))     op = COMPOP_GT;
  else
  {
    return left;
  }

  OExpr *  right = ParseExprAdd();
  if (!right)
  {
    return FreeLeftRight(left, nullptr);
  }

  HarmonizeNumericOperands(&left, &right);

  return new OCompareExpr(op, left, right);
}

// Table-driven binary operator parser: shared logic for all precedence levels
OExpr * ODqCompParser::ParseBinOpLevel(OExpr * (ODqCompParser::*parse_next)(), const BinOpEntry ops[], int nops)
{
  scf->SkipWhite();
  OExpr * left = (this->*parse_next)();
  if (!left) return nullptr;

  while (not scf->Eof())
  {
    scf->SkipWhite();

    // check first the ambigous expression terminators
    if (    scf->CheckSymbol("+=", false)
         or scf->CheckSymbol("-=", false)
         or scf->CheckSymbol("*=", false)
         or scf->CheckSymbol("/=", false)
         or scf->CheckSymbol("<<=", false)
         or scf->CheckSymbol(">>=", false)  )
    {
      break;
    }

    EBinOp op = BINOP_NONE;
    bool blocked_assignop = false;
    for (int i = 0; i < nops; ++i)
    {
      if (scf->CheckSymbol(ops[i].sym))
      {
        op = ops[i].op;
        break;
      }
    }
    if (op == BINOP_NONE)  break;

    OExpr * right = (this->*parse_next)();
    OExpr * res = CreateBinExpr(op, left, right);
    if (!res) return FreeLeftRight(left, right);
    left = res;
  }
  return left;
}

OExpr * ODqCompParser::ParseExprAdd()
{
  static const BinOpEntry ops[] = { {"+", BINOP_ADD}, {"-", BINOP_SUB} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprMul, ops, 2);
}

OExpr * ODqCompParser::ParseExprMul()
{
  static const BinOpEntry ops[] = { {"*", BINOP_MUL} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprDiv, ops, 1);
}

OExpr * ODqCompParser::ParseExprDiv()
{
  static const BinOpEntry ops[] = { {"/", BINOP_DIV}, {"IDIV", BINOP_IDIV}, {"IMOD", BINOP_IMOD} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprBinOr, ops, 3);
}

OExpr * ODqCompParser::ParseExprBinOr()
{
  static const BinOpEntry ops[] = { {"OR", BINOP_IOR}, {"XOR", BINOP_IXOR} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprBinAnd, ops, 2);
}

OExpr * ODqCompParser::ParseExprBinAnd()
{
  static const BinOpEntry ops[] = { {"AND", BINOP_IAND} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprShift, ops, 1);
}

OExpr * ODqCompParser::ParseExprShift()
{
  static const BinOpEntry ops[] = { {"<<", BINOP_ISHL}, {"SHL", BINOP_ISHL}, {">>", BINOP_ISHR}, {"SHR", BINOP_ISHR} };
  return ParseBinOpLevel(&ODqCompParser::ParseExprUnary, ops, 4);
}

OExpr * ODqCompParser::ParseExprUnary()
{
  scf->SkipWhite();

  // address-of operator: consume a full postfix-capable lvalue operand
  if (scf->CheckSymbol("&"))
  {
    OLValueExpr * lval = ParseAddressableExpr();
    if (!lval) return nullptr;
    return new OAddrOfExpr(lval);
  }

  if (scf->CheckSymbol("-"))
  {
    OExpr * val = ParseExprUnary();
    if (!val) return nullptr;
    return new ONegExpr(val);
  }

  if (scf->CheckSymbol("NOT"))
  {
    OExpr * val = ParseExprUnary();
    if (!val) return nullptr;
    return new OBinNotExpr(val);
  }

  return ParseExprPostfix();
}

OLValueExpr * ODqCompParser::ParseAddressableExpr()
{
  OExpr * expr = ParseExprPostfix();
  if (!expr) return nullptr;

  OLValueExpr * lval = dynamic_cast<OLValueExpr *>(expr);
  if (!lval)
  {
    Error(DQERR_EXPR_INVALID_ADDROF);  // Address-of requires an lvalue expression;
    delete expr;
    return nullptr;
  }

  OValSym * varref = dynamic_cast<OLValueVar *>(lval) ? static_cast<OLValueVar *>(lval)->pvalsym : nullptr;
  if (varref and VSK_VARIABLE != varref->kind and VSK_PARAMETER != varref->kind)
  {
    Error(DQERR_EXPR_VS_NOT_ADDRESSABLE, varref->name);
    delete expr;
    return nullptr;
  }

  return lval;
}

OExpr * ODqCompParser::ParseExprPostfix()
{
  OExpr * result = ParseExprPrimary();
  if (!result) return nullptr;
  return ParsePostfix(result);
}

OExpr * ODqCompParser::ParseExplicitCastExpr(bool * rattempted)
{
  if (rattempted)
  {
    *rattempted = false;
  }

  OScPosition saved_pos;
  scf->SaveCurPos(saved_pos);

  OType * dsttype = ParseTypeSpec(false);
  if (!dsttype)
  {
    scf->SetCurPos(saved_pos);
    return nullptr;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    scf->SetCurPos(saved_pos);
    return nullptr;
  }

  if (rattempted)
  {
    *rattempted = true;
  }

  OExpr * srcexpr = ParseExpression();
  if (!srcexpr)
  {
    return nullptr;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol(")"))
  {
    delete srcexpr;
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "cast");
    return nullptr;
  }

  if (!ConvertExprToType(dsttype, &srcexpr, EXPCF_GENERATE_ERRORS | EXPCF_EXPLICIT_CAST))
  {
    delete srcexpr;
    return nullptr;
  }

  return srcexpr;
}

OExpr * ODqCompParser::ParsePostfix(OExpr * base)
{
  OExpr * result = base;
  if (!result) return nullptr;

  while (true)
  {
    scf->SkipWhite();

    if (!result->ptype)
    {
      break;  // void function call: no postfix operations are possible
    }

    ETypeKind      tk   = result->ptype->kind;
    OLValueExpr *  lval = dynamic_cast<OLValueExpr *>(result);

    if (lval)
    {
      // Struct member access on a compound lvalue or a ^compound pointer: x.field / p.field
      if (scf->CheckSymbol("."))
      {
        OLValueExpr * memberbase = nullptr;
        OCompoundType * ctype = nullptr;
        if (!ResolveCompoundMemberBase(lval, lval->ptype, memberbase, ctype))
        {
          OTypePointer * ptrtype = dynamic_cast<OTypePointer *>(lval->ResolvedType());
          if (ptrtype && ptrtype->IsOpaquePointer())
          {
            Error(DQERR_PTR_OPAQUE_USAGE, "member access");
            delete result;
            return nullptr;
          }
          else
          {
            Error(DQERR_TYPE_NO_MEMBERS);
          }
          return result;
        }

        string membername;
        scf->SkipWhite();
        if (not scf->ReadIdentifier(membername))
        {
          Error(DQERR_MEMBER_NAME_EXPECTED);
          return result;
        }

        auto * object_type = dynamic_cast<OTypeObject *>(ctype);
        OCompoundType * decl_type = ctype;
        OValSym * objsym = (object_type ? object_type->FindObjectMemberSymbol(membername, &decl_type)
                                        : ctype->Members()->FindValSym(membername, nullptr, false));
        if (auto * method = dynamic_cast<OValSymFunc *>(objsym))
        {
          if (!ObjectMemberAccessAllowed(decl_type, method))
          {
            Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
            delete result;
            return nullptr;
          }
          if (!scf->CheckSymbol("("))
          {
            Error(DQERR_FUNC_CALL_PARENTH, membername);
            return result;
          }

          OExpr * callexpr = ParseExprMethodCall(method, memberbase);
          result = callexpr;
          if (!result) return nullptr;
          continue;
        }
        if (auto * ovset = dynamic_cast<OValSymOverloadSet *>(objsym))
        {
          if (!ObjectMemberAccessAllowed(decl_type, ovset))
          {
            Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
            delete result;
            return nullptr;
          }
          if (!scf->CheckSymbol("("))
          {
            Error(DQERR_FUNC_CALL_PARENTH, membername);
            return result;
          }

          OExpr * callexpr = ParseExprMethodOverloadCall(ovset, memberbase);
          result = callexpr;
          if (!result) return nullptr;
          continue;
        }

        decl_type = ctype;
        int midx = (object_type ? object_type->FindObjectFieldIndex(membername, &decl_type)
                                : ctype->FindMemberIndex(membername));
        if (midx < 0)
        {
          Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
          return result;
        }
        OType * mtype = decl_type->member_order[midx]->ptype;
        if (!ObjectMemberAccessAllowed(decl_type, decl_type->member_order[midx]))
        {
          Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
          delete result;
          return nullptr;
        }
        result = new OLValueMember(memberbase, decl_type, midx, mtype);
        continue;
      }

      // Array/slice/cstring index on any lvalue: x[i]
      if ((TK_ARRAY == tk or TK_ARRAY_SLICE == tk or TK_STRING == tk)
          and scf->CheckSymbol("["))
      {
        OExpr * indexexpr = ParseExpression();
        scf->SkipWhite();
        if (not scf->CheckSymbol("]"))
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_AFTER, "index");
        }
        result = new OLValueIndex(lval, lval->ptype, indexexpr);
        continue;
      }

      // Function call: f(args)
      OLValueVar * varref = dynamic_cast<OLValueVar *>(lval);
      if (varref)
      {
        if (dynamic_cast<OValSymOverloadSet *>(varref->pvalsym) && scf->CheckSymbol("("))
        {
          OExpr * callexpr = ParseExprOverloadCall(static_cast<OValSymOverloadSet *>(varref->pvalsym));
          delete result;
          result = callexpr;
          if (!result) return nullptr;
          continue;
        }

        OValSymFunc * vsfunc = dynamic_cast<OValSymFunc *>(varref->pvalsym);
        if (vsfunc && scf->CheckSymbol("("))
        {
          OExpr * callexpr = ParseExprFuncCall(vsfunc);
          delete result;
          result = callexpr;
          if (!result) return nullptr;
          continue;
        }
      }
    }

    if (scf->CheckSymbol("(", false))
    {
      if (TK_FUNCREF == tk)
      {
        scf->CheckSymbol("(");
        OTypeFuncRef * calltype = static_cast<OTypeFuncRef *>(result->ResolvedType());
        OExpr * callexpr = ParseExprIndirectCall(result, calltype);
        result = callexpr;
        if (!result) return nullptr;
        continue;
      }

      Error(DQERR_EXPR_NOT_CALLABLE, result->ptype->name);
      delete result;
      return nullptr;
    }

    // pointer operations — apply to any expression (not just lvalue)
    if (TK_POINTER == tk)
    {
      OTypePointer * ptrtype = static_cast<OTypePointer *>(result->ResolvedType());
      if (scf->CheckSymbol("[")) // p[i]: pointer indexing, no dereference
      {
        if (!ptrtype->IsTypedPointer())
        {
          Error(DQERR_PTR_OPAQUE_USAGE, "pointer indexing");
          delete result;
          return nullptr;
        }

        OExpr * indexexpr = ParseExpression();
        scf->SkipWhite();
        if (not scf->CheckSymbol("]"))
        {
          Error(DQERR_MISSING_CLOSE_BRACKET_AFTER, "pointer index");
        }
        result = new OPointerIndexExpr(result, indexexpr);
        continue;
      }

      if (scf->CheckSymbol("^")) // p^: dereference -> lvalue
      {
        if (!ptrtype->IsTypedPointer())
        {
          Error(DQERR_PTR_OPAQUE_USAGE, "dereference");
          delete result;
          return nullptr;
        }
        result = new OLValueDeref(result);
        continue;
      }
    }

    break;
  }

  return result;
}

OExpr * ODqCompParser::ParseExprPrimary()
{
  OExpr * result = nullptr;

  scf->SkipWhite();

  if (*scf->curp == '^'
      || *scf->curp == '['
      || ((*scf->curp >= 'A' && *scf->curp <= 'Z')
          || (*scf->curp >= 'a' && *scf->curp <= 'z')
          || (*scf->curp == '_')))
  {
    bool attempted_cast = false;
    result = ParseExplicitCastExpr(&attempted_cast);
    if (result)
    {
      return result;
    }
    if (attempted_cast)
    {
      return nullptr;
    }
  }

  if (scf->CheckSymbol("("))
  {
    result = ParseExpression();
    scf->SkipWhite();
    if (!scf->CheckSymbol(")"))
    {
      Error(DQERR_MISSING_CLOSE_PAREN);
    }
    return result;
  }

  if (scf->CheckSymbol("["))
  {
    return ParseArrayLit();
  }

  if (scf->CheckSymbol("0x"))  // hex number ?
  {
    uint64_t  hexval;
    if (scf->ReadHex64Value(hexval))
    {
      result = new OIntLit(int64_t(hexval));
    }
    else
    {
      ErrorTxt(DQERR_LIT_HEXNUM, "hexadecimal numbers expected after \"0x\"");
    }
    return result;
  }

  if (scf->IsNumChar())  // '0' .. '9' ?
  {
    int64_t  intval;
    if (scf->ReadInt64Value(intval))
    {
      // check for floating point: 0.123, 2.1e-5, 1.234E6, 0.
      char c = *scf->curp;
      if (('.' == c) or ('e' == c) or ('E' == c)) // convert to floating point
      {
        double fpval = intval;
        if (not scf->ReadFloatFracExp(fpval))
        {
          Error(DQERR_LIT_FLOAT);
        }
        result = new OFloatLit(fpval);
      }
      else
      {
        result = new OIntLit(intval);
      }
    }
    else  // impossible case
    {
      Error(DQERR_LIT_INT);
    }
    return result;
  }

  // String literal: "..." or '...'
  if (*scf->curp == '"' or *scf->curp == '\'')
  {
    string strval;
    if (scf->ReadQuotedString(strval))
    {
      return new OCStringLit(strval);
    }
    else
    {
      ErrorTxt(DQERR_LIT_STRING, "Unterminated string literal");
      return nullptr;
    }
  }

  if (scf->CheckSymbol("true"))
  {
    result = new OBoolLit(true);
    return result;
  }

  if (scf->CheckSymbol("false"))
  {
    result = new OBoolLit(false);
    return result;
  }

  if (scf->CheckSymbol("null"))
  {
    result = new ONullLit();
    return result;
  }

  if (scf->CheckSymbol("@"))
  {
    OScPosition scpos_ns = scf->prevpos;
    OValSym * vs = ResolveNamespaceValSym();
    if (!vs)
    {
      return nullptr;
    }

    result = new OLValueVar(vs);
    if (vs->kind != VSK_FUNCTION and not vs->initialized)
    {
      if (vs->IsRefLike() && (FPM_REFOUT == vs->param_mode))
      {
        if (supress_varinit_check)
        {
          AddSuppressedVarInitDiag(static_cast<OLValueVar *>(result), vs, scpos_ns);
        }
        else
        {
          Error(DQERR_REFOUT_READ_BEFORE_WRITE, vs->name, &scpos_ns);
        }
      }
      else
      {
        VarInitError(static_cast<OLValueVar *>(result), vs, scpos_ns);
      }
    }
    return result;
  }

  // identifier

  OScPosition scpos_sid;
  scf->SaveCurPos(scpos_sid);

  string  sid;
  if (!scf->ReadIdentifier(sid))
  {
    if (!scf->Eof())
    {
      Error(DQERR_EXPR_UNEXPECTED_CHAR, string(1, *scf->curp));
    }
    else
    {
      Error(DQERR_EXPR_EXPECTED);
    }
    return result;
  }

  // builtin specials

  if ("len" == sid)
  {
    return ParseBuiltinLen();
  }

  if ("iif" == sid)
  {
    return ParseBuiltinIif();
  }

  if ("sizeof" == sid)
  {
    return ParseBuiltinSizeof();
  }

  if ("offsetof" == sid)
  {
    return ParseBuiltinOffsetof();
  }

  if ("round" == sid)  return ParseBuiltinFloatRound(RNDMODE_ROUND);
  if ("ceil"  == sid)  return ParseBuiltinFloatRound(RNDMODE_CEIL);
  if ("floor" == sid)  return ParseBuiltinFloatRound(RNDMODE_FLOOR);

  if ("new" == sid)
  {
    return ParseNewExpr();
  }

  if ("inherited" == sid)
  {
    return ParseInheritedExpr();
  }

  OScope * found_scope = nullptr;
  OValSym * vs = curscope->FindValSym(sid, &found_scope);
  if (!vs)
  {
    bool object_method_scope = (curvsfunc && curvsfunc->owner_compound_type && curvsfunc->receiver_arg);
    if (object_method_scope)
    {
      OCompoundType * decl_type = nullptr;
      auto * owner_object = dynamic_cast<OTypeObject *>(curvsfunc->owner_compound_type);
      OValSym * member = (owner_object ? owner_object->FindObjectMemberSymbol(sid, &decl_type) : nullptr);
      if (member && (VSK_FUNCTION != member->kind))
      {
        if (!ObjectMemberAccessAllowed(decl_type, member))
        {
          Error(DQERR_MEMBER_UNKNOWN, sid, owner_object->name);
          return result;
        }
        int midx = decl_type->FindMemberIndex(sid);
        if (midx >= 0)
        {
          return new OLValueMember(new OLValueVar(curvsfunc->receiver_arg), decl_type, midx, member->ptype);
        }
      }

      auto nsit = g_namespaces.find(".");
      OScope * root_scope = (g_namespaces.end() != nsit ? nsit->second : nullptr);
      if (root_scope && root_scope->FindValSym(sid, nullptr, true))
      {
        Error(DQERR_OBJ_VS_IN_MODULE_NS, sid);

        //ErrorTxt(DQERR_VS_UNKNOWN,
        //         format("Unknown symbol \"{}\" in object method scope; module-root symbol exists, use \"@.{}\" to access it",
        //                sid, sid));
        return result;
      }
    }
    Error(DQERR_VS_UNKNOWN, sid);
    return result;
  }

  result = CreateImplicitObjectMemberExpr(sid, vs, found_scope);
  if (!result)
  {
    result = new OLValueVar(vs);
  }
  if (vs->kind != VSK_FUNCTION and not vs->initialized)
  {
    if (vs->IsRefLike() && (FPM_REFOUT == vs->param_mode))
    {
      if (supress_varinit_check)
      {
        AddSuppressedVarInitDiag(static_cast<OLValueVar *>(result), vs, scpos_sid);
      }
      else
      {
        Error(DQERR_REFOUT_READ_BEFORE_WRITE, vs->name, &scpos_sid);
      }
    }
    else
    {
      VarInitError(static_cast<OLValueVar *>(result), vs, scpos_sid);
    }
  }

  return result;
}

OValSym * ODqCompParser::ResolveNamespaceValSym()
{
  string nsname;
  string symname;

  if (scf->CheckSymbol("."))
  {
    nsname = ".";
  }
  else if (!scf->ReadIdentifier(nsname))
  {
    Error(DQERR_NS_NAME_EXPECTED);
    return nullptr;
  }

  if ("." != nsname && !scf->CheckSymbol("."))
  {
    Error(DQERR_DOT_MISSING_AFTER_NS_NAME);
    return nullptr;
  }

  if (!scf->ReadIdentifier(symname))
  {
    Error(DQERR_ID_EXP_AFTER, "@"+nsname);
    return nullptr;
  }

  auto it = g_namespaces.find(nsname);
  if (it == g_namespaces.end())
  {
    Error(DQERR_NS_UNKNOWN, "@"+nsname);
    return nullptr;
  }

  OValSym * vs = it->second->FindValSym(symname, nullptr, true);
  if (!vs)
  {
    Error(DQERR_VS_UNKNOWN_IN_NAMESPACE, symname, "@"+nsname);
    return nullptr;
  }

  return vs;
}

OExpr * ODqCompParser::ParseArrayLit()
{
  // "[" is already consumed
  vector<OExpr *> elems;

  while (not scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol("]"))
    {
      break;
    }

    if (elems.size() > 0)
    {
      if (not scf->CheckSymbol(","))
      {
        Error(DQERR_MISSING_COMMA_IN, "array literal");
      }
    }

    OExpr * val = ParseExpression();
    if (val)
    {
      elems.push_back(val);
    }
  }

  return new OArrayLit(elems);
}

bool ODqCompParser::ParseCallArguments(const string & callname, OTypeFunc * tfunc, vector<OExpr *> & rargs)
{
  vector<TRawCallArg> rawargs;
  if (!ParseRawCallArguments(callname, rawargs))
  {
    return false;
  }

  bool result = BindCallArguments(callname, tfunc, rawargs, rargs);
  FreeRawCallArguments(rawargs);
  return result;
}

bool ODqCompParser::ParseRawCallArguments(const string & callname, vector<TRawCallArg> & rargs)
{
  // "(" was already consumed

  int pcnt = 0;
  while (true)
  {
    scf->SkipWhite();
    if (scf->CheckSymbol(")"))
    {
      break;
    }

    if ((pcnt > 0) and not scf->CheckSymbol(","))
    {
      Error(DQERR_FUNC_ARGS_LIST, "\",\" or \")\" is missing at function \"$1\"call arguments", callname);
      FreeRawCallArguments(rargs);
      return false;
    }

    TRawCallArg rawarg;
    scf->SaveCurPos(rawarg.scpos_start);

    size_t suppressed_start = suppressed_varinit_diags.size();
    bool saved_suppress = supress_varinit_check;
    supress_varinit_check = true;
    rawarg.expr = ParseExpression();
    supress_varinit_check = saved_suppress;

    rawarg.init_diags.assign(suppressed_varinit_diags.begin() + suppressed_start, suppressed_varinit_diags.end());
    suppressed_varinit_diags.resize(suppressed_start);

    if (!rawarg.expr)
    {
      FreeRawCallArguments(rargs);
      return false;
    }

    rargs.push_back(rawarg);
    ++pcnt;
  }

  return true;
}

void ODqCompParser::FreeRawCallArguments(vector<TRawCallArg> & rawargs)
{
  for (TRawCallArg & rawarg : rawargs)
  {
    OExpr::DeleteTree(rawarg.expr);
    rawarg.expr = nullptr;
    rawarg.init_diags.clear();
  }

  rawargs.clear();
}

void ODqCompParser::EmitStoredVarInitDiags(const vector<TSuppressedVarInitDiag> & diags)
{
  for (const auto & diag : diags)
  {
    OScPosition scpos = diag.scpos;
    if (diag.valsym->IsRefLike() && (FPM_REFOUT == diag.valsym->param_mode))
    {
      Error(DQERR_REFOUT_READ_BEFORE_WRITE, diag.valsym->name, &scpos);
    }
    else
    {
      Error(DQERR_VAR_NOT_INITIALIZED, diag.valsym->name, &scpos);
    }
  }
}

OExpr * ODqCompParser::CreateImplicitObjectMemberExpr(const string & sid, OValSym * vs, OScope * found_scope)
{
  if (!curvsfunc || !curvsfunc->owner_compound_type || !curvsfunc->receiver_arg || !vs || !found_scope)
  {
    return nullptr;
  }

  auto * object_type = dynamic_cast<OTypeObject *>(curvsfunc->owner_compound_type);
  if (!object_type)
  {
    return nullptr;
  }
  if (VSK_FUNCTION == vs->kind)
  {
    return nullptr;
  }

  OCompoundType * decl_type = nullptr;
  for (OTypeObject * cur = object_type; cur; cur = cur->base_type)
  {
    if (found_scope == cur->Members())
    {
      decl_type = cur;
      break;
    }
  }
  if (!decl_type)
  {
    return nullptr;
  }

  int midx = decl_type->FindMemberIndex(sid);
  if (midx < 0)
  {
    return nullptr;
  }
  if (!ObjectMemberAccessAllowed(decl_type, vs))
  {
    Error(DQERR_MEMBER_UNKNOWN, sid, object_type->name);
    return nullptr;
  }

  return new OLValueMember(new OLValueVar(curvsfunc->receiver_arg), decl_type, midx, vs->ptype);
}

bool ODqCompParser::ObjectMemberAccessAllowed(OCompoundType * decl_type, OValSym * member) const
{
  if (!member || MV_PUBLIC == member->member_visibility)
  {
    return true;
  }

  OCompoundType * curtype = (curvsfunc ? curvsfunc->owner_compound_type : nullptr);
  if (!curtype || !decl_type)
  {
    return false;
  }

  if (MV_PRIVATE == member->member_visibility)
  {
    return curtype == decl_type;
  }

  return curtype->IsSameOrDerivedFrom(decl_type);
}

bool ODqCompParser::BindCallArguments(const string & callname, OTypeFunc * tfunc, vector<TRawCallArg> & rawargs, vector<OExpr *> & rargs)
{

  if (!tfunc)
  {
    Error(DQERR_EXPR_NOT_CALLABLE, callname);
    return false;
  }

  bool        bok = true;
  size_t      required_param_count = tfunc->RequiredParamCount();

  for (size_t pcnt = 0; pcnt < rawargs.size(); ++pcnt)
  {
    if (pcnt >= tfunc->params.size() && !tfunc->has_varargs)
    {
      Error(DQERR_FUNC_ARGS_TOO_MANY, callname, to_string(tfunc->params.size()));
      bok = false;
      break;
    }

    TRawCallArg & rawarg = rawargs[pcnt];
    OExpr * argexpr = rawarg.expr;
    rawarg.expr = nullptr;

    OFuncParam * fparam = ((pcnt < tfunc->params.size()) ? tfunc->params[pcnt] : nullptr);
    bool is_ref_arg = (fparam && fparam->IsRefLike());

    if (!argexpr)
    {
      bok = false;
      break;
    }

    if (!is_ref_arg)
    {
      if (!rawarg.init_diags.empty())
      {
        EmitStoredVarInitDiags(rawarg.init_diags);
        OExpr::DeleteTree(argexpr);
        bok = false;
        break;
      }

      rargs.push_back(argexpr);
      if (pcnt < tfunc->params.size())
      {
        OType * argtype = tfunc->params[pcnt]->ptype;
        if (not CheckAssignType(argtype, &argexpr, "Argument"))
        {
          bok = false;
          break;
        }
        // CheckAssignType may have replaced argexpr (e.g. array->slice conversion)
        rargs[pcnt] = argexpr;
      }
    }
    else
    {
      bool is_null_arg = dynamic_cast<ONullLit *>(argexpr);
      if (is_null_arg)
      {
        if (FPM_REFNULL != fparam->mode)
        {
          Error(DQERR_FUNC_ARG_REF_NULL, to_string(pcnt + 1), callname);
          OExpr::DeleteTree(argexpr);
          bok = false;
          break;
        }

        rargs.push_back(argexpr);
      }
      else
      {
        OLValueExpr * arglval = dynamic_cast<OLValueExpr *>(argexpr);
        OValSym * rootvalsym = (arglval ? GetAssignRootValSym(arglval) : nullptr);
        bool bind_ok = (arglval != nullptr);
        if (bind_ok && rootvalsym)
        {
          if ((VSK_CONST == rootvalsym->kind) || !rootvalsym->IsRefWriteable())
          {
            bind_ok = false;
          }
        }

        if (!bind_ok)
        {
          Error(DQERR_FUNC_ARG_REF_BIND, to_string(pcnt + 1), callname);
          OExpr::DeleteTree(argexpr);
          bok = false;
          break;
        }

        if (!OTypeFunc::SameRefBindingType(fparam->ptype, argexpr->ptype))
        {
          string type_text = format("{} = {}", fparam->ptype->name, argexpr->ptype->name);
          ErrorTxt(DQERR_FUNC_ARG_REF_TYPE,
                   format("Reference argument {} type mismatch for function \"{}\": {}", pcnt + 1, callname, type_text));
          OExpr::DeleteTree(argexpr);
          bok = false;
          break;
        }

        if (!rawarg.init_diags.empty() && ((FPM_REF == fparam->mode) || (FPM_REFIN == fparam->mode) || (FPM_REFNULL == fparam->mode)))
        {
          OValSym * uninitvs = rawarg.init_diags[0].valsym;
          Error(DQERR_FUNC_ARG_REF_UNINIT, uninitvs->name);
          OExpr::DeleteTree(argexpr);
          bok = false;
          break;
        }

        OTypeObject * ref_object_type = dynamic_cast<OTypeObject *>(fparam->ptype ? fparam->ptype->ResolveAlias() : nullptr);
        if (ref_object_type)
        {
          rargs.push_back(new OObjectAddrExpr(arglval));
        }
        else
        {
          rargs.push_back(new OAddrOfExpr(arglval));
        }

        if ((FPM_REFOUT == fparam->mode) && curblock && rootvalsym
            && (VSK_VARIABLE == rootvalsym->kind || VSK_PARAMETER == rootvalsym->kind))
        {
          curblock->scope->SetVarInitialized(rootvalsym);
        }
      }
    }
  }

  if (bok && (rawargs.size() < required_param_count))
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(rawargs.size()), callname, to_string(required_param_count));
    bok = false;
  }

  while (bok && (rargs.size() < tfunc->params.size()))
  {
    OFuncParam * fparam = tfunc->params[rargs.size()];
    if (!fparam->defvalue)
    {
      Error(DQERR_FUNC_ARGS_TOO_FEW, to_string(rawargs.size()), callname, to_string(required_param_count));
      bok = false;
      break;
    }

    rargs.push_back(new OLValueVar(fparam->defvalue));
  }

  if (!bok)
  {
    for (OExpr *& arg : rargs)
    {
      OExpr::DeleteTree(arg);
      arg = nullptr;
    }
    rargs.clear();
    return false;
  }

  return true;
}

OExpr * ODqCompParser::ParseExprFuncCall(OValSymFunc * vsfunc)
{
  if (vsfunc && vsfunc->owner_compound_type)
  {
    return ParseExprMethodCall(vsfunc, nullptr);
  }

  OCallExpr * result = new OCallExpr(vsfunc);
  if (!ParseCallArguments(vsfunc->name, static_cast<OTypeFunc *>(vsfunc->ptype), result->args))
  {
    delete result;
    return nullptr;
  }

  return result;
}

OExpr * ODqCompParser::ParseExprMethodCall(OValSymFunc * vsfunc, OLValueExpr * receiver)
{
  if (!vsfunc || !vsfunc->owner_compound_type)
  {
    Error(DQERR_EXPR_NOT_CALLABLE, "method");
    OExpr::DeleteTree(receiver);
    return nullptr;
  }
  if ( vsfunc->attr_is_virtual && curvsfunc &&
       ((OSF_CREATE == curvsfunc->object_specfunc_kind) or (OSF_DESTROY == curvsfunc->object_specfunc_kind)) )
  {
    ErrorTxt(DQERR_VIRT_FUNC_CALL_INVALID, vsfunc->name, "constructors or destructors");
    OExpr::DeleteTree(receiver);
    return nullptr;
  }

  vector<TRawCallArg> rawargs;

  TRawCallArg thisarg;
  scf->SaveCurPos(thisarg.scpos_start);
  if (receiver)
  {
    thisarg.expr = receiver;
  }
  else if (curvsfunc && curvsfunc->owner_compound_type == vsfunc->owner_compound_type && curvsfunc->receiver_arg)
  {
    thisarg.expr = new OLValueVar(curvsfunc->receiver_arg);
  }
  else
  {
    Error(DQERR_EXPR_NOT_CALLABLE, vsfunc->name);
    return nullptr;
  }
  rawargs.push_back(thisarg);

  if (!ParseRawCallArguments(vsfunc->name, rawargs))
  {
    return nullptr;
  }

  OCallExpr * result = new OCallExpr(vsfunc);
  if (!BindCallArguments(vsfunc->name, static_cast<OTypeFunc *>(vsfunc->ptype), rawargs, result->args))
  {
    delete result;
    FreeRawCallArguments(rawargs);
    return nullptr;
  }

  FreeRawCallArguments(rawargs);
  return result;
}

OExpr * ODqCompParser::ParseExprOverloadCall(OValSymOverloadSet * ovset)
{
  if (ovset && ovset->owner_compound_type)
  {
    return ParseExprMethodOverloadCall(ovset, nullptr);
  }

  vector<TRawCallArg> rawargs;
  return ParseExprOverloadCallWithRawArgs(ovset, rawargs);
}

OExpr * ODqCompParser::ParseExprMethodOverloadCall(OValSymOverloadSet * ovset, OLValueExpr * receiver)
{
  if (!ovset || !ovset->owner_compound_type)
  {
    Error(DQERR_EXPR_NOT_CALLABLE, "method");
    OExpr::DeleteTree(receiver);
    return nullptr;
  }

  vector<TRawCallArg> rawargs;

  TRawCallArg thisarg;
  scf->SaveCurPos(thisarg.scpos_start);
  if (receiver)
  {
    thisarg.expr = receiver;
  }
  else if (curvsfunc && curvsfunc->owner_compound_type == ovset->owner_compound_type && curvsfunc->receiver_arg)
  {
    thisarg.expr = new OLValueVar(curvsfunc->receiver_arg);
  }
  else
  {
    Error(DQERR_EXPR_NOT_CALLABLE, ovset->name);
    return nullptr;
  }
  rawargs.push_back(thisarg);

  return ParseExprOverloadCallWithRawArgs(ovset, rawargs);
}

OExpr * ODqCompParser::ParseExprOverloadCallWithRawArgs(OValSymOverloadSet * ovset, vector<TRawCallArg> & rawargs)
{
  if (!ovset)
  {
    Error(DQERR_EXPR_NOT_CALLABLE, "function");
    return nullptr;
  }

  if (!ParseRawCallArguments(ovset->name, rawargs))
  {
    return nullptr;
  }

  vector<TFuncCallArgMatch> callargs;
  callargs.reserve(rawargs.size());
  for (const TRawCallArg & rawarg : rawargs)
  {
    callargs.push_back({rawarg.expr, !rawarg.init_diags.empty()});
  }

  OValSymFunc * best_func = nullptr;
  TFuncCallMatchScore best_score;
  bool ambiguous = false;

  for (OValSymFunc * fn : ovset->funcs)
  {
    OTypeFunc * tfunc = dynamic_cast<OTypeFunc *>(fn ? fn->ptype : nullptr);
    TFuncCallMatchScore score;
    if (!tfunc || !tfunc->AnalyzeCallCandidate(callargs, score))
    {
      continue;
    }

    if (!best_func)
    {
      best_func = fn;
      best_score = score;
      ambiguous = false;
      continue;
    }

    int cmp = OTypeFunc::CompareCallCandidateScore(score, best_score);
    if (cmp < 0)
    {
      best_func = fn;
      best_score = score;
      ambiguous = false;
    }
    else if (0 == cmp)
    {
      ambiguous = true;
    }
  }

  if (!best_func || ambiguous)
  {
    FreeRawCallArguments(rawargs);
    if (ambiguous)
    {
      Error(DQERR_OVERLOAD_AMBIGUOUS, ovset->name);
    }
    else
    {
      Error(DQERR_OVERLOAD_NO_MATCH, ovset->name);
    }
    return nullptr;
  }

  OCallExpr * result = new OCallExpr(best_func);
  if (best_func->attr_is_virtual && curvsfunc &&
      ((OSF_CREATE == curvsfunc->object_specfunc_kind) or (OSF_DESTROY == curvsfunc->object_specfunc_kind)) )
  {
    delete result;
    FreeRawCallArguments(rawargs);
    ErrorTxt(DQERR_VIRT_FUNC_CALL_INVALID, best_func->name, "constructors or destructors");
    return nullptr;
  }
  if (!BindCallArguments(ovset->name, static_cast<OTypeFunc *>(best_func->ptype), rawargs, result->args))
  {
    delete result;
    FreeRawCallArguments(rawargs);
    return nullptr;
  }

  FreeRawCallArguments(rawargs);
  return result;
}

OExpr * ODqCompParser::ParseExprIndirectCall(OExpr * callee, OTypeFuncRef * calltype)
{
  OIndirectCallExpr * result = new OIndirectCallExpr(callee, calltype);
  string callname = (calltype ? calltype->name : string("funcref"));
  if (!ParseCallArguments(callname, (calltype ? calltype->functype : nullptr), result->args))
  {
    delete result;
    return nullptr;
  }

  return result;
}

OExpr * ODqCompParser::ParseNewExpr()
{
  OValSymFunc * memalloc_func = dynamic_cast<OValSymFunc *>(curscope->FindValSym("MemAlloc"));
  if (!memalloc_func)
  {
    Error(DQERR_VS_UNKNOWN, "MemAlloc");
    return nullptr;
  }

  OType * alloc_type = ParseTypeSpec();
  if (!alloc_type)
  {
    return nullptr;
  }
  alloc_type = alloc_type->ResolveAlias();
  alloc_type->EnsureLayout();

  if (TK_VOID == alloc_type->kind)
  {
    Error(DQERR_TYPE_EXPECTED, "non-void", alloc_type->name);
    return nullptr;
  }
  if (auto * object_type = dynamic_cast<OTypeObject *>(alloc_type))
  {
    if (object_type->is_abstract)
    {
      ErrorTxt(DQERR_NOT_SUPPORTED, format("constructing abstract object \"{}\"", object_type->name));
      return nullptr;
    }
    vector<OExpr *> ctor_args;
    scf->SkipWhite();
    bool has_parens = scf->CheckSymbol("(");
    if (has_parens)
    {
      vector<TRawCallArg> rawargs;
      if (!ParseRawCallArguments(alloc_type->name, rawargs))
      {
        return nullptr;
      }
      for (TRawCallArg & rawarg : rawargs)
      {
        ctor_args.push_back(rawarg.expr);
        rawarg.expr = nullptr;
      }
      FreeRawCallArguments(rawargs);
    }

    OValSymFunc * ctor = nullptr;
    if (has_parens && !CheckObjectCtorArgs(object_type, ctor_args, ctor))
    {
      for (OExpr * arg : ctor_args) OExpr::DeleteTree(arg);
      return nullptr;
    }

    ONewExpr * result = new ONewExpr(alloc_type, nullptr, memalloc_func);
    result->ctor_func = ctor;
    result->ctor_args = ctor_args;
    return result;
  }
  if (0 == alloc_type->bytesize)
  {
    Error(DQERR_NOT_SUPPORTED, "new for dynamically sized type");
    return nullptr;
  }

  OExpr * initexpr = nullptr;
  scf->SkipWhite();
  if (scf->CheckSymbol("("))
  {
    scf->SkipWhite();
    if (!scf->CheckSymbol(")"))
    {
      initexpr = ParseExpression();
      if (!initexpr)
      {
        return nullptr;
      }

      scf->SkipWhite();
      if (scf->CheckSymbol(","))
      {
        delete initexpr;
        Error(DQERR_FUNC_ARGS_TOO_MANY, "new", "1");
        scf->ReadTo(")");
        scf->CheckSymbol(")");
        return nullptr;
      }

      if (!scf->CheckSymbol(")"))
      {
        delete initexpr;
        Error(DQERR_MISSING_CLOSE_PAREN_FOR, "new");
        return nullptr;
      }

      if (!CheckAssignType(alloc_type, &initexpr, "new initializer"))
      {
        delete initexpr;
        return nullptr;
      }
    }
  }

  return new ONewExpr(alloc_type, initexpr, memalloc_func);
}

OExpr * ODqCompParser::ParseInheritedExpr()
{
  auto * owner_object = dynamic_cast<OTypeObject *>(curvsfunc ? curvsfunc->owner_compound_type : nullptr);
  if (!owner_object || !owner_object->base_type)
  {
    ErrorTxt(DQERR_INHERITED_CALL_INVALID, "inherited is only valid inside a derived object method");
    return nullptr;
  }
  if ((OSF_CREATE == curvsfunc->object_specfunc_kind) or (OSF_DESTROY == curvsfunc->object_specfunc_kind))
  {
    ErrorTxt(DQERR_INHERITED_CALL_INVALID, "inherited expressions are not valid in lifecycle methods");
    return nullptr;
  }

  string method_name;
  scf->SkipWhite();
  if (!scf->ReadIdentifier(method_name))
  {
    method_name = curvsfunc->name;
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    Error(DQERR_FUNC_CALL_PARENTH, method_name);
    return nullptr;
  }

  vector<TRawCallArg> rawargs;
  TRawCallArg thisarg;
  scf->SaveCurPos(thisarg.scpos_start);
  thisarg.expr = new OLValueVar(curvsfunc->receiver_arg);
  rawargs.push_back(thisarg);
  if (!ParseRawCallArguments(method_name, rawargs))
  {
    return nullptr;
  }

  vector<OExpr *> user_args;
  for (size_t i = 1; i < rawargs.size(); ++i)
  {
    user_args.push_back(rawargs[i].expr);
  }
  OValSymFunc * method = FindInheritedMethod(method_name, user_args);
  user_args.clear();
  if (!method)
  {
    FreeRawCallArguments(rawargs);
    Error(DQERR_OVERLOAD_NO_MATCH, method_name);
    return nullptr;
  }

  OCallExpr * result = new OCallExpr(method);
  result->force_direct = true;
  if (!BindCallArguments(method_name, static_cast<OTypeFunc *>(method->ptype), rawargs, result->args))
  {
    delete result;
    FreeRawCallArguments(rawargs);
    return nullptr;
  }

  FreeRawCallArguments(rawargs);
  return result;
}

OExpr * ODqCompParser::ParseBuiltinIif()
{
  auto recover_iif_tail = [this]()
  {
    scf->ReadTo(");");
    scf->CheckSymbol(")");
  };

  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "iif");
    return nullptr;
  }

  OExpr * condexpr = ParseExpression();
  if (!condexpr)
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "0", "iif", "3");
    return nullptr;
  }

  OType * condtype = condexpr->ResolvedType();
  if (!condtype || (TK_BOOL != condtype->kind))
  {
    Error(DQERR_BOOL_EXPR_EXPECTED, condtype ? condtype->name : "void");
    recover_iif_tail();
    delete condexpr;
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(","))
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "1", "iif", "3");
    recover_iif_tail();
    delete condexpr;
    return nullptr;
  }

  OExpr * trueexpr = ParseExpression();
  if (!trueexpr)
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "iif", "1", "3");
    delete condexpr;
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(","))
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "2", "iif", "3");
    recover_iif_tail();
    delete condexpr;
    delete trueexpr;
    return nullptr;
  }

  OExpr * falseexpr = ParseExpression();
  if (!falseexpr)
  {
    Error(DQERR_FUNC_ARGS_TOO_FEW, "2", "iif", "3");
    recover_iif_tail();
    delete condexpr;
    delete trueexpr;
    return nullptr;
  }

  scf->SkipWhite();
  if (scf->CheckSymbol(","))
  {
    Error(DQERR_FUNC_ARGS_TOO_MANY, "iif", "3");
    recover_iif_tail();
    delete condexpr;
    delete trueexpr;
    delete falseexpr;
    return nullptr;
  }

  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "iif");
    recover_iif_tail();
    delete condexpr;
    delete trueexpr;
    delete falseexpr;
    return nullptr;
  }

  OType * resulttype = nullptr;
  if (not ResolveIifType(&trueexpr, &falseexpr, &resulttype))
  {
    delete condexpr;
    delete trueexpr;
    delete falseexpr;
    return nullptr;
  }

  return new OIifExpr(condexpr, trueexpr, falseexpr, resulttype);
}

OExpr * ODqCompParser::ParseBuiltinFloatRound(ERoundMode amode)
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, GetRoundModeName(amode));
    return nullptr;
  }
  OExpr * argexpr = ParseExpression();
  if (!argexpr)  return nullptr;

  if (TK_FLOAT != argexpr->ptype->kind)
  {
    Error(DQERR_TYPE_FLOAT_EXPECTED_FOR, GetRoundModeName(amode), argexpr->ptype->name);
    delete argexpr;
    return nullptr;
  }
  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, GetRoundModeName(amode));
    delete argexpr;
    return nullptr;
  }
  return new OFloatRoundExpr(amode, argexpr);
}

OExpr * ODqCompParser::ParseBuiltinLen()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "len");
    return nullptr;
  }
  scf->SkipWhite();
  string lenarg;
  if (not scf->ReadIdentifier(lenarg))
  {
    Error(DQERR_VARNAME_EXP_AFTER, "len");
    return nullptr;
  }
  OValSym * lenvs = curscope->FindValSym(lenarg);
  if (!lenvs)
  {
    Error(DQERR_VAR_UNKNOWN, lenarg);
    return nullptr;
  }
  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "len");
    return nullptr;
  }
  if (TK_ARRAY == lenvs->ptype->kind)
  {
    OTypeArray * arrtype = static_cast<OTypeArray *>(lenvs->ptype);
    return new OIntLit(arrtype->arraylength);
  }
  else if (TK_ARRAY_SLICE == lenvs->ptype->kind)
  {
    return new OSliceLengthExpr(lenvs);
  }
  else if (TK_STRING == lenvs->ptype->kind)
  {
    return new OCStringLenExpr(lenvs);
  }
  else
  {
    Error(DQERR_LEN_INVALID_TYPE, lenvs->ptype->name);
    return nullptr;
  }
}

OExpr * ODqCompParser::ParseBuiltinSizeof()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "sizeof");
    return nullptr;
  }
  scf->SkipWhite();

  OType * sizetype = nullptr;
  OScPosition argpos;
  scf->SaveCurPos(argpos);

  if (scf->CheckSymbol("^", false) || scf->CheckSymbol("[", false))
  {
    sizetype = ParseTypeSpec();
  }
  else
  {
    string sarg;
    if (not scf->ReadIdentifier(sarg))
    {
      ErrorTxt(DQERR_VARNAME_EXP_AFTER, "Variable or type name is expected after \"sizeof\"");
      return nullptr;
    }

    OValSym * vs = curscope->FindValSym(sarg);
    if (vs)
    {
      sizetype = vs->ptype;
    }
    else
    {
      OType * foundtype = cur_mod_scope->FindType(sarg);
      if (!foundtype)
      {
        ErrorTxt(DQERR_EXPR_EXPECTED, "sizeof() expects a variable or type name");
        return nullptr;
      }

      scf->SetCurPos(argpos);
      sizetype = ParseTypeSpec();
    }
  }

  if (!sizetype)
  {
    return nullptr;
  }
  sizetype->EnsureLayout();

  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "sizeof");
    return nullptr;
  }

  if (0 == sizetype->bytesize)
  {
    ErrorTxt(DQERR_NOT_SUPPORTED, "sizeof() requires a statically sized type or value");
    return nullptr;
  }

  return new OIntLit(sizetype->bytesize);
}

OExpr * ODqCompParser::ParseBuiltinOffsetof()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol("("))
  {
    Error(DQERR_MISSING_OPEN_PAREN_AFTER, "offsetof");
    return nullptr;
  }

  OType * ptype = ParseTypeSpec();
  if (!ptype)
  {
    return nullptr;
  }
  ptype = ptype->ResolveAlias();

  OCompoundType * ctype = dynamic_cast<OCompoundType *>(ptype);
  if (!ctype)
  {
    Error(DQERR_TYPE_EXPECTED, "compound", ptype->name);
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(","))
  {
    Error(DQERR_MISSING_COMMA);
    return nullptr;
  }

  scf->SkipWhite();
  string membername;
  if (not scf->ReadIdentifier(membername))
  {
    Error(DQERR_MEMBER_NAME_EXPECTED);
    return nullptr;
  }

  ctype->EnsureLayout();
  int midx = ctype->FindMemberIndex(membername);
  if (midx < 0)
  {
    Error(DQERR_MEMBER_UNKNOWN, membername, ctype->name);
    return nullptr;
  }

  scf->SkipWhite();
  if (not scf->CheckSymbol(")"))
  {
    Error(DQERR_MISSING_CLOSE_PAREN_FOR, "offsetof");
    return nullptr;
  }

  return new OIntLit(ctype->member_order[midx]->field_offset);
}

EBinOp ODqCompParser::ParseAssignOp()
{
  scf->SkipWhite();

  if      (scf->CheckSymbol("+="))      return BINOP_ADD;
  else if (scf->CheckSymbol("-="))      return BINOP_SUB;
  else if (scf->CheckSymbol("*="))      return BINOP_MUL;
  else if (scf->CheckSymbol("/="))      return BINOP_DIV;
  else if (scf->CheckSymbol("<<="))     return BINOP_ISHL;
  else if (scf->CheckSymbol(">>="))     return BINOP_ISHR;
  else if (scf->CheckSymbol("=IDIV="))  return BINOP_IDIV;
  else if (scf->CheckSymbol("=IMOD="))  return BINOP_IMOD;
  else if (scf->CheckSymbol("=AND="))   return BINOP_IAND;
  else if (scf->CheckSymbol("=OR="))    return BINOP_IOR;
  else if (scf->CheckSymbol("=XOR="))   return BINOP_IXOR;
  else if (scf->CheckSymbol("="))       return BINOP_NONE;  // must come after =XXX=!, simple assign (ab)uses BINOP_NONE
  return EBinOp(-1);  // not an assignment operator
}

void ODqCompParser::VarInitError(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos)
{
  if (supress_varinit_check)
  {
    AddSuppressedVarInitDiag(varexpr, valsym, scpos);
  }
  else
  {
    Error(DQERR_VAR_NOT_INITIALIZED, valsym->name, &scpos);
  }
}

void ODqCompParser::AddSuppressedVarInitDiag(OLValueVar * varexpr, OValSym * valsym, OScPosition & scpos)
{
  TSuppressedVarInitDiag diag;
  diag.varexpr = varexpr;
  diag.valsym = valsym;
  diag.scpos = scpos;
  suppressed_varinit_diags.push_back(diag);
}

void ODqCompParser::EmitSuppressedVarInitDiags()
{
  if (suppressed_varinit_diags.empty())
  {
    return;
  }

  for (auto & diag : suppressed_varinit_diags)
  {
    if (diag.valsym->IsRefLike() && (FPM_REFOUT == diag.valsym->param_mode))
    {
      Error(DQERR_REFOUT_READ_BEFORE_WRITE, diag.valsym->name, &diag.scpos);
    }
    else
    {
      Error(DQERR_VAR_NOT_INITIALIZED, diag.valsym->name, &diag.scpos);
    }
  }

  suppressed_varinit_diags.clear();
}

void ODqCompParser::EmitFilteredAssignVarInitDiags(OLValueExpr * leftexpr, EBinOp op)
{
  if (suppressed_varinit_diags.empty())
  {
    return;
  }

  vector<OLValueVar *> ignored;
  if (BINOP_NONE == op)
  {
    CollectIgnoredPlainAssignVars(leftexpr, ignored);
  }

  for (auto & diag : suppressed_varinit_diags)
  {
    bool emit = true;

    if (BINOP_NONE == op)
    {
      for (OLValueVar * ignoredvar : ignored)
      {
        if (ignoredvar == diag.varexpr)
        {
          emit = false;
          break;
        }
      }
    }

    if (emit)
    {
      if (diag.valsym->IsRefLike() && (FPM_REFOUT == diag.valsym->param_mode))
      {
        Error(DQERR_REFOUT_READ_BEFORE_WRITE, diag.valsym->name, &diag.scpos);
      }
      else
      {
        Error(DQERR_VAR_NOT_INITIALIZED, diag.valsym->name, &diag.scpos);
      }
    }
  }

  suppressed_varinit_diags.clear();
}

bool ODqCompParser::FinalizeStmtAssign(OLValueExpr * leftexpr, EBinOp op, OExpr * rightexpr)
{
  if (!leftexpr || !rightexpr)
  {
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  OValSym * rootvalsym = GetAssignRootValSym(leftexpr);
  if (rootvalsym && VSK_CONST == rootvalsym->kind)
  {
    Error(DQERR_TYPE_ASSIGN_TO_CONST, rootvalsym->name);
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  if (rootvalsym && !rootvalsym->IsRefWriteable())
  {
    Error(DQERR_REF_ASSIGN_READONLY, rootvalsym->name);
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  if (leftexpr->IsFixedObjectStorageExpr())
  {
    Error(DQERR_REF_ASSIGN_READONLY, rootvalsym ? rootvalsym->name : "?");
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  OType * targettype = leftexpr->ptype;

  // Pointer arithmetic: p += int  or  p -= int
  if (TK_POINTER == targettype->kind and (BINOP_ADD == op or BINOP_SUB == op))
  {
    OTypePointer * ptrtype = static_cast<OTypePointer *>(targettype->ResolveAlias());
    if (!ptrtype->IsTypedPointer())
    {
      Error(DQERR_PTR_OPAQUE_USAGE, "pointer arithmetic");
      delete leftexpr;
      delete rightexpr;
      return false;
    }

    if (TK_INT != rightexpr->ptype->kind)
    {
      Error(DQERR_PTRARITH_TYPE, rightexpr->ptype->name);
      delete leftexpr;
      delete rightexpr;
      return false;
    }

    curblock->AddStatement(new OStmtModifyAssign(scpos_statement_start, leftexpr, op, rightexpr));
    return true;
  }

  if (not CheckAssignType(targettype, &rightexpr, "Assignment"))
  {
    delete leftexpr;
    delete rightexpr;
    return false;
  }

  if (BINOP_NONE == op)
  {
    curblock->AddStatement(new OStmtAssign(scpos_statement_start, leftexpr, rightexpr));
    if (rootvalsym && (VSK_VARIABLE == rootvalsym->kind || VSK_PARAMETER == rootvalsym->kind))
    {
      curblock->scope->SetVarInitialized(rootvalsym);
    }
  }
  else
  {
    curblock->AddStatement(new OStmtModifyAssign(scpos_statement_start, leftexpr, op, rightexpr));
  }

  return true;
}

void ODqCompParser::FinalizeStmtVoidCall(OExpr * callexpr)
{
  curblock->AddStatement(new OStmtVoidCall(scpos_statement_start, callexpr));
}

bool ODqCompParser::CheckStatementClose()
{
  scf->SkipWhite();
  if (not scf->CheckSymbol(";"))
  {
    StatementError(DQERR_MISSING_SEMICOLON_TO_CLOSE, "previous statement");
    return false;
  }
  return true;
}

OValSymConst * ODqCompParser::ParseDefineConst(const OScPosition & scpos, const string & sid)
{
  OScPosition    saved_stmtpos = scpos_statement_start;
  OScPosition *  saved_errorpos = errorpos;
  OScope *       saved_scope = curscope;
  OStmtBlock *   saved_block = curblock;

  scpos_statement_start = scpos;
  errorpos = &scpos_statement_start;
  curscope = g_defines;
  curblock = nullptr;
  scf->SetDirectiveExprMode(true);

  OValSymConst *  result = nullptr;
  OExpr *         valueexpr = nullptr;

  scf->SkipWhite();
  valueexpr = ParseExpression();
  if (!valueexpr)
  {
    goto cleanup;
  }

  if (!valueexpr->ptype)
  {
    ErrorTxt(DQERR_CDIR_EXPR_TYPE, "Unable to determine the type of #define \"$1\"", sid);
    goto cleanup;
  }

  {
    OType *   valuetype = valueexpr->ResolvedType();
    OValue *  value = valuetype->CreateValue();
    if (!value)
    {
      ErrorTxt(DQERR_CDIR_EXPR_TYPE, "Unsupported #define value type: \"$1\"", valuetype->name);
      goto cleanup;
    }

    if (!value->CalculateConstant(valueexpr))
    {
      delete value;
      goto cleanup;
    }

    result = new OValSymConst(scpos_statement_start, sid, valuetype, value);
  }

cleanup:
  delete valueexpr;

  scpos_statement_start = saved_stmtpos;
  errorpos = saved_errorpos;
  curscope = saved_scope;
  curblock = saved_block;
  scf->SetDirectiveExprMode(false);

  return result;
}

bool ODqCompParser::ParseDefineCondition(const OScPosition & scpos, bool * rok)
{
  OScPosition    saved_stmtpos = scpos_statement_start;
  OScPosition *  saved_errorpos = errorpos;
  OScope *       saved_scope = curscope;
  OStmtBlock *   saved_block = curblock;

  scpos_statement_start = scpos;
  errorpos = &scpos_statement_start;
  curscope = g_defines;
  curblock = nullptr;
  scf->SetDirectiveExprMode(true);

  bool     result = false;
  bool     ok = false;
  OExpr *  valueexpr = nullptr;

  scf->SkipWhite();
  valueexpr = ParseExpression();
  if (!valueexpr)
  {
    goto cleanup;
  }

  if (!valueexpr->ResolvedType() || (TK_BOOL != valueexpr->ResolvedType()->kind))
  {
    ErrorTxt(DQERR_CDIR_EXPR, "Preprocessor condition must be a boolean expression");
    goto cleanup;
  }

  {
    OValueBool cond(g_builtins->type_bool, false);
    if (!cond.CalculateConstant(valueexpr))
    {
      goto cleanup;
    }
    result = cond.value;
    ok = true;
  }

cleanup:
  delete valueexpr;

  scpos_statement_start = saved_stmtpos;
  errorpos = saved_errorpos;
  curscope = saved_scope;
  curblock = saved_block;
  scf->SetDirectiveExprMode(false);

  if (rok)
  {
    *rok = ok;
  }

  return result;
}
