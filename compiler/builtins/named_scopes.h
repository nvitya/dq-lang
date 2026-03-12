/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    named_scopes.h
 * authors: nvitya
 * created: 2026-03-12
 * brief:   Named scopes for @namespace.identifier resolutions
 */

 #pragma once

#include <map>
#include <string>

#include "symbols.h"

using namespace std;

extern map<string, OScope *> g_namespaces;

void init_named_scopes();
