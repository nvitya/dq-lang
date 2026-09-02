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

enum ELtoMode
{
  LTOMODE_OFF,
  LTOMODE_FULL
};

enum EOptimizationLevel
{
  OPTLEVEL_O0 = 0,
  OPTLEVEL_O1 = 1,
  OPTLEVEL_O2 = 2,
  OPTLEVEL_O3 = 3,
  OPTLEVEL_OS,
  OPTLEVEL_OZ
};

enum ETargetPlatform
{
  TARGET_PLATFORM_LINUX,
  TARGET_PLATFORM_WIN,
  TARGET_PLATFORM_WASI,
  TARGET_PLATFORM_BARE
};

enum ETargetFloatAbi
{
  TARGET_FLOAT_ABI_DEFAULT,
  TARGET_FLOAT_ABI_SOFT,
  TARGET_FLOAT_ABI_HARD
};

enum ECompLinkMode
{
  DQC_LINK_AUTO,
  DQC_LINK_COMPILE_ONLY,
  DQC_LINK_FORCE
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
  string clang_fpu;
  string clang_arch;
  string llvm_abi;
  string llvm_backend = "native";
  string gcc_multilib;
  uint32_t pointer_size = 8;
  uint8_t default_float_bits = 64;
  ETargetPlatform platform = TARGET_PLATFORM_LINUX;
  ETargetFloatAbi float_abi = TARGET_FLOAT_ABI_DEFAULT;
  bool exceptions_supported = true;
  bool default_exceptions = true;
  bool default_dynstrings = true;
  bool static_relocation = false;

  bool IsWindows() const { return TARGET_PLATFORM_WIN == platform; }
  bool IsLinux() const { return TARGET_PLATFORM_LINUX == platform; }
  bool IsWasi() const { return TARGET_PLATFORM_WASI == platform; }
  bool IsBare() const { return TARGET_PLATFORM_BARE == platform; }
  bool IsArm() const { return "ARM" == llvm_backend; }
  bool IsWasm() const { return "WebAssembly" == llvm_backend; }
  bool IsRiscV() const { return "RISCV" == llvm_backend; }

  void AppendCpuFeatures(const string & features);
  void ConfigureHost();
  bool Configure(const string & name, string & rerror);
  bool ConfigureFromCommandLine(int argc, char ** argv, string & rerror,
                                const string & default_name = "");
  static vector<OCompTarget> CanonicalTargets();
  static void PrintSupportedTargets();
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
  bool     exceptions = true;
  bool     exceptions_explicit = false;
  bool     dynstrings = true;
  bool     dynstrings_explicit = false;
  ECompLinkMode link_mode = DQC_LINK_AUTO;
  EOptimizationLevel optlevel = OPTLEVEL_O1;
  ELtoMode lto_mode = LTOMODE_OFF;

  bool     ifgen  = false;  // --ifgen
  bool     ifdump = false;  // --ifdump
  bool     langserver = false;  // --langserver
  bool     langserver_worker = false;  // internal language-server compiler worker
  bool     diagnostic_json = false;  // internal JSON-lines diagnostics
  bool     no_use_sys = false;  // --no-use-sys
  bool     regen_if_stale = false;  // internal child module regeneration mode
  int      module_root_depth = 0;

  string   compiler_executable;
  string   compiler_executable_dir;
  string   project_filename;
  string   project_main_filename;
  string   project_output_filename;
  bool     project_has_output = false;
  string   input_filename;
  string   explicit_output;
  bool     has_dash_o = false;
  bool     has_output = false;
  string   build_root_dir;
  string   package_build_root_dir;
  string   build_tag;
  vector<string>  module_use_stack;
  vector<string>  package_paths;
  string          module_root_dir;
  string          module_name;
  string          source_overlay_filename;

  vector<OCmdLineDefine>  cmdline_defines;
  vector<string>          link_libraries;
  vector<string>          link_objects;
  vector<string>          linker_args;

  string          compiler_runtime;
  string          c_runtime;
  string          cpu_features;

  // include dirs
  // module dirs

  OCompOptions();

  void ApplyTargetDefaults();
  string ProcessCommandLineOpts(int argc, char ** argv);
  static void PrintUsage();
  static bool CommandLineOptionHasValue(const string & option);

  void InitializeCompilerExecutable(const string & argv0);
  vector<string> DefaultPackagePaths() const;
  bool SetCompilerRuntime(const string & value, string & rerror);
  bool SetCRuntime(const string & value, string & rerror);
  bool ValidateTargetSettings(string & rerror) const;
  bool ValidateRuntimeSettings(string & rerror) const;
  string EffectiveCompilerRuntime() const;
  string EffectiveCRuntime() const;
  bool ResolveGccMultilibDir(string & rpath, string & rerror) const;
  const char * OptimizationLevelName() const;
  bool OptimizesForSize() const
  {
    return (OPTLEVEL_OS == optlevel) || (OPTLEVEL_OZ == optlevel);
  }

  bool ShouldLink(bool has_app_main) const
  {
    if (DQC_LINK_FORCE == link_mode) return true;
    if (DQC_LINK_COMPILE_ONLY == link_mode) return false;
    return !target.IsBare() && has_app_main;
  }

  void SetExceptions(bool value) { exceptions = value; exceptions_explicit = true; }
  void SetDynStrings(bool value) { dynstrings = value; dynstrings_explicit = true; }

private:
  static bool IsValidDefineName(const string & name);
  static bool ParseDefineIntValue(const string & text, int64_t & rvalue);
  static bool ParseDefineBoolValue(const string & text, bool & rvalue);
  static void ParseModuleUseStack(const string & text, vector<string> & rstack);
  string ParseLtoMode(const string & text);
};

extern OCompOptions  g_opt;
