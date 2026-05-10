/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    module_path.cpp
 * authors: nvitya
 * created: 2026-05-10
 * brief:   DQ module path resolution helpers
 */

#include "module_path.h"

#include <algorithm>
#include <format>

#include "comp_options.h"

using namespace std;

static filesystem::path AbsNorm(const filesystem::path & path)
{
  error_code ec;
  filesystem::path result = filesystem::absolute(path, ec);
  if (ec)
  {
    result = path;
  }
  return result.lexically_normal();
}

static vector<string> SplitModulePath(const string & path)
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

static string JoinModulePath(const vector<string> & items, size_t start = 0)
{
  string result;
  for (size_t i = start; i < items.size(); ++i)
  {
    if (!result.empty()) result += "/";
    result += items[i];
  }
  return result;
}

static filesystem::path SourcePathForLocal(const filesystem::path & root_dir, const string & local_path)
{
  filesystem::path result = root_dir;
  for (const string & item : SplitModulePath(local_path))
  {
    result /= item;
  }
  result += ".dq";
  return result.lexically_normal();
}

static string BuildArtifactSuffix()
{
  string suffix;
  if (g_opt.optlevel != 0)
  {
    suffix += ".O" + to_string(g_opt.optlevel);
  }
  if (g_opt.dbg_info)
  {
    suffix += ".g";
  }
  for (const OCmdLineDefine & def : g_opt.cmdline_defines)
  {
    suffix += ".D";
    suffix += def.name;
    if (def.has_bool_value)
    {
      suffix += def.bool_value ? "_true" : "_false";
    }
    else if (def.has_int_value)
    {
      suffix += "_" + to_string(def.int_value);
    }
  }

  for (char & c : suffix)
  {
    bool ok = ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'))
              || ((c >= '0') && (c <= '9')) || ('.' == c) || ('_' == c) || ('-' == c);
    if (!ok)
    {
      c = '_';
    }
  }
  return suffix;
}

filesystem::path DqBuildArtifactPath(const filesystem::path & source_path)
{
  filesystem::path result = source_path;
  string suffix = BuildArtifactSuffix();
  result.replace_extension(suffix + ".dqm");
  return result;
}

static string ModuleIdFromPackageLocal(const string & package_name, const string & local_path)
{
  if (local_path == package_name)
  {
    return package_name;
  }
  return package_name + "/" + local_path;
}

