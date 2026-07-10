/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    module_path.cpp
 * authors: nvitya
 * created: 2026-05-10
 * brief:   DQ module path resolution
 */

#include "module_path.h"

#include <format>

#include "comp_options.h"

using namespace std;

void OModulePath::Clear()
{
  source_text.clear();
  namespace_name.clear();
  kind = EKind::PACKAGE;

  root_dir.clear();
  package_name.clear();
  local_path.clear();
  module_id.clear();
  source_path.clear();
  artifact_path.clear();
  interface_artifact_path.clear();
}

filesystem::path OModulePath::AbsNorm(const filesystem::path & path)
{
  error_code ec;
  filesystem::path result = filesystem::absolute(path, ec);
  if (ec)
  {
    result = path;
  }
  return result.lexically_normal();
}

vector<string> OModulePath::Split(const string & path)
{
  vector<string> result;
  size_t start = 0;
  while (start <= path.size())
  {
    size_t slash = path.find('/', start);
    string item = (slash == string::npos ? path.substr(start) : path.substr(start, slash - start));
    if (!item.empty())
    {
      result.push_back(item);
    }
    if (slash == string::npos)
    {
      break;
    }
    start = slash + 1;
  }
  return result;
}

string OModulePath::Join(const vector<string> & items, size_t start)
{
  string result;
  for (size_t i = start; i < items.size(); ++i)
  {
    if (!result.empty()) result += "/";
    result += items[i];
  }
  return result;
}

filesystem::path OModulePath::SourcePathForLocal(const filesystem::path & root_dir, const string & local_path)
{
  filesystem::path result = root_dir;
  for (const string & item : Split(local_path))
  {
    result /= item;
  }
  result += ".dq";
  return result.lexically_normal();
}

filesystem::path OModulePath::BuildRootDir()
{
  if (!g_opt.build_root_dir.empty())
  {
    return AbsNorm(g_opt.build_root_dir);
  }
  return AbsNorm(filesystem::current_path());
}

filesystem::path OModulePath::BuildTagDir()
{
  filesystem::path result = BuildRootDir() / ".dqbuild";
  result /= (g_opt.build_tag.empty() ? "default" : g_opt.build_tag);
  return result.lexically_normal();
}

bool OModulePath::IsPackageRoot(const string & package_name, const filesystem::path & root_dir)
{
  filesystem::path normalized_root = AbsNorm(root_dir);
  for (const string & package_path : g_opt.package_paths)
  {
    filesystem::path candidate = AbsNorm(filesystem::path(package_path) / package_name);
    if (candidate == normalized_root)
    {
      return true;
    }
  }
  return false;
}

filesystem::path OModulePath::BuildArtifactPathForModule(const string & package_name, const string & local_path,
                                                         const filesystem::path & root_dir, bool interface_only)
{
  filesystem::path result = BuildTagDir();
  if (IsPackageRoot(package_name, root_dir))
  {
    result /= "pkg";
    result /= package_name;
  }
  else
  {
    result /= "local";
  }

  for (const string & item : Split(local_path))
  {
    result /= item;
  }

  result += (interface_only ? ".dqm_if" : ".dqm");
  return result.lexically_normal();
}

filesystem::path OModulePath::BuildArtifactPath(const filesystem::path & source_path)
{
  filesystem::path src = AbsNorm(source_path);
  filesystem::path source_no_ext = src;
  source_no_ext.replace_extension();

  filesystem::path rel = source_no_ext.lexically_relative(BuildRootDir());
  string local_path = rel.generic_string();
  if (local_path.empty() || local_path.starts_with(".."))
  {
    local_path = source_no_ext.filename().generic_string();
  }

  return BuildArtifactPathForModule("", local_path, BuildRootDir(), false);
}

filesystem::path OModulePath::BuildInterfaceArtifactPath(const filesystem::path & source_path)
{
  filesystem::path src = AbsNorm(source_path);
  filesystem::path source_no_ext = src;
  source_no_ext.replace_extension();

  filesystem::path rel = source_no_ext.lexically_relative(BuildRootDir());
  string local_path = rel.generic_string();
  if (local_path.empty() || local_path.starts_with(".."))
  {
    local_path = source_no_ext.filename().generic_string();
  }

  return BuildArtifactPathForModule("", local_path, BuildRootDir(), true);
}

