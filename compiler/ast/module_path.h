/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    module_path.h
 * authors: nvitya
 * created: 2026-05-10
 * brief:   DQ module path resolution helpers
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

using namespace std;

enum class EModulePathKind
{
  LOCAL_RELATIVE,
  ROOT_RELATIVE,
  PACKAGE
};

struct SCurrentModulePath
{
  filesystem::path  root_dir;
  string            package_name;
  string            local_path;
  string            module_id;
};

struct SModuleUsePath
{
  string           source_text;
  string           namespace_name;
  EModulePathKind  kind = EModulePathKind::PACKAGE;
};

struct SResolvedModulePath
{
  string            module_id;
  filesystem::path  module_root_dir;
  filesystem::path  source_path;
  filesystem::path  artifact_path;
};

bool DqComputeCurrentModulePath(const filesystem::path & source_path, SCurrentModulePath & rcur,
                                string & rerror);
bool DqParseModuleUsePath(const string & first_id, SModuleUsePath & rpath, string & rerror);
bool DqResolveModuleUsePath(const SCurrentModulePath & current, const SModuleUsePath & use_path,
                            SResolvedModulePath & rresolved, string & rerror);
bool DqResolveCanonicalModuleArtifact(const string & module_id, const string & context_module_id,
                                      const filesystem::path & context_artifact,
                                      filesystem::path & rartifact_path);
filesystem::path DqBuildArtifactPath(const filesystem::path & source_path);
