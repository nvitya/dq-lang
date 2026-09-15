/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 *
 * Worker-side serialization deliberately walks compiler declarations.  Source
 * text is used only to locate the body of a forward-declared function after
 * the parser has resolved its owner and signature.
 */

#include <fstream>
#include <algorithm>
#include <unordered_set>

#include "dq_module.h"
#include "module_intf.h"
#include "jsontools.h"
#include "semantic_result.h"

#include "named_scopes.h"
#include "otype_compound.h"
#include "otype_func.h"

namespace
{

int SymbolKind(const ODecl & declaration)
{
  if (DK_VALSYM == declaration.kind)
  {
    switch (declaration.pvalsym->kind)
    {
      case VSK_FUNCTION: return 12; // Function
      case VSK_VARIABLE: return 13; // Variable
      case VSK_CONST:    return 14; // Constant
      case VSK_PROPERTY: return 7;  // Property
      case VSK_PARAMETER:return 13; // Variable
    }
    return 13;
  }

  switch (declaration.ptype->kind)
  {
    case TK_ENUM:   return 10; // Enum
    case TK_OBJECT: return 5;  // Class
    case TK_STRUCT:
    case TK_UNION:  return 23; // Struct
    default:        return 26; // TypeParameter is the closest generic type symbol
  }
}

int ValSymKind(int kind)
{
  switch (kind)
  {
    case VSK_FUNCTION: return 12;
    case VSK_VARIABLE: return 13;
    case VSK_CONST:    return 14;
    case VSK_PROPERTY: return 7;
    case VSK_PARAMETER:return 13;
  }
  return 13;
}

int TypeKind(int kind)
{
  switch (kind)
  {
    case TK_ENUM:   return 10;
    case TK_OBJECT: return 5;
    case TK_STRUCT:
    case TK_UNION:  return 23;
  }
  return 26;
}

const OSymbol * DeclarationSymbol(const ODecl & declaration)
{
  return (DK_VALSYM == declaration.kind ? static_cast<const OSymbol *>(declaration.pvalsym)
                                        : static_cast<const OSymbol *>(declaration.ptype));
}

bool HasSourcePosition(const OSymbol * symbol)
{
  return symbol && !symbol->name.empty() && !symbol->name.starts_with("__dq_")
      && symbol->scpos.scfile && symbol->scpos.pos && symbol->scpos.scfile->pstart;
}

bool IsGeneratedMethod(const OValSymFunc * function)
{
  return function && function->has_body && (function->scpos.pos == function->scpos_endfunc.pos);
}

OScPosition SymbolPosition(const OSymbol & symbol)
{
  const auto * function = dynamic_cast<const OValSymFunc *>(&symbol);
  if (!function || !function->scpos.scfile || !function->scpos.scfile->pstart)
  {
    return symbol.scpos;
  }

  string declaration = "func ";
  if (function->owner_compound_type)
  {
    declaration += function->owner_compound_type->name + ".";
    if (function->object_specfunc_kind != OSF_NONE) declaration += "*";
  }
  declaration += function->name;

  OScFile * file = function->scpos.scfile;
  string_view source(file->pstart, file->length);
  if (!function->owner_compound_type)
  {
    size_t position = source.rfind(declaration);
    return ((position == string_view::npos) || (file->pstart + position <= function->scpos.pos))
        ? symbol.scpos : OScPosition(file, file->pstart + position);
  }

  size_t overload_index = 0;
  const auto * overload_set = dynamic_cast<const OValSymOverloadSet *>(
      function->owner_compound_type->Members()->FindValSym(function->name, nullptr, false));
  if (overload_set)
  {
    auto it = find(overload_set->funcs.begin(), overload_set->funcs.end(), function);
    if (it != overload_set->funcs.end()) overload_index = size_t(it - overload_set->funcs.begin());
  }

  size_t position = 0;
  size_t found_count = 0;
  while (true)
  {
    position = source.find(declaration, position);
    if (position == string_view::npos) return symbol.scpos;
    position += declaration.length();
    if ((file->pstart + position > function->scpos.pos) && (found_count++ == overload_index)) break;
  }

  return OScPosition(function->scpos.scfile, function->scpos.scfile->pstart + position - declaration.length());
}

bool IsCompoundMember(const OValSym * symbol)
{
  if (const auto * function = dynamic_cast<const OValSymFunc *>(symbol))
  {
    return function->owner_compound_type != nullptr;
  }
  if (const auto * overload_set = dynamic_cast<const OValSymOverloadSet *>(symbol))
  {
    return overload_set->owner_compound_type != nullptr;
  }
  return false;
}

void AddSymbol(TJsonNode & symbols, const OSymbol & symbol, int kind, TJsonNode ** rout = nullptr)
{
  OScPosition position = SymbolPosition(symbol);
  TJsonNode & json_symbol = symbols.Add().GetAsObject();
  json_symbol.Add("name", symbol.name);
  json_symbol.Add("kind", kind);
  json_symbol.Add("path", position.scfile->fullpath);
  json_symbol.Add("line", position.line);
  json_symbol.Add("column", position.col);
  if (rout) *rout = &json_symbol;
}

void AddCompoundMembers(TJsonNode & json_parent, const OCompoundType & compound)
{
  struct SMember
  {
    const OValSym * symbol;
    int kind;
  };
  vector<SMember> members;
  unordered_set<const OValSym *> seen;
  auto add_member = [&](const OValSym * symbol, int kind)
  {
    if (HasSourcePosition(symbol) && seen.insert(symbol).second) members.push_back({symbol, kind});
  };
  auto add_function = [&](const OValSymFunc * function)
  {
    if (function && (function->owner_compound_type == &compound) && !IsGeneratedMethod(function))
    {
      add_member(function, ValSymKind(function->kind));
    }
  };

  for (const OValSym * member : compound.member_order)
  {
    if (member) add_member(member, ValSymKind(member->kind));
  }
  for (const OValSymProperty * property : compound.properties)
  {
    if (property) add_member(property, ValSymKind(property->kind));
  }
  for (const auto & entry : compound.member_scope.valsyms)
  {
    const OValSym * member = entry.second;
    if (const auto * overload_set = dynamic_cast<const OValSymOverloadSet *>(member))
    {
      for (const OValSymFunc * function : overload_set->funcs) add_function(function);
    }
    else add_function(dynamic_cast<const OValSymFunc *>(member));
  }
  if (members.empty()) return;

  sort(members.begin(), members.end(), [](const SMember & left, const SMember & right)
  {
    if (left.symbol->scpos.line != right.symbol->scpos.line) return left.symbol->scpos.line < right.symbol->scpos.line;
    return left.symbol->scpos.col < right.symbol->scpos.col;
  });
  TJsonNode & json_members = json_parent.Add("children").GetAsArray();
  for (const SMember & member : members)
  {
    AddSymbol(json_members, *member.symbol, member.kind);
  }
}

} // namespace

