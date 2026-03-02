/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    test_symbols.cpp
 * authors: nvitya
 * created: 2026-02-01
 * brief:   Test code for symbols
 */

#include <cstdlib>
#include <unistd.h>

#include <vector>
#include <string>
#include <print>

#include "symbols.h"
#include "otype_int.h"

using namespace std;

void test_symbols()
{
  print("TEST_SYMBOLS\n");

  // 1. Create Global Scope (Root container)

  OScope * global_scope = new OScope(nullptr, "global");
  OScope * scope = global_scope;
  OScPosition scpos;

  OType * type_int = global_scope->DefineType(new OTypeInt("int", 64, true));

  OCompoundType * type_class = new OCompoundType("OMyClass", global_scope);
  scope = type_class->Members();
  scope->DefineValSym(new OValSym(scpos, "field1", type_int));
  scope->DefineValSym(new OValSym(scpos, "field2", type_int));

  global_scope->DefineType(type_class);

  // Resolution Test
  OScope * found_scope;

  OValSym * found_vs = scope->FindValSym("field1", &found_scope);
  if (found_vs)
  {
    print("Found member: {} in scope {}\n", found_vs->name, found_scope->debugname);
  }

  // If we look for 'int' inside the class, it bubbles up to Global
  OType * found_type = scope->FindType("int", &found_scope);
  if (found_type)
  {
    print("Found type: {} in scope {}\n", found_type->name, found_scope->debugname);
  }

  //OSymbol sym("int");
  //OType   ltype("myobj", TK_PRIMITIVE);

  print("to be continued...\n");
}