static bool NormalizeLocalPath(vector<string> & stack, const vector<string> & suffix,
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

bool DqComputeCurrentModulePath(const filesystem::path & source_path, SCurrentModulePath & rcur,
                                string & rerror)
{
  rerror.clear();
  filesystem::path abs_source = AbsNorm(source_path);

  if (!g_opt.module_root_dir.empty() && !g_opt.module_name.empty())
  {
    rcur.root_dir = AbsNorm(g_opt.module_root_dir);
    rcur.module_id = g_opt.module_name;
    vector<string> idparts = SplitModulePath(rcur.module_id);
    if (idparts.empty())
    {
      rerror = "empty module name";
      return false;
    }
    rcur.package_name = idparts[0];
    rcur.local_path = (idparts.size() == 1 ? rcur.package_name : JoinModulePath(idparts, 1));
    return true;
  }

  filesystem::path root_dir = abs_source.parent_path();
  for (int i = 0; i < g_opt.module_root_depth; ++i)
  {
    root_dir = root_dir.parent_path();
  }
  root_dir = root_dir.lexically_normal();

  string package_name = root_dir.filename().string();
  if (package_name.empty())
  {
    rerror = format("can not determine package name from module root \"{}\"", root_dir.string());
    return false;
  }

  filesystem::path source_no_ext = abs_source;
  source_no_ext.replace_extension();
  filesystem::path rel = source_no_ext.lexically_relative(root_dir);
  string local_path = rel.generic_string();
  if (local_path.empty() || local_path.starts_with(".."))
  {
    rerror = format("source file \"{}\" is outside module root \"{}\"", abs_source.string(), root_dir.string());
    return false;
  }

  rcur.root_dir = root_dir;
  rcur.package_name = package_name;
  rcur.local_path = local_path;
  rcur.module_id = ModuleIdFromPackageLocal(package_name, local_path);
  return true;
}

bool DqParseModuleUsePath(const string & first_id, SModuleUsePath & rpath, string & rerror)
{
  rerror.clear();
  rpath.source_text = first_id;
  rpath.namespace_name = first_id;
  rpath.kind = EModulePathKind::PACKAGE;

  if (first_id.starts_with("./") || first_id.starts_with("../"))
  {
    rpath.kind = EModulePathKind::LOCAL_RELATIVE;
  }
  else if (first_id.starts_with("^/"))
  {
    rpath.kind = EModulePathKind::ROOT_RELATIVE;
  }

  vector<string> parts = SplitModulePath(first_id);
  if (parts.empty())
  {
    rerror = first_id;
    return false;
  }
  rpath.namespace_name = parts.back();
  return true;
}

bool DqResolveModuleUsePath(const SCurrentModulePath & current, const SModuleUsePath & use_path,
                            SResolvedModulePath & rresolved, string & rerror)
{
  rerror.clear();

  string package_name;
  string local_path;
  filesystem::path root_dir;

  if (EModulePathKind::PACKAGE == use_path.kind)
  {
    vector<string> parts = SplitModulePath(use_path.source_text);
    if (parts.empty())
    {
      rerror = use_path.source_text;
      return false;
    }

    package_name = parts[0];
    local_path = (parts.size() == 1 ? package_name : JoinModulePath(parts, 1));

    filesystem::path package_dir;
    bool found_package = false;
    for (auto it = g_opt.package_paths.rbegin(); it != g_opt.package_paths.rend(); ++it)
    {
      filesystem::path candidate = AbsNorm(filesystem::path(*it) / package_name);
      error_code ec;
      if (filesystem::is_directory(candidate, ec) && !ec)
      {
        package_dir = candidate;
        found_package = true;
        break;
      }
    }
    if (!found_package)
    {
      rerror = package_name;
      return false;
    }
    root_dir = package_dir;
  }
  else
  {
    package_name = current.package_name;
    root_dir = current.root_dir;

    vector<string> stack;
    if (EModulePathKind::LOCAL_RELATIVE == use_path.kind)
    {
      stack = SplitModulePath(current.local_path);
      if (!stack.empty())
      {
        stack.pop_back();
      }
    }

    vector<string> suffix = SplitModulePath(use_path.source_text);
    if ((EModulePathKind::ROOT_RELATIVE == use_path.kind) && !suffix.empty() && ("^" == suffix[0]))
    {
      suffix.erase(suffix.begin());
    }
    if (!NormalizeLocalPath(stack, suffix, use_path.source_text, rerror))
    {
      return false;
    }
    local_path = JoinModulePath(stack);
  }

  rresolved.module_id = ModuleIdFromPackageLocal(package_name, local_path);
  rresolved.module_root_dir = root_dir;
  rresolved.source_path = SourcePathForLocal(root_dir, local_path);
  rresolved.artifact_path = DqBuildArtifactPath(rresolved.source_path);
  return true;
}

bool DqResolveCanonicalModuleArtifact(const string & module_id, const string & context_module_id,
                                      const filesystem::path & context_artifact,
                                      filesystem::path & rartifact_path)
{
  vector<string> target_parts = SplitModulePath(module_id);
  if (target_parts.empty())
  {
    return false;
  }

  vector<string> context_parts = SplitModulePath(context_module_id);
  if (!context_parts.empty() && target_parts[0] == context_parts[0] && !context_artifact.empty())
  {
    filesystem::path root_dir = context_artifact.parent_path();
    size_t context_local_depth = (context_parts.size() == 1 ? 1 : context_parts.size() - 1);
    for (size_t i = 1; i < context_local_depth; ++i)
    {
      root_dir = root_dir.parent_path();
    }

    string local_path = (target_parts.size() == 1 ? target_parts[0] : JoinModulePath(target_parts, 1));
    rartifact_path = DqBuildArtifactPath(SourcePathForLocal(root_dir, local_path));
    return true;
  }

  string package_name = target_parts[0];
  string local_path = (target_parts.size() == 1 ? package_name : JoinModulePath(target_parts, 1));
  for (auto it = g_opt.package_paths.rbegin(); it != g_opt.package_paths.rend(); ++it)
  {
    filesystem::path package_dir = AbsNorm(filesystem::path(*it) / package_name);
    error_code ec;
    if (filesystem::is_directory(package_dir, ec) && !ec)
    {
      rartifact_path = DqBuildArtifactPath(SourcePathForLocal(package_dir, local_path));
      return true;
    }
  }

  return false;
}