string OModulePath::ModuleIdFromPackageLocal(const string & package_name, const string & local_path)
{
  if (local_path == package_name)
  {
    return package_name;
  }
  return package_name + "/" + local_path;
}

bool OModulePath::NormalizeLocalPath(vector<string> & stack, const vector<string> & suffix,
                                     const string & source_text, string & rerror)
{
  for (const string & item : suffix)
  {
    if ("." == item)
    {
      continue;
    }
    if (".." == item)
    {
      if (stack.empty())
      {
        rerror = source_text;
        return false;
      }
      stack.pop_back();
    }
    else
    {
      stack.push_back(item);
    }
  }

  if (stack.empty())
  {
    rerror = source_text;
    return false;
  }
  return true;
}

bool OModulePath::IsRtlPackageSubmodule(const string & module_id, const filesystem::path & root_dir)
{
  if (!module_id.starts_with("rtl/"))
  {
    return false;
  }
  return IsPackageRoot("rtl", root_dir);
}

bool OModulePath::SuppressesImplicitSys(const string & module_id, const filesystem::path & root_dir)
{
  if (module_id.starts_with("libc/"))
  {
    return IsPackageRoot("libc", root_dir);
  }
  return IsRtlPackageSubmodule(module_id, root_dir);
}

bool OModulePath::InitCurrent(const filesystem::path & asource_path, string & rerror)
{
  Clear();
  rerror.clear();
  source_path = AbsNorm(asource_path);

  if (!g_opt.module_root_dir.empty() && !g_opt.module_name.empty())
  {
    root_dir = AbsNorm(g_opt.module_root_dir);
    module_id = g_opt.module_name;
    vector<string> idparts = Split(module_id);
    if (idparts.empty())
    {
      rerror = "empty module name";
      return false;
    }
    package_name = idparts[0];
    local_path = (idparts.size() == 1 ? package_name : Join(idparts, 1));
    artifact_path = BuildArtifactPathForModule(package_name, local_path, root_dir, false);
    interface_artifact_path = BuildArtifactPathForModule(package_name, local_path, root_dir, true);
    namespace_name = Split(local_path).back();
    return true;
  }

  root_dir = source_path.parent_path();
  for (int i = 0; i < g_opt.module_root_depth; ++i)
  {
    root_dir = root_dir.parent_path();
  }
  root_dir = root_dir.lexically_normal();

  package_name = root_dir.filename().string();
  if (package_name.empty())
  {
    rerror = format("can not determine package name from module root \"{}\"", root_dir.string());
    return false;
  }

  filesystem::path source_no_ext = source_path;
  source_no_ext.replace_extension();
  filesystem::path rel = source_no_ext.lexically_relative(root_dir);
  local_path = rel.generic_string();
  if (local_path.empty() || local_path.starts_with(".."))
  {
    rerror = format("source file \"{}\" is outside module root \"{}\"", source_path.string(), root_dir.string());
    return false;
  }

  module_id = ModuleIdFromPackageLocal(package_name, local_path);
  artifact_path = BuildArtifactPathForModule(package_name, local_path, root_dir, false);
  interface_artifact_path = BuildArtifactPathForModule(package_name, local_path, root_dir, true);
  namespace_name = Split(local_path).back();
  return true;
}

bool OModulePath::ParseUsePath(const string & apath_text, string & rerror)
{
  Clear();
  rerror.clear();
  source_text = apath_text;
  namespace_name = apath_text;
  kind = EKind::PACKAGE;

  if (apath_text.starts_with("./") || apath_text.starts_with("../"))
  {
    kind = EKind::LOCAL_RELATIVE;
  }
  else if (apath_text.starts_with("^/"))
  {
    kind = EKind::ROOT_RELATIVE;
  }

  vector<string> parts = Split(apath_text);
  if (parts.empty())
  {
    rerror = apath_text;
    return false;
  }
  namespace_name = parts.back();
  return true;
}

