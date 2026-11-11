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
#include "dq_utils.h"
#include "strparse.h"

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
    return SourcePathIdentity(g_opt.build_root_dir);
  }
  return SourcePathIdentity(filesystem::current_path());
}

filesystem::path OModulePath::BuildTagDir()
{
  filesystem::path result = BuildRootDir() / ".dqbuild";
  result /= (g_opt.build_tag.empty() ? "default" : g_opt.build_tag);
  return result.lexically_normal();
}

filesystem::path OModulePath::PackageBuildRootDir()
{
  if (!g_opt.package_build_root_dir.empty())
  {
    return AbsNormPath(g_opt.package_build_root_dir);
  }
  return BuildRootDir();
}

filesystem::path OModulePath::PackageBuildTagDir()
{
  filesystem::path result = PackageBuildRootDir() / ".dqbuild";
  result /= (g_opt.build_tag.empty() ? "default" : g_opt.build_tag);
  return result.lexically_normal();
}

bool OModulePath::IsPackageRoot(const string & package_name, const filesystem::path & root_dir)
{
  filesystem::path selected_root;
  return ResolvePackageRoot(package_name, g_opt.package_paths, selected_root)
      && SourcePathIdentity(selected_root) == SourcePathIdentity(root_dir);
}

filesystem::path OModulePath::LocalRootForSource(const filesystem::path & source)
{
  filesystem::path root = BuildRootDir();
  filesystem::path relative = source.lexically_relative(root);
  if (relative.empty()) return source.root_path();
  while (*relative.begin() == "..")
  {
    root = root.parent_path();
    relative = source.lexically_relative(root);
  }
  return root;
}

bool OModulePath::FindPackage(const filesystem::path & source, string & rpackage,
                              filesystem::path & rroot)
{
  filesystem::path identity = SourcePathIdentity(source);
  for (auto it = g_opt.package_paths.rbegin(); it != g_opt.package_paths.rend(); ++it)
  {
    filesystem::path search_root = SourcePathIdentity(*it);
    auto match = [&](const string & name) -> bool
    {
      filesystem::path selected;
      if (!ResolvePackageRoot(name, g_opt.package_paths, selected)) return false;
      filesystem::path root = SourcePathIdentity(search_root / name);
      // A shadowed package must not acquire the active package's identity.
      if (selected != root) return false;
      filesystem::path relative = identity.lexically_relative(root);
      if (relative.empty() || *relative.begin() == ".." || relative == ".") return false;
      rpackage = name;
      rroot = root;
      return true;
    };

    filesystem::path relative = identity.lexically_relative(search_root);
    if (!relative.empty() && *relative.begin() != ".." && !relative.parent_path().empty()
        && match(relative.begin()->string())) return true;

    // A package symlink may point outside its search directory entirely.
    error_code ec;
    filesystem::directory_iterator entry(search_root, ec), end;
    while (!ec && entry != end)
    {
      if (entry->is_symlink(ec) && match(entry->path().filename().string())) return true;
      entry.increment(ec);
    }
  }
  return false;
}

bool OModulePath::ResolvePackageRoot(const string & package_name, const vector<string> & package_paths,
                                     filesystem::path & rroot_dir)
{
  for (auto it = package_paths.rbegin(); it != package_paths.rend(); ++it)
  {
    filesystem::path candidate = AbsNormPath(filesystem::path(*it) / package_name);
    error_code ec;
    if (filesystem::is_directory(candidate, ec) && !ec)
    {
      rroot_dir = SourcePathIdentity(candidate);
      return true;
    }
  }
  return false;
}

filesystem::path OModulePath::BuildArtifactPathForModule(const string & package_name, const string & local_path,
                                                         const filesystem::path & root_dir, bool interface_only)
{
  bool is_package = IsPackageRoot(package_name, root_dir);
  filesystem::path result = (is_package ? PackageBuildTagDir() : BuildTagDir());
  if (is_package)
  {
    result /= "pkg";
    result /= package_name;
  }
  else
  {
    filesystem::path source = SourcePathForLocal(root_dir, local_path);
    filesystem::path relative = source.lexically_relative(BuildRootDir());
    if (relative.empty() || *relative.begin() == "..")
    {
      result /= "external";
      relative = source.relative_path();
      if (source.has_root_name())
      {
        string drive = source.root_name().string();
        for (char & ch : drive)
        {
          if (ch == ':' || ch == '/' || ch == '\\') ch = '_';
        }
        result /= drive;
      }
    }
    else
    {
      result /= "local";
    }
    result /= relative;
    result.replace_extension(interface_only ? ".dqm_if" : ".o");
    return result.lexically_normal();
  }

  for (const string & item : Split(local_path))
  {
    result /= item;
  }

  result += (interface_only ? ".dqm_if" : ".o");
  return result.lexically_normal();
}

