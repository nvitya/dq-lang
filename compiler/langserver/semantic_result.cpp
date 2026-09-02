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
#include "dq_utils.h"
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
  ofstream output(filename, ios::binary | ios::trunc);
  if (!output)
  {
    rerror = "Can not write language-server semantic result: " + filename;
    return false;
  }

  output << "{\"formatVersion\":1,\"success\":" << (success ? "true" : "false")
         << ",\"documentSymbols\":[";
  bool first = true;
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

      if (!first) output << ',';
      first = false;
      const OScPosition & position = symbol->scpos;
      output << format("{{\"name\":\"{}\",\"kind\":{},\"path\":\"{}\",\"line\":{},\"column\":{}}}",
                       JsonEscape(symbol->name), SymbolKind(*declaration), JsonEscape(position.scfile->fullpath),
                       position.line, position.col);
    }
  }
  output << "],\"namespaces\":{";
  bool first_ns = true;
  for (const auto & [ns_name, scope] : g_namespaces)
  {
    if (!scope) continue;
    if (!first_ns) output << ',';
    first_ns = false;
    output << "\"" << JsonEscape(ns_name) << "\":[";
    bool first_sym = true;
    for (const auto & [name, type] : scope->typesyms)
    {
      if (!type || name.empty() || name.starts_with("__dq_")) continue;
      if (!first_sym) output << ',';
      first_sym = false;
      output << format("{{\"name\":\"{}\",\"kind\":{}}}", JsonEscape(name), TypeKind(type->kind));
    }
    for (const auto & [name, valsym] : scope->valsyms)
    {
      if (!valsym || name.empty() || name.starts_with("__dq_")) continue;
      if (!first_sym) output << ',';
      first_sym = false;
      output << format("{{\"name\":\"{}\",\"kind\":{}}}", JsonEscape(name), ValSymKind(valsym->kind));
    }
    output << "]";
  }
  
  if (g_module)
  {
    for (const ODecl * declaration : g_module->declarations)
    {
      if (!declaration || DK_TYPE != declaration->kind || !declaration->ptype) continue;
      if (declaration->ptype->IsCompound())
      {
        OCompoundType * ctype = static_cast<OCompoundType *>(declaration->ptype);
        if (!first_ns) output << ',';
        first_ns = false;
        output << "\"" << JsonEscape(ctype->name) << "\":[";
        bool first_sym = true;
        for (const auto & [name, type] : ctype->member_scope.typesyms)
        {
          if (!type || name.empty() || name.starts_with("__dq_")) continue;
          if (!first_sym) output << ',';
          first_sym = false;
          output << format("{{\"name\":\"{}\",\"kind\":{}}}", JsonEscape(name), TypeKind(type->kind));
        }
        for (const auto & [name, valsym] : ctype->member_scope.valsyms)
        {
          if (!valsym || name.empty() || name.starts_with("__dq_")) continue;
          if (!first_sym) output << ',';
          first_sym = false;
          output << format("{{\"name\":\"{}\",\"kind\":{}}}", JsonEscape(name), ValSymKind(valsym->kind));
        }
        output << "]";
      }
    }
  }
  
  output << "}}";
  if (!output)
  {
    rerror = "Can not finish language-server semantic result: " + filename;
    return false;
  }
  return true;
}
