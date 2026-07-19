/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    comp_options.h
 * authors: nvitya
 * created: 2026-02-27
 * brief:   compiler options
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>

using namespace std;

enum EVerboseLevel
{
  VERBLEVEL_NONE   = 0,   // -v0 (default)
  VERBLEVEL_STATUS = 1,   // -v or -v1
  VERBLEVEL_INFO   = 2,   // -vv or -v2
  VERBLEVEL_DEBUG  = 3,   // -vvv or -v3
};

enum class ELtoMode
{
  OFF,
  FULL
};

enum ETargetPlatform
{
  TARGET_PLATFORM_LINUX,
  TARGET_PLATFORM_WIN,
  TARGET_PLATFORM_BARE
};

enum ETargetFloatAbi
{
  TARGET_FLOAT_ABI_DEFAULT,
  TARGET_FLOAT_ABI_SOFT,
  TARGET_FLOAT_ABI_HARD
};

class OCompTarget
{
public:
  string name;
  string arch;
  string platform_name;
  string llvm_triple;
  string llvm_cpu = "generic";
  string llvm_features;
  string llvm_backend = "native";
  uint32_t pointer_size = 8;
  ETargetPlatform platform = TARGET_PLATFORM_LINUX;
  ETargetFloatAbi float_abi = TARGET_FLOAT_ABI_DEFAULT;

  bool IsWindows() const { return TARGET_PLATFORM_WIN == platform; }
  bool IsLinux() const { return TARGET_PLATFORM_LINUX == platform; }
  bool IsBare() const { return TARGET_PLATFORM_BARE == platform; }
  bool IsArm() const { return "ARM" == llvm_backend; }

  void ConfigureHost();
  bool Configure(const string & name, string & rerror);
  bool ConfigureFromCommandLine(int argc, char ** argv, string & rerror);
};

class OCmdLineDefine
{
public:
  string   name;
  bool     has_bool_value = false;
  bool     bool_value = false;
  bool     has_int_value = false;
  int64_t  int_value = 0;
};

class OCompOptions
{
public:
  OCompTarget target;

  bool     print_version = false;  // --version
  int      verblevel = VERBLEVEL_NONE;
  bool     dbg_info = false;      // -g
  bool     ir_print = false;      // -ir
  bool     compile_only = false;  // -c
  int      optlevel = 1;          // -On
  ELtoMode lto_mode = ELtoMode::OFF;

  bool     ifgen  = false;  // --ifgen
  bool     ifdump = false;  // --ifdump
  bool     no_use_sys = false;  // --no-use-sys
  bool     regen_if_stale = false;  // internal child module regeneration mode
  int      module_root_depth = 0;

  string   compiler_executable;
  string   compiler_executable_dir;
  string   build_root_dir;
  string   build_tag;
  vector<string>  module_use_stack;
  vector<string>  package_paths;
  string          module_root_dir;
  string          module_name;

  vector<OCmdLineDefine>  cmdline_defines;
  vector<string>          link_libraries;

  // include dirs
  // module dirs

  OCompOptions();
};

extern OCompOptions  g_opt;
