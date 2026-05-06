/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqm_if.h
 * authors: nvitya
 * created: 2026-05-06
 * brief:   DQ Module Interface Definitions
 */

#pragma once

#include "stdint.h"

//                                    MAJOR          MINOR
const uint32_t  DQMIF_VERSION      ( (    1 << 16) |     0 );  // generated version
const uint32_t  DQMIF_MIN_VERSION  ( (    1 << 16) |     0 );  // minimal required version

struct TDqmIfHeader // compact global header (32 bytes)
{
  char      magic[6];              // "DQMIF\0"
  uint16_t  header_size;           // sizeof(TDqmIfHeader) for this writer
  uint32_t  version;               // format version
  uint32_t  payload_size;          // record stream size
  uint64_t  payload_hash;          // hash/checksum of record stream
  uint64_t  header_csum;           // header checksum
};

typedef const uint16_t  TDqmIfRecId;

// Universal records

TDqmIfRecId  DQMIF_INVALID                = 0x0000;

TDqmIfRecId  DQMIF_TYPE_SPEC              = 0x8000;  // str, simple type spec
TDqmIfRecId  DQMIF_TYPE_SPEC_PTR          = 0x8080;  // str, simple type spec
TDqmIfRecId  DQMIF_TYPE_SPEC_FUNC_BEGIN   = 0x80F0;  // str, simple type spec
TDqmIfRecId  DQMIF_TYPE_SPEC_END          = 0x80FF;  // 0: used only for complex types

TDqmIfRecId  DQMIF_VALUE_INLINE           = 0x8100;  // blob, variable length
TDqmIfRecId  DQMIF_VALUE_LINKED           = 0x8101;  // 0: signalizes value in the object data (arrays)

TDqmIfRecId  DQMIF_ATTR_FLAGS             = 0x8200;  // uint64
TDqmIfRecId  DQMIF_ATTR_ALIGN_VALUE       = 0x8201;  // int32
TDqmIfRecId  DQMIF_ATTR_EXT_LINK_NAME     = 0x8202;  // str
TDqmIfRecId  DQMIF_ATTR_SECTION_NAME      = 0x8203;  // str

// 0100: Header records

TDqmIfRecId  DQMIF_H_BEGIN                = 0x0100;  // 0
TDqmIfRecId  DQMIF_H_END                  = 0x01FF;  // 0
TDqmIfRecId  DQMIF_H_SRC_FILENAME         = 0x0102;  // str
TDqmIfRecId  DQMIF_H_SRC_FILESIZE         = 0x0103;  // int64
TDqmIfRecId  DQMIF_H_SRC_FILETIME         = 0x0104;  // int64
TDqmIfRecId  DQMIF_H_TARGET_ARCH          = 0x0108;  // str
TDqmIfRecId  DQMIF_H_TARGET_RTL           = 0x0109;  // str: AKA OS
TDqmIfRecId  DQMIF_H_BUILD_OPTIONS        = 0x010A;  // str


// 0200: Constants

TDqmIfRecId  DQMIF_CONST_BEGIN            = 0x0200;  // str: constant name
// exp.:     DQMIF_TYPE_SPEC
// exp.:     DQMIF_VALUE_INLINE / DQMIF_VALUE_LINKED
TDqmIfRecId  DQMIF_CONST_END              = 0x02FF;  // 0

// 0300: Type aliases

TDqmIfRecId  DQMIF_TYPE_BEGIN             = 0x0300;  // str: type alias name
// exp.:     DQMIF_TYPE_SPEC
TDqmIfRecId  DQMIF_TYPE_END               = 0x03FF;  // 0

// 0400: Functions

TDqmIfRecId  DQMIF_FUNC_BEGIN             = 0x0400;  // str: function name
TDqmIfRecId  DQMIF_FUNC_END               = 0x04FF;  // 0
// opt.:     attributes (at the front only)
TDqmIfRecId  DQMIF_FUNC_RETVAL            = 0x0401;  // str: like the DQMIF_TYPE_SPEC

// 0500: function Parameters

TDqmIfRecId  DQMIF_FUNC_PARAM_BEGIN       = 0x0500;  // str
TDqmIfRecId  DQMIF_FUNC_PARAM_MODE_REF    = 0x0513;  // 0
TDqmIfRecId  DQMIF_FUNC_PARAM_MODE_REFIN  = 0x0511;  // 0
TDqmIfRecId  DQMIF_FUNC_PARAM_MODE_REFOUT = 0x0512;  // 0
// exp.:     DQMIF_TYPE_SPEC
// opt.:     DQMIF_VALUE_INLINE (for default value)
TDqmIfRecId  DQMIF_FUNC_PARAM_END         = 0x05FF;  // 0
TDqmIfRecId  DQMIF_FUNC_PARAM_VARARGS     = 0x05A0;  // 0

// 0600: Structures, objects

TDqmIfRecId  DQMIF_STRUCT_BEGIN           = 0x0600;  // str: name
TDqmIfRecId  DQMIF_STRUCT_END             = 0x06FF;  // 0
TDqmIfRecId  DQMIF_OBJ_BEGIN              = 0x0601;  // str: name
TDqmIfRecId  DQMIF_OBJ_END                = 0x06FE;  // 0

TDqmIfRecId  DQMIF_FIELD_BEGIN            = 0x0610;  // str
TDqmIfRecId  DQMIF_FIELD_END              = 0x061F;  // 0
TDqmIfRecId  DQMIF_FIELD_OFFSET           = 0x0611;  // int32
// exp.:     DQMIF_TYPE_SPEC
// opt.:     DQMIF_VALUE_INLINE (for default value)
// opt.:     attributes (at the front only)

TDqmIfRecId  DQMIF_METHOD_BEGIN           = 0x0680;  // str
TDqmIfRecId  DQMIF_METHOD_END             = 0x068F;  // 0

// 0700: Linking
TDqmIfRecId  DQMIF_LINKLIB                = 0x0700;  // str, no end marker

// 0800: Uses
TDqmIfRecId  DQMIF_USE_BEGIN              = 0x0800;  // str
TDqmIfRecId  DQMIF_USE_ALIAS              = 0x0801;  // str
TDqmIfRecId  DQMIF_USE_ONLY               = 0x0802;  // str
TDqmIfRecId  DQMIF_USE_REEXPORT           = 0x08E0;  // 0
TDqmIfRecId  DQMIF_USE_END                = 0x08FF;  // 0


// 0900: ?