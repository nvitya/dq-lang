/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    symbols.h
 * authors: nvitya
 * created: 2026-02-01
 * brief:   Compiler Symbol Objects
 */

#include <stdexcept>
#include "string.h"
#include <format>

#include "symbols.h"
#include "dqc.h"

using namespace std;

OType * OScope::DefineType(OType * atype)
{
  auto found = typesyms.find(atype->name);
  if (found != typesyms.end())
  {
    g_compiler->Error(format("Type \"{}\" is already defined in scope \"{}\"", atype->name, this->debugname));
    return found->second;
  }

  typesyms[atype->name] = atype;
  return atype;
}

OValSym * OScope::DefineValSym(OValSym * avalsym)
{
  auto found = valsyms.find(avalsym->name);
  if (found != valsyms.end())
  {
    g_compiler->Error(format("\"{}\" is already defined in scope \"{}\"", avalsym->name, this->debugname));
    return found->second;
  }

  valsyms[avalsym->name] = avalsym;
  return avalsym;
}

OType * OScope::FindType(const string & name, OScope ** rscope, bool arecursive)
{
  auto it = typesyms.find(name);
  if (it != typesyms.end())
  {
    if (rscope)
    {
      *rscope = this;
    }
    return it->second;
  }

  // If not found here, check the parent scope
  if (arecursive and (parent_scope != nullptr))
  {
    return parent_scope->FindType(name, rscope);
  }

  return nullptr;
}

OValSym * OScope::FindValSym(const string & name, OScope ** rscope, bool arecursive)
{
  auto it = valsyms.find(name);
  if (it != valsyms.end())
  {
    if (rscope)
    {
      *rscope = this;
    }
    return it->second;
  }

  // If not found here, check the parent scope
  if (arecursive and (parent_scope != nullptr))
  {
    return parent_scope->FindValSym(name, rscope);
  }

  return nullptr;
}

void OScope::SetVarInitialized(OValSym * vs)
{
  if (not vs->initialized)
  {
    firstassign.push_back(vs);
    vs->initialized = true;
  }
}

void OScope::RevertFirstAssignments()
{
  for (OValSym * vs : firstassign)
  {
    vs->initialized = false;
  }
}

bool OScope::FirstAssigned(OValSym * avs)
{
  for (OValSym * vs : firstassign)
  {
    if (avs == vs)
    {
      return true;
    }
  }
  return false;
}

LlDiScope * OScope::GetDiScope()
{
  if (di_scope)
  {
    return di_scope;
  }

  if (parent_scope)
  {
    return parent_scope->GetDiScope();
  }

  return di_main_file;
}

OType::~OType()
{
  delete ptr_type;
}

OTypePointer * OType::GetPointerType()
{
  if (!ptr_type)
  {
    ptr_type = new OTypePointer(this);
  }
  return ptr_type;
}

OValSym * OType::CreateValSym(OScPosition & apos, const string aname)
{
  OValSym * result = new OValSym(apos, aname, this);
  return result;
}

void OValSym::GenGlobalDecl(bool apublic, OValue * ainitval)
{
  if (VSK_VARIABLE == kind)
  {
    LlLinkType  linktype =
      (apublic ? LlLinkType::ExternalLinkage
               : LlLinkType::InternalLinkage);

    LlType *          ll_type  = ptype->GetLlType();
    LlConst *         ll_init_val = ainitval->GetLlConst();

    llvm::GlobalVariable * gv = new llvm::GlobalVariable(*ll_module, ll_type, false, linktype, ll_init_val, name);
    ll_value = gv;

    if (g_opt.dbg_info)
    {
      llvm::DIGlobalVariableExpression * debug_expr = di_builder->createGlobalVariableExpression(
          di_unit,            // The scope (usually the compile unit)
          name,               // The name in the source code
          name,               // The linkage name (mangled name, if applicable)
          scpos.scfile->di_file, // The file where it is declared
          scpos.line,         // The line number in the source code (example: line 10)
          ptype->GetDiType(), // The debug type
          not apublic         // Is it local to the compile unit? (false for true globals)
      );
      gv->addDebugInfo(debug_expr);
    }

  }
}

OExpr::OExpr()
{
  ptype = g_builtins->type_int;
}
