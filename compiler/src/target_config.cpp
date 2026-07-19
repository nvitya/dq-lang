/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    target_config.cpp
 * authors: nvitya
 * created: 2026-07-19
 * brief:   compiler target configuration
 */

#include "target_config.h"

OTargetConfig g_target;

static void ConfigureHostTarget()
{
  g_target = OTargetConfig();

#if defined(_WIN32) || defined(_WIN64)
  g_target.platform = ETargetPlatform::WIN;
  g_target.platform_name = "win";
#else
  g_target.platform = ETargetPlatform::LINUX;
  g_target.platform_name = "linux";
#endif

#if defined(__x86_64__) || defined(_M_X64)
  g_target.arch = "x86_64";
  g_target.pointer_size = 8;
  #if defined(_WIN32) || defined(_WIN64)
    g_target.name = "x86_64-win";
    g_target.llvm_triple = "x86_64-w64-windows-gnu";
  #else
    g_target.name = "x86_64-linux";
    g_target.llvm_triple = "x86_64-unknown-linux-gnu";
  #endif
#elif defined(__i386__) || defined(_M_IX86)
  g_target.arch = "x86";
  g_target.pointer_size = 4;
  #if defined(_WIN32) || defined(_WIN64)
    g_target.name = "x86-win";
    g_target.llvm_triple = "i686-w64-windows-gnu";
  #else
    g_target.name = "x86-linux";
    g_target.llvm_triple = "i386-unknown-linux-gnu";
  #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
  g_target.arch = "aarch64";
  g_target.pointer_size = 8;
  g_target.name = "aarch64-" + g_target.platform_name;
  #if defined(_WIN32) || defined(_WIN64)
    g_target.llvm_triple = "aarch64-w64-windows-gnu";
  #else
    g_target.llvm_triple = "aarch64-unknown-linux-gnu";
  #endif
#elif defined(__arm__) || defined(_M_ARM)
  g_target.arch = "arm";
  g_target.pointer_size = 4;
  g_target.name = "arm-" + g_target.platform_name;
  #if defined(_WIN32) || defined(_WIN64)
    g_target.llvm_triple = "armv7-w64-windows-gnu";
  #else
    g_target.llvm_triple = "arm-unknown-linux-gnueabihf";
  #endif
#elif defined(__riscv)
  #if __riscv_xlen == 64
    g_target.arch = "riscv64";
    g_target.pointer_size = 8;
    g_target.name = "riscv64-linux";
    g_target.llvm_triple = "riscv64-unknown-linux-gnu";
  #else
    g_target.arch = "riscv32";
    g_target.pointer_size = 4;
    g_target.name = "riscv32-linux";
    g_target.llvm_triple = "riscv32-unknown-linux-gnu";
  #endif
#endif
}

static bool ConfigureTarget(const string & name, string & rerror)
{
  ConfigureHostTarget();
  if (name.empty())
  {
    return true;
  }

  if ((name == g_target.name)
      || (("x64-linux" == name) && ("x86_64-linux" == g_target.name))
      || (("x64-win" == name) && ("x86_64-win" == g_target.name)))
  {
    return true;
  }

  if ("arm_m7f-bare" == name)
  {
    g_target = OTargetConfig();
    g_target.name = name;
    g_target.arch = "arm_m7f";
    g_target.platform_name = "bare";
    g_target.llvm_triple = "thumbv7em-none-eabihf";
    g_target.llvm_cpu = "cortex-m7";
    g_target.llvm_features = "+fp-armv8d16,+fp-armv8d16sp,+fp64,-d32";
    g_target.llvm_backend = "ARM";
    g_target.pointer_size = 4;
    g_target.platform = ETargetPlatform::BARE;
    g_target.float_abi = ETargetFloatAbi::HARD;
    return true;
  }

  rerror = "Unsupported target \"" + name + "\"";
  return false;
}

bool ConfigureTargetFromCommandLine(int argc, char ** argv, string & rerror)
{
  string target_name;

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
    if (!target_name.empty() && (target_name != value))
    {
      rerror = "Conflicting target options \"" + target_name + "\" and \"" + value + "\"";
      return false;
    }
    target_name = value;
  }

  return ConfigureTarget(target_name, rerror);
}
