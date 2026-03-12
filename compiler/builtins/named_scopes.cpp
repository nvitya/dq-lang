/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    named_scopes.cpp
 * authors: nvitya
 * created: 2026-03-12
 * brief:   Named scopes for @namespace.identifier resolutions
 */

#include "named_scopes.h"

#include "dq_module.h"
#include "scope_builtins.h"
#include "scope_defines.h"

map<string, OScope *> g_namespaces;

void init_named_scopes()
{
  g_namespaces.clear();

  g_namespaces["."] = g_module->scope_priv;
  g_namespaces["def"] = g_defines;
  g_namespaces["dq"] = g_builtins;
}
