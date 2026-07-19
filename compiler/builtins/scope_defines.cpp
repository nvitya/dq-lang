/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
  OScPosition scpos;

  if (g_opt.target.IsWindows())
  {
    DefineValSym(g_builtins->type_bool->CreateConst(scpos, "WINDOWS", true));
  }

  if (g_opt.target.IsLinux())
  {
    DefineValSym(g_builtins->type_bool->CreateConst(scpos, "LINUX", true));
  }

  if (g_opt.target.IsBare())
  {
    DefineValSym(g_builtins->type_bool->CreateConst(scpos, "BARE", true));
  }

  if (g_opt.target.IsArm())
  {
    DefineValSym(g_builtins->type_bool->CreateConst(scpos, "ARM", true));
  }

  if (4 == g_opt.target.pointer_size)
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