filesystem::path OModulePath::BuildArtifactPath(const filesystem::path & source_path)
{
  filesystem::path source = SourcePathIdentity(source_path);
  return BuildArtifactPathForModule("", source.stem().string(), source.parent_path(), false);
}

filesystem::path OModulePath::BuildInterfaceArtifactPath(const filesystem::path & source_path)
{
  filesystem::path source = SourcePathIdentity(source_path);
  return BuildArtifactPathForModule("", source.stem().string(), source.parent_path(), true);
}

string OModulePath::ModuleIdFromPackageLocal(const string & package_name, const string & local_path)
{
  if (local_path == package_name)
  {
    return package_name;
  }
  return package_name + "/" + local_path;
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
  source_path = SourcePathIdentity(asource_path);

  if (!g_opt.module_root_dir.empty() && !g_opt.module_name.empty())
  {
    root_dir = SourcePathIdentity(g_opt.module_root_dir);
    module_id = g_opt.module_name;
    vector<string> idparts = Split(module_id);
    if (idparts.empty())
    {
      rerror = "empty module name";
      return false;
    }
    package_name = idparts[0];
    local_path = (idparts.size() == 1 ? package_name : Join(idparts, 1));
  }
  else
  {
    if (!FindPackage(source_path, package_name, root_dir))
    {
      root_dir = source_path.parent_path();
      package_name = root_dir.filename().string();
    }
    if (package_name.empty())
    {
      rerror = format("can not determine package name from module root \"{}\"", root_dir.string());
      return false;
    }

    filesystem::path source_no_ext = source_path;
    source_no_ext.replace_extension();
    local_path = source_no_ext.lexically_relative(root_dir).generic_string();
    module_id = ModuleIdFromPackageLocal(package_name, local_path);
  }

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

  // Expanded paths must obey the same grammar as paths written in source.
  TStrParseObj parser(source_text.data(), source_text.size());
  if (!parser.CheckSymbol("^/") && !parser.CheckSymbol("./"))
  {
    while (parser.CheckSymbol("../")) {}
  }
  string component;
  do
  {
    if (!parser.ReadIdentifier(component))
    {
      rerror = apath_text;
      return false;
    }
  } while (parser.CheckSymbol("/"));
  if (parser.readptr != parser.bufend)
  {
    rerror = apath_text;
    return false;
  }
  vector<string> parts = Split(apath_text);
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

    if (!ResolvePackageRoot(resolved_package_name, g_opt.package_paths, resolved_root_dir))
    {
      rerror = resolved_package_name;
      return false;
    }
  }
  else
  {
    resolved_package_name = current.package_name;
    resolved_root_dir = current.root_dir;

    if (EKind::ROOT_RELATIVE == kind)
    {
      if (!FindPackage(current.source_path, resolved_package_name, resolved_root_dir))
      {
        rerror = source_text;
        return false;
      }
      resolved_local_path = source_text.substr(2);
    }
    else
    {
      filesystem::path target = current.source_path.parent_path() / source_text;
      target += ".dq";
      target = SourcePathIdentity(target);
      if (!FindPackage(target, resolved_package_name, resolved_root_dir))
      {
        // Derive local identities from the build root, so reaching a file
        // through an external module does not give it a second identity.
        resolved_root_dir = LocalRootForSource(target);
        resolved_package_name = resolved_root_dir.filename().string();
        if (resolved_package_name.empty()) resolved_package_name = "__root";
      }
      target.replace_extension();
      resolved_local_path = target.lexically_relative(resolved_root_dir).generic_string();
    }
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
                                           const filesystem::path & source_path,
                                           filesystem::path & rartifact_path)
{
  vector<string> target_parts = Split(module_id);
  if (target_parts.empty())
  {
    return false;
  }

  string package_name = target_parts[0];
  string local_path = (target_parts.size() == 1 ? package_name : Join(target_parts, 1));

  // A canonical module name alone cannot distinguish a package import from a
  // local import when the local package happens to have the same name.  The
  // interface records the resolved source path, which preserves that origin.
  if (!source_path.empty())
  {
    filesystem::path normalized_source = SourcePathIdentity(source_path);
    filesystem::path package_dir;
    if (ResolvePackageRoot(package_name, g_opt.package_paths, package_dir)
        && normalized_source == SourcePathForLocal(package_dir, local_path))
    {
      rartifact_path = BuildArtifactPathForModule(package_name, local_path, package_dir, false);
      return true;
    }

    rartifact_path = BuildArtifactPath(normalized_source);
    return true;
  }

  filesystem::path build_tag_dir = BuildTagDir();
  filesystem::path local_dir = build_tag_dir / "local";
  filesystem::path pkg_dir = PackageBuildTagDir() / "pkg";

  vector<string> context_parts = Split(context_module_id);
  if (!context_parts.empty() && target_parts[0] == context_parts[0] && !context_artifact.empty())
  {
    filesystem::path context_path = AbsNormPath(context_artifact);
    filesystem::path local_rel = context_path.lexically_relative(local_dir);
    if (!local_rel.empty() && !local_rel.generic_string().starts_with(".."))
    {
      rartifact_path = BuildArtifactPathForModule(package_name, local_path, BuildRootDir(), false);
      return true;
    }

    filesystem::path pkg_rel = context_path.lexically_relative(pkg_dir);
    if (!pkg_rel.empty() && !pkg_rel.generic_string().starts_with(".."))
    {
      rartifact_path = PackageBuildTagDir() / "pkg" / package_name;
      for (const string & item : Split(local_path))
      {
        rartifact_path /= item;
      }
      rartifact_path += ".o";
      rartifact_path = rartifact_path.lexically_normal();
      return true;
    }
  }

  filesystem::path package_dir;
  if (ResolvePackageRoot(package_name, g_opt.package_paths, package_dir))
  {
    rartifact_path = BuildArtifactPathForModule(package_name, local_path, package_dir, false);
    return true;
  }

  return false;
}

