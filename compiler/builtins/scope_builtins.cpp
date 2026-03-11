/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    scope_builtins.cpp
 * authors: nvitya
 * created: 2026-02-02
 * brief:   built-in types, variables
 */

#include "scope_builtins.h"

OScopeBuiltins *  g_builtins;

void OScopeBuiltins::Init()
{
  type_bool = new OTypeBool();
  DefineType(type_bool);

  type_func = new OTypeFunc("function");
  DefineType(type_func);

  type_int8  = new OTypeInt("int8",   8, true);
  type_int16 = new OTypeInt("int16", 16, true);
  type_int32 = new OTypeInt("int32", 32, true);
  type_int64 = new OTypeInt("int64", 64, true);
  DefineType(type_int8);
  DefineType(type_int16);
  DefineType(type_int32);
  DefineType(type_int64);

  type_uint8  = new OTypeInt("uint8",   8, false);
  type_uint16 = new OTypeInt("uint16", 16, false);
  type_uint32 = new OTypeInt("uint32", 32, false);
  type_uint64 = new OTypeInt("uint64", 64, false);
  DefineType(type_uint8);
  DefineType(type_uint16);
  DefineType(type_uint32);
  DefineType(type_uint64);

  #if defined(TARGET_32BIT)
    native_int  = type_int32;
    native_uint = type_uint32;
  #else
    native_int  = type_int64;
    native_uint = type_uint64;
  #endif
  type_int  = new OTypeAlias("int", native_int);
  type_uint = new OTypeAlias("uint", native_uint);
  DefineType(type_int);
  DefineType(type_uint);

  type_float32 = new OTypeFloat("float32", 32);
  type_float64 = new OTypeFloat("float64", 64);
  type_float   = new OTypeFloat("float", 64);
  DefineType(type_float32);
  DefineType(type_float64);
  DefineType(type_float);

  type_cchar   = new OTypeInt("cchar", 8, true);
  type_cstring = new OTypeCString(0);  // base unsized type
  DefineType(type_cchar);
  DefineType(type_cstring);

  DefineType(new OTypeAlias("byte", type_uint8));
}

void init_scope_builtins()
{
  g_builtins = new OScopeBuiltins();
  g_builtins->Init();
}
