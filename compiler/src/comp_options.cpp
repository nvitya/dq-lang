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

  if ("arm_m7f-bare" == aname)
  {
    *this = OCompTarget();
    name = aname;
    arch = "arm_m7f";
    platform_name = "bare";
    llvm_triple = "thumbv7em-none-eabihf";
    llvm_cpu = "cortex-m7";
    llvm_features = "+fp-armv8d16,+fp-armv8d16sp,+fp64,-d32";
    llvm_backend = "ARM";
    pointer_size = 4;
    platform = TARGET_PLATFORM_BARE;
    float_abi = TARGET_FLOAT_ABI_HARD;
    return true;
  }

  rerror = "Unsupported target \"" + aname + "\"";
  return false;
}

bool OCompTarget::ConfigureFromCommandLine(int argc, char ** argv, string & rerror)
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

  return Configure(target_name, rerror);
}

OCompOptions::OCompOptions()
{
  //
}
