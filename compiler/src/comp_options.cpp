/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    comp_options.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include "comp_options.h"

#include <cstdlib>
#include <filesystem>

#include "executable_path.h"

OCompOptions  g_opt;

void OCompTarget::ConfigureHost()
{
  *this = OCompTarget();

#if defined(_WIN32) || defined(_WIN64)
  platform = TARGET_PLATFORM_WIN;
  platform_name = "win";
#else
  platform = TARGET_PLATFORM_LINUX;
  platform_name = "linux";
#endif

#if defined(__x86_64__) || defined(_M_X64)
  arch = "x86_64";
  pointer_size = 8;
  #if defined(_WIN32) || defined(_WIN64)
    name = "x86_64-win";
    llvm_triple = "x86_64-w64-windows-gnu";
  #else
    name = "x86_64-linux";
    llvm_triple = "x86_64-unknown-linux-gnu";
  #endif
#elif defined(__i386__) || defined(_M_IX86)
  arch = "x86";
  pointer_size = 4;
  #if defined(_WIN32) || defined(_WIN64)
    name = "x86-win";
    llvm_triple = "i686-w64-windows-gnu";
  #else
    name = "x86-linux";
    llvm_triple = "i386-unknown-linux-gnu";
  #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
  arch = "aarch64";
  pointer_size = 8;
  name = "aarch64-" + platform_name;
  #if defined(_WIN32) || defined(_WIN64)
    llvm_triple = "aarch64-w64-windows-gnu";
  #else
    llvm_triple = "aarch64-unknown-linux-gnu";
  #endif
#elif defined(__arm__) || defined(_M_ARM)
  arch = "arm";
  pointer_size = 4;
  name = "arm-" + platform_name;
  #if defined(_WIN32) || defined(_WIN64)
    llvm_triple = "armv7-w64-windows-gnu";
  #else
    llvm_triple = "arm-unknown-linux-gnueabihf";
  #endif
#elif defined(__riscv)
  #if __riscv_xlen == 64
    arch = "riscv64";
    pointer_size = 8;
    name = "riscv64-linux";
    llvm_triple = "riscv64-unknown-linux-gnu";
  #else
    arch = "riscv32";
    pointer_size = 4;
    name = "riscv32-linux";
    llvm_triple = "riscv32-unknown-linux-gnu";
  #endif
#endif
}

bool OCompTarget::Configure(const string & aname, string & rerror)
{
  ConfigureHost();
  if (aname.empty())
  {
    return true;
  }

  if ((aname == name)
      || (("x64-linux" == aname) && ("x86_64-linux" == name))
      || (("x64-win" == aname) && ("x86_64-win" == name)))
  {
    return true;
  }

  struct SArmBarePreset
  {
    const char * name;
    const char * arch;
    const char * triple;
    const char * cpu;
    const char * features;
    ETargetFloatAbi float_abi;
  };

  static const SArmBarePreset arm_bare_presets[] = {
    {"arm_m0-bare", "arm_m0", "thumbv6m-none-eabi", "cortex-m0",
      "-fpregs", TARGET_FLOAT_ABI_SOFT},
    {"arm_m3-bare", "arm_m3", "thumbv7m-none-eabi", "cortex-m3",
      "-fpregs", TARGET_FLOAT_ABI_SOFT},
    {"arm_m4-bare", "arm_m4", "thumbv7em-none-eabi", "cortex-m4",
      "-fpregs", TARGET_FLOAT_ABI_SOFT},
    {"arm_m4f-bare", "arm_m4f", "thumbv7em-none-eabihf", "cortex-m4",
      "+vfp4d16sp,-fp64,-d32", TARGET_FLOAT_ABI_HARD},
    {"arm_m33f-bare", "arm_m33f", "thumbv8m.main-none-eabihf", "cortex-m33",
      "+fp-armv8d16sp,-fp64,-d32", TARGET_FLOAT_ABI_HARD},
    {"arm_m7f-bare", "arm_m7f", "thumbv7em-none-eabihf", "cortex-m7",
      "+fp-armv8d16,+fp-armv8d16sp,+fp64,-d32", TARGET_FLOAT_ABI_HARD},
  };

  for (const SArmBarePreset & preset : arm_bare_presets)
  {
    if (aname != preset.name) continue;

    *this = OCompTarget();
    name = preset.name;
    arch = preset.arch;
    platform_name = "bare";
    llvm_triple = preset.triple;
    llvm_cpu = preset.cpu;
    llvm_features = preset.features;
    llvm_backend = "ARM";
    pointer_size = 4;
    platform = TARGET_PLATFORM_BARE;
    float_abi = preset.float_abi;
    return true;
  }

  rerror = "Unsupported target \"" + aname + "\"";
  return false;
}

bool OCompTarget::ConfigureFromCommandLine(int argc, char ** argv, string & rerror,
                                           const string & default_name)
{
  string command_line_target;

  for (int i = 1; i < argc; ++i)
  {
    string arg(argv[i]);
    string value;
    if (arg.starts_with("--target="))
    {
      value = arg.substr(9);
    }
    else if ("--target" == arg)
    {
      if (++i >= argc)
      {
        rerror = "Missing target name after --target";
        return false;
      }
      value = argv[i];
    }
    else
    {
      continue;
    }

    if (value.empty())
    {
      rerror = "Empty target name";
      return false;
    }
    if (!command_line_target.empty() && (command_line_target != value))
    {
      rerror = "Conflicting target options \"" + command_line_target + "\" and \"" + value + "\"";
      return false;
    }
    command_line_target = value;
  }

  return Configure(command_line_target.empty() ? default_name : command_line_target, rerror);
}

OCompOptions::OCompOptions()
{
  //
}

void OCompOptions::InitializeCompilerExecutable(const string & argv0)
{
  compiler_executable = CurrentExecutablePath(argv0, "dq-comp");
  filesystem::path path(compiler_executable);
  compiler_executable_dir = path.has_parent_path() ? path.parent_path().lexically_normal().string() : "";
}

vector<string> OCompOptions::DefaultPackagePaths() const
{
  vector<string> result;
  result.push_back("/usr/lib/dq/stdpkg");

  if (!compiler_executable_dir.empty())
  {
    filesystem::path executable_dir(compiler_executable_dir);
    result.push_back((executable_dir / ".." / "lib" / "dq" / "stdpkg").lexically_normal().string());
    result.push_back((executable_dir / ".." / "stdpkg").lexically_normal().string());
  }

  result.push_back("/usr/lib/dq/packages");
  if (!compiler_executable_dir.empty())
  {
    filesystem::path executable_dir(compiler_executable_dir);
    result.push_back((executable_dir / ".." / "lib" / "dq" / "packages").lexically_normal().string());
  }

  const char * user_home = getenv("HOME");
  if (user_home && user_home[0])
  {
    result.push_back((filesystem::path(user_home) / ".dq" / "packages").lexically_normal().string());
  }
  return result;
}