bool OModulePath::ResolveCanonicalSource(const string & module_id, const string & context_module_id,
                                         const filesystem::path & context_artifact,
                                         const filesystem::path & context_source,
                                         const filesystem::path & recorded_source, filesystem::path & rsource_path)
{
  vector<string> target_parts = Split(module_id);
  if (target_parts.empty()) return false;

  string package_name = target_parts[0];
  string local_path = (target_parts.size() == 1 ? package_name : Join(target_parts, 1));

  vector<string> context_parts = Split(context_module_id);
  // Module IDs do not preserve whether a use was package-qualified or local.
  // The source of the current module does preserve its package root, though.
  // Keep sibling modules in that root instead of letting an unrelated package
  // search path with the same package name override a local use.
  if (!context_source.empty() && (target_parts.size() > 1) && (context_parts.size() > 1)
      && (target_parts[0] == context_parts[0]))
  {
    filesystem::path context_root = AbsNormPath(context_source);
    context_root.replace_extension();
    for (size_t i = 1; i < context_parts.size(); ++i)
    {
      context_root = context_root.parent_path();
    }
    rsource_path = SourcePathForLocal(context_root, local_path);
    return true;
  }

  filesystem::path package_dir;
  if (ResolvePackageRoot(package_name, g_opt.package_paths, package_dir))
  {
    rsource_path = SourcePathForLocal(package_dir, local_path);
    return true;
  }

  // Parent traversal can reach a local module with a different root. Its
  // identity alone cannot reconstruct that root; retain the recorded source.
  if (!recorded_source.empty())
  {
    filesystem::path source = SourcePathIdentity(recorded_source);
    filesystem::path root = LocalRootForSource(source);
    string root_name = root.filename().string();
    if (root_name.empty()) root_name = "__root";
    filesystem::path local = source.lexically_relative(root);
    local.replace_extension();
    if (ModuleIdFromPackageLocal(root_name, local.generic_string()) == module_id)
    {
      rsource_path = source;
      return true;
    }
  }

  filesystem::path local_dir = BuildTagDir() / "local";
  filesystem::path context_path = AbsNormPath(context_artifact);
  filesystem::path local_rel = context_path.lexically_relative(local_dir);
  if (!context_parts.empty() && target_parts[0] == context_parts[0]
      && !local_rel.empty() && !local_rel.generic_string().starts_with(".."))
  {
    rsource_path = SourcePathForLocal(BuildRootDir(), local_path);
    return true;
  }
  return false;
}
