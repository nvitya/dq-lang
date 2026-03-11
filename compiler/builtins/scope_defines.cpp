/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    scope_defines.cpp
 * authors: nvitya
 * created: 2026-02-02
 * brief:
 */

#include "comp_config.h"
#include "scope_defines.h"
#include "scope_builtins.h"

OScopeDefines *  g_defines;

void init_scope_defines()
{
  g_defines = new OScopeDefines();
  g_defines->Init();
}

void OScopeDefines::Init()
{
  // using variables here to compile check the inactive branches too
  bool target_win = false;
  bool target_linux = false;
  bool target_32bit = false;

  #if defined(TARGET_WIN)
    target_win = true;
  #elif defined(TARGET_LINUX)
    target_linux = true;
  #else
    #error "unsupported target platform"
  #endif

  OScPosition scpos;

  if (target_win)
  {
    DefineValSym(g_builtins->type_bool->CreateConst(scpos, "WINDOWS", true));
  }

  if (target_linux)
  {
    DefineValSym(g_builtins->type_bool->CreateConst(scpos, "LINUX", true));
  }

  if (target_32bit)
  {
    DefineValSym(g_builtins->type_bool->CreateConst(scpos, "TARGET_32BIT", true));
    DefineValSym(g_builtins->native_int->CreateConst(scpos, "PTRSIZE", 4));
  }
  else
  {
    DefineValSym(g_builtins->type_bool->CreateConst(scpos, "TARGET_64BIT", true));
    DefineValSym(g_builtins->native_int->CreateConst(scpos, "PTRSIZE", 8));
  }
}

bool OScopeDefines::Defined(const string aname)
{
  if (FindValSym(aname))
  {
    return true;
  }
  return false;
}
