/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    executable_path.h
 * authors: Codex
 * created: 2026-06-29
 * brief:   current executable path helpers
 */

#pragma once

#include <string>

using namespace std;

string CurrentExecutablePath(const string & argv0, const string & fallback_name);
string CurrentExecutableDir(const string & argv0 = "");
