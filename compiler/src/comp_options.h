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
