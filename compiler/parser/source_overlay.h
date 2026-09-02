/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

using namespace std;

// Maps source paths known to the compiler to private, staged copies.  The
// compiler continues to report and record the logical path; only file access
// is redirected.  This lets an editor compile unsaved documents without
// changing the user's working tree.
class OSourceOverlay
{
public:
  bool Load(const string & manifest_filename, string & rerror);
  bool Exists(const filesystem::path & logical_path) const;
  bool GetMetadata(const filesystem::path & logical_path, int64_t & rsize, int64_t & rfiletime) const;
  filesystem::path PhysicalPath(const filesystem::path & logical_path) const;

private:
  unordered_map<string, filesystem::path> staged_paths;
};

extern OSourceOverlay g_source_overlay;
