/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    target_config.h
 * authors: nvitya
 * created: 2026-07-19
 * brief:   compiler target configuration
 */

#pragma once

#include <cstdint>
#include <string>

using namespace std;

enum class ETargetPlatform
{
  LINUX,
  WIN,
  BARE
};

enum class ETargetFloatAbi
{
  DEFAULT,
  SOFT,
  HARD
};

class OTargetConfig
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
  ETargetPlatform platform = ETargetPlatform::LINUX;
  ETargetFloatAbi float_abi = ETargetFloatAbi::DEFAULT;

  bool IsWindows() const { return ETargetPlatform::WIN == platform; }
  bool IsLinux() const { return ETargetPlatform::LINUX == platform; }
  bool IsBare() const { return ETargetPlatform::BARE == platform; }
  bool IsArm() const { return "ARM" == llvm_backend; }
};

extern OTargetConfig g_target;

bool ConfigureTargetFromCommandLine(int argc, char ** argv, string & rerror);
