/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>

using namespace std;

bool WriteDqLanguageServerSemanticResult(const string & filename, bool success, string & rerror);
