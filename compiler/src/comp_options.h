/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    comp_options.h
 * authors: nvitya
 * created: 2026-02-27
 * brief:   compiler options
 */

#pragma once

#include <vector>
#include <string>

using namespace std;

class OCompOptions
{
public:
  bool     blockmode_braces = false;
  bool     dbg_info = true;

  // include dirs
  // module dirs

  OCompOptions();
};

extern OCompOptions  g_opt;