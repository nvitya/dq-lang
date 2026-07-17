/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
#include "dqc.h"
#include "otype_func.h"
#include "otype_array.h"
#include "otype_cstring.h"
#include "otype_string.h"
#include "otype_int.h"
#include "otype_enum.h"
#include "otype_compound.h"
#include "named_scopes.h"
#include "scope_defines.h"
#include "expressions.h"
#include "statements.h"
#include "module_path.h"

using namespace std;

static bool IsCStringMethodSourceType(OType * type)
{
  return IsTextSourceType(type);
}

static bool IsStringMethodSourceType(OType * type)
{
  return IsTextSourceType(type);
}

static OLValueExpr * CloneContextLValue(OLValueExpr * src)
{
  if (auto * var = dynamic_cast<OLValueVar *>(src))
  {
    return new OLValueVar(var->pvalsym);
  }
  if (auto * member = dynamic_cast<OLValueMember *>(src))
  {
    OLValueExpr * base = CloneContextLValue(member->base);
    if (!base)
    {
      return nullptr;
    }
    return new OLValueMember(base, member->structtype, member->memberindex, member->ptype);
  }
  return nullptr;
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

    ODqCompBaseSuppressWarningsScope sws(this, attr->IsSet(ATTF_NOWARN));

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
      scf->EnterImplementationSection();
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
    else if ("enum" == sid)
    {
      ParseEnumDecl();
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
      int line_before = scf->last_token_end_line;
      scf->SkipWhite();
      if (scf->CheckSymbol(";", false) or scf->CheckSymbol(",", false))  // detect the end of the use block
      {
        break;
      }
      if (scf->curline > line_before)  // line break also ends the use block
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
      artifact_intf.EnsureFreshInterfaceArtifact(use_path);
    if (!artifact_result.Ok())
    {
      report_artifact_error(artifact_result);
      return;
    }

    filesystem::path interface_load_path = use_path.interface_artifact_path;

    if (!g_opt.ifgen && !in_module_stack)
    {
      artifact_result = artifact_intf.EnsureFreshCompiledArtifact(use_path);
      if (!artifact_result.Ok())
      {
        report_artifact_error(artifact_result);
        return;
      }
    }

    int prev_errorcnt = errorcnt;
    if (!g_module->UseCompiledModule(module_path, namespace_name, interface_load_path.string(),
                                    artifact_path.string(), cur_mod_scope, !section_public, merge_mode,
                                    symbol_names, reexport))
    {
      if (prev_errorcnt == errorcnt)
      {
        OScPosition errpos;
        errpos.Assign(scpos_statement_start);
        errpos.RecalcLineCol();
        Error(DQERR_USE_INTERFACE_LOAD, interface_load_path.string(),
              g_module->last_interface_load_error.empty() ? "unknown error" : g_module->last_interface_load_error,
              &errpos);
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

void ODqCompParser::ParseEnumDecl()
{
  string enum_name;
  scf->SkipWhite();
  if (!scf->ReadIdentifier(enum_name))
  {
    RootStatementError(DQERR_ID_EXP_AFTER, "enum");
    return;
  }
  if (!CheckSpecialReservedRootName(enum_name))
  {
    return;
  }
  if (g_module->TypeDeclared(enum_name))
  {
    RootStatementError(DQERR_TYPE_ALREADY_DEFINED, enum_name, &scf->prevpos);
    return;
  }

  OTypeInt * storage_type = g_builtins->type_uint8;
  scf->SkipWhite();
  if (scf->CheckSymbol(":"))
  {
    OType * parsed_storage = ParseTypeSpec();
    storage_type = dynamic_cast<OTypeInt *>(parsed_storage ? parsed_storage->ResolveAlias() : nullptr);
    if (!storage_type
        || (storage_type != g_builtins->type_int8 && storage_type != g_builtins->type_uint8
            && storage_type != g_builtins->type_int16 && storage_type != g_builtins->type_uint16
            && storage_type != g_builtins->type_int32 && storage_type != g_builtins->type_uint32
            && storage_type != g_builtins->type_int64 && storage_type != g_builtins->type_uint64))
    {
      Error(DQERR_ENUM_STORAGE_TYPE);
      storage_type = g_builtins->type_uint8;
    }
  }

  scf->SkipWhite();
  if (!scf->CheckSymbol("="))
  {
    RootStatementError(DQERR_MISSING_ASSIGN_FOR, enum_name);
    return;
  }
  scf->SkipWhite();
  if (!scf->CheckSymbol("("))
  {
    RootStatementError(DQERR_MISSING_OPEN_PAREN_FOR, "enum");
    return;
  }

  auto * enum_type = new OTypeEnum(enum_name, storage_type);
  g_module->DeclareType(section_public, enum_type);

  uint64_t next_value = 0;
  bool previous_was_max = false;
  while (!scf->Eof())
  {
    scf->SkipWhite();
    if (scf->CheckSymbol(")"))
    {
      break;
    }
    if (!enum_type->items.empty() && !scf->CheckSymbol(","))
    {
      Error(DQERR_MISSING_COMMA_IN, "enum");
      break;
    }
    scf->SkipWhite();
    if (scf->CheckSymbol(")"))
    {
      break;
    }

    string item_name;
    if (!scf->ReadIdentifier(item_name))
    {
      Error(DQERR_ID_EXP_AFTER, "enum item");
      break;
    }
    if (enum_type->FindItem(item_name))
    {
      Error(DQERR_ENUM_ITEM_DUPLICATE, item_name);
    }

    uint64_t item_value = next_value;
    bool source_negative = false;
    bool explicit_value = false;
    bool value_valid = true;
    bool const_expr_error = false;
    scf->SkipWhite();
    if (scf->CheckSymbol("="))
    {
      explicit_value = true;
      scf->SkipWhite();
      source_negative = (*scf->curp == '-');
      OExpr * value_expr = ParseExpression();
      int64_t signed_value = 0;
      if (!value_expr || !TryCalculateIntConstant(value_expr, signed_value))
      {
        Error(DQERR_ENUM_VALUE_CONST_EXPR, item_name);
        value_valid = false;
        const_expr_error = true;
      }
      else
      {
        item_value = uint64_t(signed_value);
      }
      OExpr::DeleteTree(value_expr);
    }
    else if (previous_was_max)
    {
      value_valid = false;
    }

    if (value_valid && explicit_value && !enum_type->items.empty())
    {
      uint64_t previous_value = enum_type->items.back().value;
      bool smaller = storage_type->issigned
          ? (int64_t(item_value) < int64_t(previous_value))
          : (item_value < previous_value);
      if (smaller)
      {
        Error(DQERR_ENUM_VALUE_ORDER, item_name);
      }
    }

    bool in_range = value_valid;
    if (in_range && storage_type->issigned)
    {
      int64_t signed_value = int64_t(item_value);
      if (storage_type->bitlength < 64)
      {
        int64_t min_value = -(int64_t(1) << (storage_type->bitlength - 1));
        int64_t max_value =  (int64_t(1) << (storage_type->bitlength - 1)) - 1;
        in_range = (signed_value >= min_value && signed_value <= max_value);
      }
      else if (!source_negative && explicit_value && signed_value < 0)
      {
        in_range = false;
      }
    }
    else if (in_range)
    {
      if (source_negative)
      {
        in_range = false;
      }
      else if (storage_type->bitlength < 64)
      {
        uint64_t max_value = (uint64_t(1) << storage_type->bitlength) - 1;
        in_range = item_value <= max_value;
      }
    }
    if (!in_range && !const_expr_error)
    {
      Error(DQERR_ENUM_VALUE_RANGE, item_name, storage_type->name);
    }
    if (in_range && enum_type->HasValue(item_value))
    {
      Error(DQERR_ENUM_VALUE_DUPLICATE, item_name);
    }
    enum_type->items.push_back({item_name, item_value});
    previous_was_max = (item_value == (storage_type->issigned
        ? (storage_type->bitlength == 64 ? uint64_t(INT64_MAX)
                                         : uint64_t((int64_t(1) << (storage_type->bitlength - 1)) - 1))
        : (storage_type->bitlength == 64 ? UINT64_MAX
                                         : ((uint64_t(1) << storage_type->bitlength) - 1))));
    next_value = item_value + 1;
  }

  if (enum_type->items.empty())
  {
    Error(DQERR_ENUM_EMPTY, enum_name);
  }
  CheckStatementClose();
}

void ODqCompParser::ParseCompoundBlockStart(const string & end_keyword, string & rblock_closer)
{
  scf->SkipWhite();
  if (scf->CheckSymbol("{"))
  {
    rblock_closer = "}";
    return;
  }

  if (scf->CheckSymbol(":"))
  {
    rblock_closer = end_keyword;
    return;
  }

  Error(DQERR_STMTBLK_START_MISSING);
  rblock_closer = end_keyword;
}

bool ODqCompParser::CheckCompoundBlockEnd(const string & block_closer)
{
  return !block_closer.empty() && scf->CheckSymbol(block_closer.c_str());
}

void ODqCompParser::ParseStructDecl()
{
  // note: "struct" is already consumed
  // syntax form: "struct Name:\n  field : type;\n  ...\nendstruct" or "struct Name { ... }"

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

  if (!ParseAttributes(false))
  {
    SkipToModuleStatementStart();
    return;
  }

  OType * existing = cur_mod_scope->FindType(sname, nullptr, false);
  if (!existing && !section_public)
  {
    existing = g_module->scope_pub->FindType(sname, nullptr, false);
  }

  OCompoundType * ctype = nullptr;
  bool is_forward_def = false;

  if (existing)
  {
    if (existing->incomplete && (TK_STRUCT == existing->kind))
    {
      ctype = static_cast<OCompoundType *>(existing);
      is_forward_def = true;
    }
    else
    {
      RootStatementError(DQERR_TYPE_ALREADY_DEFINED_IN, sname, existing->module ? existing->module->name : "");
      return;
    }
  }
  else
  {
    ctype = new OCompoundType(sname, cur_mod_scope);
    ctype->incomplete = true;
    g_module->DeclareType(section_public, ctype);
  }

  if (attr->flags)
  {
    attr->CheckInvalidAttributes(ATGT_COMPOUND_TYPE);
    ctype->is_packed = attr->IsSet(ATTF_PACKED);
    if (attr->IsSet(ATTF_FORWARD))
    {
      if (!is_forward_def && !g_module->OModuleBase::declarations.empty())
      {
        g_module->OModuleBase::declarations.back()->is_forward = true;
      }
      return;
    }
  }


  auto discard_failed_new_decl = [&]()
  {
    if (!is_forward_def)
    {
      g_module->DiscardTypeDeclaration(ctype);
      delete ctype;
    }
  };

  scf->SkipWhite();
  if (scf->CheckSymbol("("))
  {
    OType * basetype = ParseTypeSpec();
    OCompoundType * base_struct = dynamic_cast<OCompoundType *>(basetype ? basetype->ResolveAlias() : nullptr);
    if (!base_struct || (TK_STRUCT != base_struct->kind))
    {
      Error(DQERR_TYPE_EXPECTED, "struct", basetype ? basetype->name : "?");
      SkipToModuleStatementStart();
      discard_failed_new_decl();
      return;
    }
    if (base_struct->ContainsManagedStorage())
    {
      ErrorTxt(DQERR_NOT_SUPPORTED, "managed struct base");
      SkipToModuleStatementStart();
      discard_failed_new_decl();
      return;
    }
    ctype->base_type = base_struct;

    scf->SkipWhite();
    if (scf->CheckSymbol(","))
    {
      ErrorTxt(DQERR_NOT_SUPPORTED, "multiple struct inheritance");
      SkipToModuleStatementStart();
      discard_failed_new_decl();
      return;
    }
    if (!scf->CheckSymbol(")"))
    {
      Error(DQERR_MISSING_CLOSE_PAREN_FOR, "struct base");
      SkipToModuleStatementStart();
      discard_failed_new_decl();
      return;
    }
  }

  string block_closer;
  ParseCompoundBlockStart("endstruct", block_closer);

  OScPosition mempos;
  string membername;

  while (not scf->Eof())
  {
    scf->SkipWhite();

    if (CheckCompoundBlockEnd(block_closer))
    {
      break;
    }

    if (!ParseAttributes(true))
    {
      SkipCurStatement();
      continue;
    }

    scf->SkipWhite();
    if (CheckCompoundBlockEnd(block_closer))
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
      ReadCompoundMethod(ctype, MV_PUBLIC);
      continue;
    }

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
    if (mtype->ContainsManagedStorage())
    {
      StatementError(DQERR_NOT_SUPPORTED, "managed struct member");
      break;
    }

    if (!ParseAttributes(false))
    {
      SkipCurStatement();
      continue;
    }

    CheckStatementClose();

    OValSym * mvsym = mtype->CreateValSym(mempos, membername);
    mvsym->initialized = true;  // struct members are always accessible
    mvsym->ApplyAttributes(attr, ATGT_STRUCT_MEMBER);
    ctype->AddMember(mvsym);
  }

  ctype->EnsureLayout();
  ctype->incomplete = false;
  ctype->layout_ready = true;

  if (is_forward_def)
  {
    g_module->OModuleBase::declarations.push_back(new OIntfDecl(ctype, false));
  }
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
  if (vsfunc->attr_is_always_inline && vsfunc->attr_is_noinline)
  {
    OScPosition errpos(scf->curfile, scf->curp);
    Error(DQERR_ATTR_CONFLICT, "[[always_inline]] and [[noinline]]", &errpos);
    RecoverFailedFunctionDecl();
    curvsfunc = nullptr;
    delete vsfunc;
    return false;
  }

  if (vsfunc->owner_compound_type)
  {
    auto * owner_object = dynamic_cast<OTypeObject *>(vsfunc->owner_compound_type);
    if (!owner_object
        && (vsfunc->attr_is_virtual || vsfunc->attr_is_override
            || vsfunc->attr_is_abstract || vsfunc->attr_is_final))
    {
      ErrorTxt(DQERR_OBJ_FUNC_OVERRIDE,
               "struct methods cannot be virtual, override, abstract, or final");
      RecoverFailedFunctionDecl();
      curvsfunc = nullptr;
      delete vsfunc;
      return false;
    }

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
  bool has_body = scf->CheckSymbol(":", false) || scf->CheckSymbol("{", false);
  bool is_declaration_only = !has_body;

  auto consume_declaration_semicolon = [&](const string & what)
  {
    if (has_body)
    {
      Error(DQERR_FUNC_NO_BODY_ALLOWED_AFTER, what);
    }
    else if (not CheckStatementClose())
    {
      // CheckStatementClose generates error if missing
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

    if ((fwdfunc->attr_is_always_inline || vsfunc->attr_is_always_inline)
        && (fwdfunc->attr_is_noinline || vsfunc->attr_is_noinline))
    {
      OScPosition errpos(scf->curfile, scf->curp);
      Error(DQERR_ATTR_CONFLICT, "[[always_inline]] and [[noinline]]", &errpos);
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

bool ODqCompParser::ReadCompoundMethod(OCompoundType * compound_type, EMemberVisibility avisibility)
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
    objspecfunc_kind = ObjectSpecFuncKindFromName(sid);
    if (OSF_NONE == objspecfunc_kind)
    {
      ErrorTxt(DQERR_OBJ_SPEC_FUNC_INVALID, sid);
      RecoverFailedFunctionDecl();
      return false;
    }
  }

  OTypeFunc    * tfunc  = new OTypeFunc(method_name);

  OValSymFunc  * vsfunc = new OValSymFunc(scpos_statement_start, method_name, tfunc, compound_type->Members());
  vsfunc->owner_compound_type = compound_type;
  vsfunc->generated_linkage_name = compound_type->name + "." + method_name;
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
  OTypeObject * object_type = dynamic_cast<OTypeObject *>(compound_type);
  if (!object_type && (OSF_NONE != objspecfunc_kind))
  {
    ErrorTxt(DQERR_OBJ_SPEC_FUNC_INVALID, "struct methods cannot be constructors or destructors");
    RecoverFailedFunctionDecl();
    curvsfunc = nullptr;
    delete vsfunc;
    return false;
  }

  InjectObjectReceiver(vsfunc, compound_type);

  string owner_desc = object_type ? "object method declaration" : "struct method declaration";
  bool ok = FinishFunctionDecl(vsfunc, compound_type->Members(), compound_type->Members(), true, false, owner_desc);
  if (ok && object_type && (OSF_NONE != objspecfunc_kind))
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

static bool SamePropertyType(OType * left, OType * right)
{
  return left && right && (left->ResolveAlias() == right->ResolveAlias());
}

static bool PropertyAccessorVisibleFrom(OTypeObject * owner, OCompoundType * decl_type, OValSym * accessor)
{
  if (!owner || !decl_type || !accessor)
  {
    return false;
  }
  if (owner == decl_type || MV_PUBLIC == accessor->member_visibility)
  {
    return true;
  }
  return MV_PROTECTED == accessor->member_visibility && owner->IsSameOrDerivedFrom(decl_type);
}

enum EPropertyAccessorMismatch
{
  PAM_NONE,
  PAM_TYPE,
  PAM_SIGNATURE,
  PAM_MODE
};

static EPropertyAccessorMismatch MatchPropertyMethod(OValSymFunc * method, OValSymProperty * property,
                                                      bool write)
{
  auto * sig = dynamic_cast<OTypeFunc *>(method ? method->ptype : nullptr);
  if (!sig || sig->params.empty())
  {
    return PAM_SIGNATURE;
  }

  size_t expected_explicit = property->indices.size() + (write ? 1 : 0);
  if (sig->params.size() != expected_explicit + 1)
  {
    return PAM_SIGNATURE;
  }
  if (write ? (sig->rettype != nullptr) : !SamePropertyType(sig->rettype, property->ptype))
  {
    return write ? PAM_SIGNATURE : PAM_TYPE;
  }

  for (size_t i = 0; i < property->indices.size(); ++i)
  {
    OFuncParam * param = sig->params[i + 1];
    const OPropertyIndex & index = property->indices[i];
    if (param->mode != index.mode)
    {
      return PAM_MODE;
    }
    if (!SamePropertyType(param->ptype, index.ptype))
    {
      return write ? PAM_SIGNATURE : PAM_TYPE;
    }
  }

  if (write)
  {
    OFuncParam * value_param = sig->params.back();
    if (FPM_REF == value_param->mode || FPM_REFOUT == value_param->mode || FPM_REFNULL == value_param->mode)
    {
      return PAM_MODE;
    }
    if (!SamePropertyType(value_param->ptype, property->ptype))
    {
      return property->IsIndexed() ? PAM_SIGNATURE : PAM_TYPE;
    }
  }
  return PAM_NONE;
}

bool ODqCompParser::ReadObjectProperty(OTypeObject * object_type, EMemberVisibility avisibility)
{
  string property_name;
  scf->SkipWhite();
  if (!scf->ReadIdentifier(property_name))
  {
    Error(DQERR_ID_EXP_AFTER, "property");
    return false;
  }
  scf->SkipWhite();
  if (!scf->CheckSymbol(":"))
  {
    Error(DQERR_TYPE_SPECIFIER_EXP_AFTER, property_name);
    return false;
  }

  auto * property = new OValSymProperty(scpos_statement_start, property_name, nullptr);
  property->member_visibility = avisibility;

  scf->SkipWhite();
  OScPosition type_start;
  scf->SaveCurPos(type_start);
  bool indexed = false;
  if (scf->CheckSymbol("["))
  {
    scf->SkipWhite();
    indexed = !scf->IsIntLiteral();
  }
  scf->SetCurPos(type_start);

  if (indexed)
  {
    scf->CheckSymbol("[");
    while (!scf->Eof())
    {
      OPropertyIndex index;
      scf->SkipWhite();

      OScPosition index_start;
      scf->SaveCurPos(index_start);
      string first_id;
      if (scf->ReadIdentifier(first_id))
      {
        scf->SkipWhite();
        if (scf->CheckSymbol(":"))
        {
          index.name = first_id;
        }
        else
        {
          scf->SetCurPos(index_start);
        }
      }

      scf->SkipWhite();
      OScPosition mode_start;
      scf->SaveCurPos(mode_start);
      string mode_name;
      EParamMode mode = FPM_VALUE;
      if (scf->ReadIdentifier(mode_name) && ParseParamModeKeyword(mode_name, mode))
      {
        index.mode = mode;
        if (FPM_VALUE != mode && FPM_REFIN != mode)
        {
          Error(DQERR_PROPERTY_ACCESSOR_MODE, property_name, &property->scpos);
        }
      }
      else
      {
        scf->SetCurPos(mode_start);
      }

      index.ptype = ParseTypeSpec();
      if (!index.ptype)
      {
        delete property;
        return false;
      }
      property->indices.push_back(index);

      scf->SkipWhite();
      if (scf->CheckSymbol("]"))
      {
        break;
      }
      if (!scf->CheckSymbol(","))
      {
        Error(DQERR_MISSING_COMMA_IN, "property index list");
        delete property;
        return false;
      }
    }
    property->ptype = ParseTypeSpec();
  }
  else
  {
    property->ptype = ParseTypeSpec();
  }

  if (!property->ptype)
  {
    delete property;
    return false;
  }

  bool seen_default = false;
  bool seen_read = false;
  bool seen_write = false;
  string read_name;
  string write_name;
  int last_stage = -1;

  while (!scf->Eof())
  {
    int previous_line = scf->last_token_end_line;
    scf->SkipWhite();
    if (scf->curline > previous_line)
    {
      break;
    }

    string specifier;
    if (!scf->ReadIdentifier(specifier))
    {
      break;
    }

    int stage = ("default" == specifier ? 0 : ("read" == specifier ? 1 : ("write" == specifier ? 2 : -1)));
    if (stage < 0)
    {
      break;
    }
    bool duplicate = (0 == stage && seen_default) || (1 == stage && seen_read) || (2 == stage && seen_write);
    if (duplicate)
    {
      Error(DQERR_PROPERTY_SPECIFIER_DUPLICATE, specifier);
    }
    else if (stage < last_stage)
    {
      Error(DQERR_PROPERTY_SPECIFIER_ORDER, specifier);
    }
    last_stage = max(last_stage, stage);

    if (0 == stage)
    {
      seen_default = true;
      property->is_default = true;
      continue;
    }

    string accessor_name;
    scf->SkipWhite();
    if (!scf->ReadIdentifier(accessor_name))
    {
      Error(DQERR_ID_EXP_AFTER, specifier);
      continue;
    }
    if (1 == stage)
    {
      seen_read = true;
      if (read_name.empty()) read_name = accessor_name;
    }
    else
    {
      seen_write = true;
      if (write_name.empty()) write_name = accessor_name;
    }
  }

  if (!seen_read && !seen_write)
  {
    Error(DQERR_PROPERTY_ACCESSOR_REQUIRED, &property->scpos);
  }
  if (property->is_default && !property->IsIndexed())
  {
    Error(DQERR_PROPERTY_DEFAULT_INDEXED, &property->scpos);
  }

  auto resolve_accessor = [&](const string & accessor_name, bool write,
                              OValSym *& raccessor, OCompoundType *& rdecl_type)
  {
    if (accessor_name.empty())
    {
      return;
    }
    OCompoundType * decl_type = nullptr;
    OValSym * symbol = object_type->FindMemberSymbol(accessor_name, &decl_type);
    if (!symbol || !PropertyAccessorVisibleFrom(object_type, decl_type, symbol))
    {
      Error(DQERR_PROPERTY_ACCESSOR_UNKNOWN, accessor_name, &property->scpos);
      return;
    }

    if (auto * field = dynamic_cast<OValSymProperty *>(symbol))
    {
      (void)field;
      Error(DQERR_PROPERTY_ACCESSOR_UNKNOWN, accessor_name, &property->scpos);
      return;
    }

    if (VSK_FUNCTION != symbol->kind)
    {
      if (property->IsIndexed())
      {
        Error(DQERR_PROPERTY_INDEXED_ACCESSOR_METHOD, accessor_name, &property->scpos);
      }
      else if (!SamePropertyType(symbol->ptype, property->ptype))
      {
        Error(DQERR_PROPERTY_ACCESSOR_TYPE, accessor_name, &property->scpos);
      }
      else
      {
        raccessor = symbol;
        rdecl_type = decl_type;
      }
      return;
    }

    vector<OValSymFunc *> candidates;
    if (auto * method = dynamic_cast<OValSymFunc *>(symbol))
    {
      candidates.push_back(method);
    }
    else if (auto * overloads = dynamic_cast<OValSymOverloadSet *>(symbol))
    {
      candidates = overloads->funcs;
    }

    OValSymFunc * match = nullptr;
    EPropertyAccessorMismatch best_mismatch = PAM_SIGNATURE;
    for (OValSymFunc * candidate : candidates)
    {
      EPropertyAccessorMismatch mismatch = MatchPropertyMethod(candidate, property, write);
      if (PAM_NONE == mismatch)
      {
        if (match)
        {
          Error(DQERR_PROPERTY_ACCESSOR_SIGNATURE, accessor_name, &property->scpos);
          return;
        }
        match = candidate;
      }
      else if (PAM_MODE == mismatch || (PAM_TYPE == mismatch && PAM_MODE != best_mismatch))
      {
        best_mismatch = mismatch;
      }
    }
    if (!match)
    {
      if (PAM_MODE == best_mismatch)
      {
        Error(DQERR_PROPERTY_ACCESSOR_MODE, accessor_name, &property->scpos);
      }
      else if (PAM_TYPE == best_mismatch)
      {
        Error(DQERR_PROPERTY_ACCESSOR_TYPE, accessor_name, &property->scpos);
      }
      else
      {
        Error(DQERR_PROPERTY_ACCESSOR_SIGNATURE, accessor_name, &property->scpos);
      }
      return;
    }
    raccessor = match;
    rdecl_type = decl_type;
  };

  resolve_accessor(read_name, false, property->read_accessor, property->read_decl_type);
  resolve_accessor(write_name, true, property->write_accessor, property->write_decl_type);

  object_type->AddProperty(property);
  return true;
}

void ODqCompParser::ParseObjectDecl()
{
  // note: "object" is already consumed
  // syntax form: "object Name:\n  field : type;  function Method(...): ... endfunc\nendobj" or "object Name { ... }"

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

  if (!ParseAttributes(false))
  {
    SkipToModuleStatementStart();
    return;
  }

  OType * existing = cur_mod_scope->FindType(sname, nullptr, false);
  if (!existing && !section_public)
  {
    existing = g_module->scope_pub->FindType(sname, nullptr, false);
  }

  OTypeObject * object_type = nullptr;
  bool is_forward_def = false;

  if (existing)
  {
    if (existing->incomplete && (TK_OBJECT == existing->kind))
    {
      object_type = static_cast<OTypeObject *>(existing);
      is_forward_def = true;
    }
    else
    {
      RootStatementError(DQERR_TYPE_ALREADY_DEFINED_IN, sname, existing->module ? existing->module->name : "");
      return;
    }
  }
  else
  {
    object_type = new OTypeObject(sname, cur_mod_scope);
    object_type->incomplete = true;
    g_module->DeclareType(section_public, object_type);
  }

  if (attr->flags)
  {
    attr->CheckInvalidAttributes(ATGT_COMPOUND_TYPE);
    object_type->is_packed = attr->IsSet(ATTF_PACKED);
    if (attr->IsSet(ATTF_FORWARD))
    {
      if (!is_forward_def && !g_module->OModuleBase::declarations.empty())
      {
        g_module->OModuleBase::declarations.back()->is_forward = true;
      }
      return;
    }
  }


  auto discard_failed_new_decl = [&]()
  {
    if (!is_forward_def)
    {
      g_module->DiscardTypeDeclaration(object_type);
      delete object_type;
    }
  };

  scf->SkipWhite();
  if (scf->CheckSymbol("("))
  {
    OType * basetype = ParseTypeSpec();
    OTypeObject * base_object = dynamic_cast<OTypeObject *>(basetype ? basetype->ResolveAlias() : nullptr);
    if (!base_object)
    {
      Error(DQERR_TYPE_EXPECTED, "object", basetype ? basetype->name : "?");
      SkipToModuleStatementStart();
      discard_failed_new_decl();
      return;
    }
    object_type->base_type = base_object;

    scf->SkipWhite();
    if (scf->CheckSymbol(","))
    {
      ErrorTxt(DQERR_NOT_SUPPORTED, "multiple object inheritance");
      SkipToModuleStatementStart();
      discard_failed_new_decl();
      return;
    }
    if (!scf->CheckSymbol(")"))
    {
      Error(DQERR_MISSING_CLOSE_PAREN_FOR, "object base");
      SkipToModuleStatementStart();
      discard_failed_new_decl();
      return;
    }
  }

  string block_closer;
  ParseCompoundBlockStart("endobj", block_closer);

  OScPosition mempos;
  string membername;
  EMemberVisibility current_visibility = MV_PUBLIC;

  while (not scf->Eof())
  {
    scf->SkipWhite();

    if (CheckCompoundBlockEnd(block_closer))
    {
      break;
    }

    if (!ParseAttributes(true))
    {
      SkipCurStatement();
      continue;
    }

    scf->SkipWhite();
    if (CheckCompoundBlockEnd(block_closer))
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
      ReadCompoundMethod(object_type, current_visibility);
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
    if ("property" == membername)
    {
      ReadObjectProperty(object_type, current_visibility);
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

      CheckStatementClose();

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

    CheckStatementClose();

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
  if (need_generated_ctors && object_type->GetBaseObject() && !object_type->GetBaseObject()->constructors.empty())
  {
    size_t ctor_count = object_type->GetBaseObject()->constructors.size();
    for (size_t i = 0; i < ctor_count; ++i)
    {
      AddGeneratedObjectConstructor(object_type, object_type->GetBaseObject()->constructors[i],
                                    scpos_statement_start, ctor_count);
    }
    need_generated_ctors = false;
  }

  bool needs_implicit_ctor = need_generated_ctors;

  if (needs_implicit_ctor)
  {
    if (object_type->base_type)
    {
      if (!object_type->GetBaseObject()->FindSpecialMethod(OSF_CREATE, 0))
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
      OValSymFunc * inherited_ctor = object_type->GetBaseObject()->FindSpecialMethod(OSF_CREATE, 0);
      auto * stmt = new OStmtInheritedCall(scpos_statement_start, ctor, inherited_ctor);
      stmt->emit_derived_field_init = true;
      ctor->body->stlist.push_back(stmt);
    }
  }

  if (!object_type->destructor)
  {
    OValSymFunc * inherited_dtor = (object_type->base_type
        ? object_type->GetBaseObject()->FindSpecialMethod(OSF_DESTROY)
        : nullptr);
    bool needs_implicit_dtor = true;

    if (needs_implicit_dtor)
    {
      AddGeneratedObjectDestructor(object_type, inherited_dtor, scpos_statement_start);
    }
  }

  object_type->UpdateObjectInheritanceFlags();
  object_type->EnsureLayout();
  object_type->incomplete = false;

  if (is_forward_def)
  {
    g_module->OModuleBase::declarations.push_back(new OIntfDecl(object_type, false));
  }
}

void ODqCompParser::ParseQualifiedObjectFunction(const string & object_name)
{
  string method_name;
  scf->SkipWhite();
  bool specfunc_decl = scf->CheckSymbol("*");

  if (not scf->ReadIdentifier(method_name))
  {
    Error(DQERR_ID_EXP_AFTER, object_name + ".");
    return;
  }

  EObjectSpecFuncKind objspecfunc_kind = OSF_NONE;
  if (specfunc_decl)
  {
    objspecfunc_kind = ObjectSpecFuncKindFromName(method_name);
    if (OSF_NONE == objspecfunc_kind)
    {
      ErrorTxt(DQERR_OBJ_SPEC_FUNC_INVALID, method_name);
      RecoverFailedFunctionDecl();
      return;
    }
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

  OCompoundType * compound_type = dynamic_cast<OCompoundType *>(foundtype->ResolveAlias());
  if (!compound_type)
  {
    Error(DQERR_TYPE_EXPECTED, "compound", foundtype->name);
    delete tfunc;
    RecoverFailedFunctionDecl();
    return;
  }

  OValSymFunc  * vsfunc = new OValSymFunc(scpos_statement_start, method_name, tfunc, compound_type->Members());
  vsfunc->owner_compound_type = compound_type;
  vsfunc->generated_linkage_name = compound_type->name + "." + method_name;
  vsfunc->object_specfunc_kind = objspecfunc_kind;
  curvsfunc = vsfunc;

  InjectObjectReceiver(vsfunc, compound_type);
  string owner_desc = compound_type->IsObject() ? "object method declaration" : "struct method declaration";
  FinishFunctionDecl(vsfunc, compound_type->Members(), compound_type->Members(), true, false, owner_desc);
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

// Table-driven binary operator parser: shared logic for all precedence levels


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
