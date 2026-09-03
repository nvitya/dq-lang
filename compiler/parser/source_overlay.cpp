/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 */

#include <fstream>
#include <sstream>

#include "source_overlay.h"
#include "dq_utils.h"
#include "jsontools.h"

using namespace std;

OSourceOverlay g_source_overlay;

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
  TJsonNode root;
  if (!root.TryParse(text))
  {
    rerror = "Invalid source overlay manifest: " + manifest_filename;
    return false;
  }
  if (root.GetKind() != nkObject)
  {
    rerror = "Source overlay manifest root must be an object";
    return false;
  }
  const TJsonNode * files = root.Child("files");
  if (!files || files->GetKind() != nkArray)
  {
    rerror = "Source overlay manifest has no files array";
    return false;
  }
  for (int index = 0; index < files->GetCount(); ++index)
  {
    const TJsonNode & entry = files->Child(index);
    if (entry.GetKind() != nkObject) continue;
    const TJsonNode * logical = entry.Child("source");
    const TJsonNode * staged = entry.Child("staged");
    if (!logical || !staged || logical->GetKind() != nkString || staged->GetKind() != nkString) continue;
    staged_paths[AbsNormPath(logical->GetAsString()).string()] = filesystem::path(staged->GetAsString());
  }
  return true;
}

filesystem::path OSourceOverlay::PhysicalPath(const filesystem::path & logical_path) const
{
  auto it = staged_paths.find(AbsNormPath(logical_path).string());
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
  rfiletime = FileTimeTicks(filetime);
  return true;
}
