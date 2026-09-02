/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dq_utils.h
 * authors: nvitya
 * created: 2026-09-02
 * brief:   Generic utilities used by the DQ compiler and tools
 */

#pragma once

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <string>
#include <format>

using namespace std;

string JsonEscape(string_view text);
filesystem::path AbsNormPath(const filesystem::path & path);
int64_t FileTimeTicks(filesystem::file_time_type filetime);

#if defined(_WIN32)
string WindowsErrorMessage(uint32_t error);
#endif
