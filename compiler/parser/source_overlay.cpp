/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 */

#include <chrono>
#include <fstream>
#include <sstream>

#include <llvm/Support/JSON.h>

#include "source_overlay.h"

using namespace std;

OSourceOverlay g_source_overlay;

string OSourceOverlay::Normalize(const filesystem::path & path)
{
  error_code ec;
  filesystem::path absolute_path = filesystem::absolute(path, ec);
  if (ec) absolute_path = path;
  return absolute_path.lexically_normal().string();
}

bool OSourceOverlay::Load(const string & manifest_filename, string & rerror)
{
  staged_paths.clear();
  if (manifest_filename.empty()) return true;

  ifstream input(manifest_filename, ios::binary);
  if (!input)
  {
    rerror = "Can not open source overlay manifest: " + manifest_filename;
    return false;
  }
  string text((istreambuf_iterator<char>(input)), istreambuf_iterator<char>());
  auto parsed = llvm::json::parse(text);
  if (!parsed)
  {
    rerror = "Invalid source overlay manifest: " + manifest_filename;
    return false;
  }
  llvm::json::Object * root = parsed->getAsObject();
  if (!root)
  {
    rerror = "Source overlay manifest root must be an object";
    return false;
  }
  llvm::json::Array * files = root->getArray("files");
  if (!files)
  {
    rerror = "Source overlay manifest has no files array";
    return false;
  }
  for (llvm::json::Value & value : *files)
  {
    llvm::json::Object * entry = value.getAsObject();
    if (!entry) continue;
    optional<llvm::StringRef> logical = entry->getString("source");
    optional<llvm::StringRef> staged = entry->getString("staged");
    if (!logical || !staged) continue;
    staged_paths[Normalize(logical->str())] = filesystem::path(staged->str());
  }
  return true;
}

filesystem::path OSourceOverlay::PhysicalPath(const filesystem::path & logical_path) const
{
  auto it = staged_paths.find(Normalize(logical_path));
  if (it != staged_paths.end()) return it->second;
  return logical_path;
}

bool OSourceOverlay::Exists(const filesystem::path & logical_path) const
{
  error_code ec;
  return filesystem::exists(PhysicalPath(logical_path), ec) && !ec;
}

bool OSourceOverlay::GetMetadata(const filesystem::path & logical_path, int64_t & rsize, int64_t & rfiletime) const
{
  error_code ec;
  filesystem::path physical_path = PhysicalPath(logical_path);
  uintmax_t size = filesystem::file_size(physical_path, ec);
  if (ec) return false;
  auto filetime = filesystem::last_write_time(physical_path, ec);
  if (ec) return false;
  rsize = int64_t(size);
  rfiletime = int64_t(chrono::duration_cast<chrono::nanoseconds>(filetime.time_since_epoch()).count());
  return true;
}
