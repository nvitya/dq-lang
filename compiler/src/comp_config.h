/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    comp_config.h
 * authors: nvitya
 * created: 2026-02-03
 * brief:   compiler configuration (defines), defaults to the current GCC platform
 */

#pragma once

#include "target_config.h"

#define CONF_DEBUG_INFO  0

#if defined(_WIN32) || defined(_WIN64)

  #define HOST_WIN

#elif defined(__linux__)

  #define HOST_LINUX

#else

  #error "Unsupported host os"

#endif

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)

  #define HOST_X86

#elif defined(__aarch64__) || defined(__arm__) || defined(_M_ARM64) || defined(_M_ARM)

  #define HOST_ARM

#elif defined(__riscv)

  #define HOST_RISCV

#else
  #error "unsupported host architecture"
#endif


#if defined(__LP64__) || defined(_WIN64)
  #define HOST_64BIT
  #define HOST_BITS     64
  #define HOST_PTRSIZE   8
#else
  #define HOST_32BIT
  #define HOST_BITS     32
  #define HOST_PTRSIZE   4
#endif


#define TARGET_BITS     (g_target.pointer_size * 8)
#define TARGET_PTRSIZE  (g_target.pointer_size)
