/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    scope_builtins.h
 * authors: nvitya
 * created: 2026-02-02
 * brief:
 */

#pragma once

#include "comp_config.h"
#include "symbols.h"

#include "otype_int.h"
#include "otype_bool.h"
#include "otype_float.h"
#include "otype_func.h"
#include "otype_cstring.h"

class OScopeBuiltins : public OScope
{
private:
  using         super = OScope;

public:
  OTypeBool *   type_bool;
  OTypeFunc *   type_func;

  OTypeAlias *  type_int;
  OTypeInt *    type_int8;
  OTypeInt *    type_int16;
  OTypeInt *    type_int32;
  OTypeInt *    type_int64;

  OTypeAlias *  type_uint;
  OTypeInt *    type_uint8;
  OTypeInt *    type_uint16;
  OTypeInt *    type_uint32;
  OTypeInt *    type_uint64;

  OTypeInt *    native_int;
  OTypeInt *    native_uint;

  OTypeFloat *  type_float;
  OTypeFloat *  type_float32;
  OTypeFloat *  type_float64;

  OTypeInt *      type_cchar;
  OTypeCString *  type_cstring;

  OScopeBuiltins()
  :
    super(nullptr, "builtins")
  {
  }

  void Init();
};

extern OScopeBuiltins *  g_builtins;

void init_scope_builtins();
