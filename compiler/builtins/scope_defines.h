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

#pragma once

#include "symbols.h"

class OScopeDefines : public OScope
{
private:
  using         super = OScope;

public:

  OScopeDefines()
  :
    super(nullptr, "defines")
  {
  }

  void Init();

  bool Defined(const string aname);

};

extern OScopeDefines *  g_defines;

void init_scope_defines();