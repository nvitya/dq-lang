/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 *
 * Worker-side serialization deliberately walks compiler declarations.  It does
 * not reparse source text: the result only contains facts produced by the DQ
 * parser and semantic model.
 */

#include <fstream>
#include <format>

#include "dq_module.h"
#include "jsontools.h"
#include "semantic_result.h"

#include "named_scopes.h"
#include "otype_compound.h"

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
      if (!symbol || symbol->name.empty() || symbol->name.starts_with("__dq_")
          || !symbol->scpos.scfile || !symbol->scpos.pos || !symbol->scpos.scfile->pstart)
      {
        continue;
      }

      const OScPosition & position = symbol->scpos;
      TJsonNode & json_symbol = document_symbols.Add().GetAsObject();
      json_symbol.Add("name", symbol->name);
      json_symbol.Add("kind", SymbolKind(*declaration));
      json_symbol.Add("path", position.scfile->fullpath);
      json_symbol.Add("line", position.line);
      json_symbol.Add("column", position.col);
    }
  }
  TJsonNode & json_namespaces = result.Add("namespaces").GetAsObject();
  for (const auto & [ns_name, scope] : g_namespaces)
  {
    if (!scope) continue;
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
          if (!valsym || name.empty() || name.starts_with("__dq_")) continue;
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
