/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    module_path.h
 * authors: nvitya
 * created: 2026-05-10
 * brief:   DQ module path resolution
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

using namespace std;

class OModulePath
{
public:
  enum class EKind
  {
    LOCAL_RELATIVE,
    ROOT_RELATIVE,
    PACKAGE
  };

public:
  string            source_text;
  string            namespace_name;
  EKind             kind = EKind::PACKAGE;

  filesystem::path  root_dir;
  string            package_name;
  string            local_path;
  string            module_id;
  filesystem::path  source_path;
  filesystem::path  artifact_path;
  filesystem::path  interface_artifact_path;

public:
  void Clear();

  bool InitCurrent(const filesystem::path & asource_path, string & rerror);
  bool ParseUsePath(const string & apath_text, string & rerror);
  bool ResolveFrom(const OModulePath & current, string & rerror);

  bool IsLocalReference() const;

  static bool IsRtlPackageSubmodule(const string & module_id);
  static bool SuppressesImplicitSys(const string & module_id);
  static bool ResolveCanonicalArtifact(const string & module_id, const string & context_module_id,
                                       const filesystem::path & context_artifact,
                                       filesystem::path & rartifact_path);
  static filesystem::path BuildRootDir();
  static filesystem::path BuildTagDir();
  static filesystem::path BuildArtifactPath(const filesystem::path & source_path);
  static filesystem::path BuildInterfaceArtifactPath(const filesystem::path & source_path);

private:
  static filesystem::path AbsNorm(const filesystem::path & path);
  static vector<string> Split(const string & path);
  static string Join(const vector<string> & items, size_t start = 0);
  static filesystem::path SourcePathForLocal(const filesystem::path & root_dir, const string & local_path);
  static filesystem::path BuildArtifactPathForModule(const string & package_name, const string & local_path,
                                                     const filesystem::path & root_dir, bool interface_only);
  static bool IsPackageRoot(const string & package_name, const filesystem::path & root_dir);
  static string ModuleIdFromPackageLocal(const string & package_name, const string & local_path);
  static bool NormalizeLocalPath(vector<string> & stack, const vector<string> & suffix,
                                 const string & source_text, string & rerror);
};
