/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    attributes.h
 * authors: nvitya
 * created: 2026-04-22
 * brief:   Attributes
 */

#pragma once

#include "stdint.h"
#include "scf_base.h"

enum EAttrFlag
{
  ATTF_ALIGN          = 0x00000001,
  ATTF_VOLATILE       = 0x00000002,
  ATTF_OVERLOAD       = 0x00000004,
  ATTF_PACKED         = 0x00000008,

  ATTF_EXTERNAL       = 0x00000100,
  ATTF_EXPORT         = 0x00000200,
  ATTF_CEXPORT        = 0x00000400,

  ATTF_SECTION        = 0x00001000,  // special linker section

  ATTF_VIRTUAL        = 0x00010000,
  ATTF_OVERRIDE       = 0x00020000,
  ATTF_ABSTRACT       = 0x00040000,
  ATTF_FINAL          = 0x00080000,
  ATTF_FORWARD        = 0x00100000,
  ATTF_NOWARN         = 0x00200000,
  ATTF_INLINE         = 0x00400000,
  ATTF_ALWAYS_INLINE  = 0x00800000,
  ATTF_NOINLINE       = 0x01000000,
};

enum EAttrTarget
{
  ATGT_NONE           = 0x0000,
  ATGT_FUNCTION       = 0x0001,
  ATGT_GLOBAL_VAR     = 0x0002,
  ATGT_GLOBAL_CONST   = 0x0004,

  ATGT_STRUCT_MEMBER  = 0x0008,  //TODO: member var, member func would be better
  ATGT_COMPOUND_TYPE  = 0x0010,
  ATGT_STATEMENT      = 0x0020
};

class OAttr
{
public:
  OScPosition    scpos; // start of the last attribute block

  uint64_t       flags = 0;

  int64_t        align_value = 0;
  string         external_linkage_name = "";
  string         export_linkage_name = "";
  string         section_name = "";

  void Reset();
  inline void SetFlag(EAttrFlag aflag) { flags |= aflag; }
  inline bool IsSet(EAttrFlag aflag) { return ((flags & aflag) != 0); }
  void CheckInvalidAttributes(EAttrTarget atarget);
  void CheckAttrAllowed(EAttrFlag aflag, EAttrTarget atarget, uint32_t allowed_target_mask);

};

extern string AttrName(EAttrFlag aflag);
extern string AttrTargetName(EAttrTarget atarget);