bool OModulePath::ResolveFrom(const OModulePath & current, string & rerror)
{
  rerror.clear();

  string resolved_package_name;
  string resolved_local_path;
  filesystem::path resolved_root_dir;

  if (EKind::PACKAGE == kind)
  {
    vector<string> parts = Split(source_text);
    if (parts.empty())
    {
      rerror = source_text;
      return false;
    }

    resolved_package_name = parts[0];
    resolved_local_path = (parts.size() == 1 ? resolved_package_name : Join(parts, 1));

    bool found_package = false;
    for (auto it = g_opt.package_paths.rbegin(); it != g_opt.package_paths.rend(); ++it)
    {
      filesystem::path candidate = AbsNorm(filesystem::path(*it) / resolved_package_name);
      error_code ec;
      if (filesystem::is_directory(candidate, ec) && !ec)
      {
        resolved_root_dir = candidate;
        found_package = true;
        break;
      }
    }
    if (!found_package)
    {
      rerror = resolved_package_name;
      return false;
    }
  }
  else
  {
    resolved_package_name = current.package_name;
    resolved_root_dir = current.root_dir;

    vector<string> stack;
    if (EKind::LOCAL_RELATIVE == kind)
    {
      stack = Split(current.local_path);
      if (!stack.empty())
      {
        stack.pop_back();
      }
    }

    vector<string> suffix = Split(source_text);
    if ((EKind::ROOT_RELATIVE == kind) && !suffix.empty() && ("^" == suffix[0]))
    {
      suffix.erase(suffix.begin());
    }
    if (!NormalizeLocalPath(stack, suffix, source_text, rerror))
    {
      return false;
    }
    resolved_local_path = Join(stack);
  }

  package_name = resolved_package_name;
  local_path = resolved_local_path;
  root_dir = resolved_root_dir;
  module_id = ModuleIdFromPackageLocal(package_name, local_path);
  source_path = SourcePathForLocal(root_dir, local_path);
  artifact_path = BuildArtifactPathForModule(package_name, local_path, root_dir, false);
  interface_artifact_path = BuildArtifactPathForModule(package_name, local_path, root_dir, true);
  return true;
}

bool OModulePath::IsLocalReference() const
{
  return (EKind::LOCAL_RELATIVE == kind) || (EKind::ROOT_RELATIVE == kind);
}

bool OModulePath::ResolveCanonicalArtifact(const string & module_id, const string & context_module_id,
                                           const filesystem::path & context_artifact,
                                           filesystem::path & rartifact_path)
{
  vector<string> target_parts = Split(module_id);
  if (target_parts.empty())
  {
    return false;
  }

  string package_name = target_parts[0];
  string local_path = (target_parts.size() == 1 ? package_name : Join(target_parts, 1));

  filesystem::path build_tag_dir = BuildTagDir();
  filesystem::path local_dir = build_tag_dir / "local";
  filesystem::path pkg_dir = build_tag_dir / "pkg";

  vector<string> context_parts = Split(context_module_id);
  if (!context_parts.empty() && target_parts[0] == context_parts[0] && !context_artifact.empty())
  {
    filesystem::path context_path = AbsNorm(context_artifact);
    filesystem::path local_rel = context_path.lexically_relative(local_dir);
    if (!local_rel.empty() && !local_rel.generic_string().starts_with(".."))
    {
      rartifact_path = BuildArtifactPathForModule(package_name, local_path, BuildRootDir(), false);
      return true;
    }

    filesystem::path pkg_rel = context_path.lexically_relative(pkg_dir);
    if (!pkg_rel.empty() && !pkg_rel.generic_string().starts_with(".."))
    {
      rartifact_path = BuildTagDir() / "pkg" / package_name;
      for (const string & item : Split(local_path))
      {
        rartifact_path /= item;
      }
      rartifact_path += ".dqm";
      rartifact_path = rartifact_path.lexically_normal();
      return true;
    }
  }

  for (auto it = g_opt.package_paths.rbegin(); it != g_opt.package_paths.rend(); ++it)
  {
    filesystem::path package_dir = AbsNorm(filesystem::path(*it) / package_name);
    error_code ec;
    if (filesystem::is_directory(package_dir, ec) && !ec)
    {
      rartifact_path = BuildArtifactPathForModule(package_name, local_path, package_dir, false);
      return true;
    }
  }

  return false;
}
