/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    rtlint.h
 * brief:   Compiler-side definitions shared with rtl/rtlint.dq
 */

#pragma once

#include <cstdint>

// Must match the DQTK_xxx values in stdpkg/rtl/rtlint.dq.
enum ETypeKind
{
  TK_VOID         =  0,
  TK_INT          =  1,
  TK_FLOAT        =  2,
  TK_BOOL         =  3,
  TK_POINTER      =  4,
  TK_ENUM         =  5,
  TK_CHAR         =  6,

  TK_CSTRING      =  8,
  TK_STRVIEW      =  9,
  TK_DYNSTR       = 10,
  TK_ROSTR        = 11,

  TK_ANYVALUE     = 15,

  TK_STRUCT       = 16,
  TK_OBJECT       = 17,
  TK_UNION        = 18,

  TK_ARRAY        = 20,
  TK_ARRAY_SLICE  = 21,
  TK_DYN_ARRAY    = 22,

  TK_FUNCTION     = 28,
  TK_FUNCREF      = 29,
  TK_OBJECT_TYPE  = 30,

  TK_ALIAS        = 31,
};

// SDqTextInfo and SDqRoStrInfo charlen and info bits.
inline constexpr uint32_t DQTI_MAXCHLEN_MASK    = 0x00FFFFFF;
inline constexpr uint32_t DQTIF_CHARLEN_INVALID = 0x80000000;
inline constexpr uint32_t DQTIF_READONLY        = 0x02000000;