bool WriteDqLanguageServerSemanticResult(const string & filename, bool success, string & rerror)
{
  TJsonNode result(nkObject);
  result.Add("formatVersion", 1);
  result.Add("success", success);
  TJsonNode & document_symbols = result.Add("documentSymbols").GetAsArray();
  if (g_module)
  {
    for (const ODecl * declaration : g_module->declarations)
    {
      if (!declaration) continue;
      const OSymbol * symbol = DeclarationSymbol(*declaration);
      if (!HasSourcePosition(symbol)) continue;
      if (declaration->kind == DK_VALSYM)
      {
        if (IsCompoundMember(declaration->pvalsym)) continue;
        const auto * function = dynamic_cast<const OValSymFunc *>(declaration->pvalsym);
        if (function && (function->special_kind == SFK_MODULE_INIT)) continue;
      }

      TJsonNode * json_symbol = nullptr;
      AddSymbol(document_symbols, *symbol, SymbolKind(*declaration), &json_symbol);
      if (declaration->kind == DK_TYPE && declaration->ptype->IsCompound())
      {
        AddCompoundMembers(*json_symbol, *static_cast<const OCompoundType *>(declaration->ptype));
      }
    }
  }
  TJsonNode & json_namespaces = result.Add("namespaces").GetAsObject();
  TJsonNode & json_module_namespaces = result.Add("moduleNamespaces").GetAsArray();
  for (const auto & [ns_name, scope] : g_namespaces)
  {
    if (!scope) continue;
    json_module_namespaces.Add().SetAsString(ns_name);
    TJsonNode & json_symbols = json_namespaces.Add(ns_name).GetAsArray();
    for (const auto & [name, type] : scope->typesyms)
    {
      if (!type || name.empty() || name.starts_with("__dq_")) continue;
      TJsonNode & json_symbol = json_symbols.Add().GetAsObject();
      json_symbol.Add("name", name);
      json_symbol.Add("kind", TypeKind(type->kind));
    }
    for (const auto & [name, valsym] : scope->valsyms)
    {
      if (!valsym || name.empty() || name.starts_with("__dq_")) continue;
      TJsonNode & json_symbol = json_symbols.Add().GetAsObject();
      json_symbol.Add("name", name);
      json_symbol.Add("kind", ValSymKind(valsym->kind));
    }
  }

  TJsonNode & json_used_module_sources = result.Add("usedModuleSources").GetAsArray();
  unordered_set<string> used_module_sources;
  if (g_module)
  {
    for (const OModuleUse * use : g_module->used_modules)
    {
      const auto * module = use ? dynamic_cast<const OModuleIntf *>(use->module) : nullptr;
      if (!module) continue;
      bool added_source = false;
      for (const auto & [module_name, source] : module->module_sources)
      {
        if ((module_name == module->name) && used_module_sources.insert(source).second)
        {
          json_used_module_sources.Add().SetAsString(source);
          added_source = true;
        }
      }
      if (!added_source && !module->source_dependencies.empty())
      {
        const string & source = module->source_dependencies.front().filename;
        if (used_module_sources.insert(source).second) json_used_module_sources.Add().SetAsString(source);
      }
    }
  }
  
  if (g_module)
  {
    for (const ODecl * declaration : g_module->declarations)
    {
      if (!declaration || DK_TYPE != declaration->kind || !declaration->ptype) continue;
      if (declaration->ptype->IsCompound())
      {
        OCompoundType * ctype = static_cast<OCompoundType *>(declaration->ptype);
        TJsonNode & json_symbols = json_namespaces.Add(ctype->name).GetAsArray();
        for (const auto & [name, type] : ctype->member_scope.typesyms)
        {
          if (!type || name.empty() || name.starts_with("__dq_")) continue;
          TJsonNode & json_symbol = json_symbols.Add().GetAsObject();
          json_symbol.Add("name", name);
          json_symbol.Add("kind", TypeKind(type->kind));
        }
        for (const auto & [name, valsym] : ctype->member_scope.valsyms)
        {
          if (!valsym || valsym->member_visibility != MV_PUBLIC || name.empty() || name.starts_with("__dq_")) continue;
          TJsonNode & json_symbol = json_symbols.Add().GetAsObject();
          json_symbol.Add("name", name);
          json_symbol.Add("kind", ValSymKind(valsym->kind));
        }
      }
    }
  }
  
  ofstream output(filename, ios::binary | ios::trunc);
  if (!output)
  {
    rerror = "Can not write language-server semantic result: " + filename;
    return false;
  }
  string json = result.GetAsJson(true);
  output.write(json.data(), static_cast<streamsize>(json.size()));
  if (!output)
  {
    rerror = "Can not finish language-server semantic result: " + filename;
    return false;
  }
  return true;
}
