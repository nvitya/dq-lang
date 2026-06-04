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

#include <cstddef>
#include <string>
#include <vector>
#include "stdint.h"

using namespace std;

//                                    MAJOR          MINOR
const uint32_t  DQMIF_VERSION      ( (    1 << 16) |     7 );  // generated version
const uint32_t  DQMIF_MIN_VERSION  ( (    1 << 16) |     7 );  // minimal required version

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

struct TDqmIfRec
{
  uint16_t  recid;
  uint16_t  len;
};

class ODqmIfWriter
{
public:
  vector<uint8_t>  payload;
  string           error;

  bool Ok() const { return error.empty(); }
  bool Fail(const string & amsg);

  bool AddRec(TDqmIfRecId arecid, const void * adata, size_t alen);
  bool AddRec(TDqmIfRecId arecid, const vector<uint8_t> & adata);
  bool AddRecStr(TDqmIfRecId arecid, const string & avalue);
  bool AddRecEmpty(TDqmIfRecId arecid);
  bool AddRecU8(TDqmIfRecId arecid, uint8_t avalue);
  bool AddRecU32(TDqmIfRecId arecid, uint32_t avalue);
  bool AddRecI32(TDqmIfRecId arecid, int32_t avalue);
  bool AddRecU64(TDqmIfRecId arecid, uint64_t avalue);
  bool AddRecI64(TDqmIfRecId arecid, int64_t avalue);
  bool AddTypeSpecRec(TDqmIfRecId arecid, const string & atypename);

  void AddU8(vector<uint8_t> & rdst, uint8_t avalue) const;
  void AddU16(vector<uint8_t> & rdst, uint16_t avalue) const;
  void AddU32(vector<uint8_t> & rdst, uint32_t avalue) const;
  void AddU64(vector<uint8_t> & rdst, uint64_t avalue) const;

  uint64_t Checksum(const vector<uint8_t> & adata) const;
  bool BuildFileData(vector<uint8_t> & rdata);
  bool WriteToFile(const string & filename);
};

class ODqmIfReader
{
public:
  vector<uint8_t>  payload;
  string           error;

  size_t           pos = 0;
  uint16_t         recid = 0;
  uint16_t         reclen = 0;
  size_t           recpos = 0;

  bool Ok() const { return error.empty(); }
  bool Fail(const string & amsg);

  uint64_t Checksum(const vector<uint8_t> & adata) const;
  bool ReadFromData(const vector<uint8_t> & data, const string & filename);
  bool ReadFromFile(const string & filename);
  bool ReadFromArtifact(const string & filename);

  bool Eof() const { return pos >= payload.size(); }
  bool NextRec();
  bool ExpectEmpty(TDqmIfRecId arecid);
  bool ReadString(string & rvalue);
  bool ReadU8(uint8_t & rvalue);
  bool ReadU32(uint32_t & rvalue);
  bool ReadI32(int32_t & rvalue);
  bool ReadU64(uint64_t & rvalue);
  bool ReadI64(int64_t & rvalue);
  bool ReadBlob(vector<uint8_t> & rvalue);
  bool SkipGroup(TDqmIfRecId abegin_recid, TDqmIfRecId aend_recid);
};

// DQM Record Ids

// Universal records

TDqmIfRecId  DQMIF_INVALID                = 0x0000;

TDqmIfRecId  DQMIF_TYPE_SPEC_SIMPLE       = 0x8000;  // str, simple named type spec
TDqmIfRecId  DQMIF_TYPE_SPEC_BEGIN        = 0x8001;  // 0: complex type spec begin
TDqmIfRecId  DQMIF_TYPE_SPEC_NAME         = 0x8002;  // str, named type inside complex type spec
TDqmIfRecId  DQMIF_TYPE_SPEC_PTR          = 0x8003;  // 0: pointer wrapper inside complex type spec
TDqmIfRecId  DQMIF_TYPE_SPEC_ARRAY_BEGIN  = 0x8010;  // int32: fixed array wrapper begin
TDqmIfRecId  DQMIF_TYPE_SPEC_ARRAY_END    = 0x801F;  // 0
TDqmIfRecId  DQMIF_TYPE_SPEC_SLICE_BEGIN  = 0x8020;  // 0: array slice wrapper begin
TDqmIfRecId  DQMIF_TYPE_SPEC_SLICE_END    = 0x802F;  // 0
TDqmIfRecId  DQMIF_TYPE_SPEC_DYN_ARRAY_BEGIN = 0x8030;  // 0: dynamic array wrapper begin
TDqmIfRecId  DQMIF_TYPE_SPEC_DYN_ARRAY_END   = 0x803F;  // 0
TDqmIfRecId  DQMIF_TYPE_SPEC_FUNCREF      = 0x80F0;  // 0: function reference wrapper begin
TDqmIfRecId  DQMIF_TYPE_SPEC_OBJFUNCREF   = 0x80F8;  // 0: object function reference wrapper begin
TDqmIfRecId  DQMIF_TYPE_SPEC_END          = 0x80FF;  // 0: complex type spec end

TDqmIfRecId  DQMIF_VALUE_INLINE           = 0x8100;  // blob, variable length
TDqmIfRecId  DQMIF_VALUE_LINKED           = 0x8101;  // 0: signalizes value in the object data (arrays)

TDqmIfRecId  DQMIF_ATTR_FLAGS             = 0x8200;  // uint64
TDqmIfRecId  DQMIF_ATTR_ALIGN_VALUE       = 0x8201;  // int32
TDqmIfRecId  DQMIF_ATTR_EXT_LINK_NAME     = 0x8202;  // str
TDqmIfRecId  DQMIF_ATTR_SECTION_NAME      = 0x8203;  // str
TDqmIfRecId  DQMIF_ATTR_LINK_NAME         = 0x8204;  // str

TDqmIfRecId  DQMIF_SIZE_SPEC              = 0x8304;  // int32: size specifier (structs / objects)

// 0100: Header records

TDqmIfRecId  DQMIF_H_BEGIN                = 0x0100;  // 0
TDqmIfRecId  DQMIF_H_END                  = 0x01FF;  // 0
TDqmIfRecId  DQMIF_H_SRC_FILENAME         = 0x0102;  // str
TDqmIfRecId  DQMIF_H_SRC_FILESIZE         = 0x0103;  // int64
TDqmIfRecId  DQMIF_H_SRC_FILETIME         = 0x0104;  // int64
TDqmIfRecId  DQMIF_H_TARGET_ARCH          = 0x0108;  // str
TDqmIfRecId  DQMIF_H_TARGET_RTL           = 0x0109;  // str: AKA OS
TDqmIfRecId  DQMIF_H_BUILD_OPTIONS        = 0x010A;  // str


// 0200: Type aliases

TDqmIfRecId  DQMIF_TYPE_BEGIN             = 0x0200;  // str: type alias name
// exp.:     type spec
TDqmIfRecId  DQMIF_TYPE_END               = 0x02FF;  // 0


// 0300: Constants

TDqmIfRecId  DQMIF_CONST_BEGIN            = 0x0300;  // str: constant name
// exp.:     type spec
// exp.:     DQMIF_VALUE_INLINE / DQMIF_VALUE_LINKED
TDqmIfRecId  DQMIF_CONST_END              = 0x03FF;  // 0

// 0400: Global Variables

TDqmIfRecId  DQMIF_VAR_BEGIN              = 0x0400;  // str: variable name
// opt.:     attributes (at the front only)
// exp.:     type spec
TDqmIfRecId  DQMIF_VAR_END                = 0x04FF;  // 0


// 0500: Functions

TDqmIfRecId  DQMIF_FUNC_BEGIN             = 0x0500;  // str: function name
TDqmIfRecId  DQMIF_FUNC_END               = 0x05FF;  // 0
// opt.:     attributes (at the front only)
TDqmIfRecId  DQMIF_FUNC_RETVAL            = 0x0501;  // 0, followed by type spec
TDqmIfRecId  DQMIF_FUNC_SPECIAL_KIND      = 0x0502;  // uint8: ESpecialFuncKind

// 0600: function Parameters

TDqmIfRecId  DQMIF_FUNC_PARAM_BEGIN       = 0x0600;  // str
TDqmIfRecId  DQMIF_FUNC_PARAM_MODE_REF    = 0x0613;  // 0
TDqmIfRecId  DQMIF_FUNC_PARAM_MODE_REFIN  = 0x0611;  // 0
TDqmIfRecId  DQMIF_FUNC_PARAM_MODE_REFOUT = 0x0612;  // 0
TDqmIfRecId  DQMIF_FUNC_PARAM_MODE_REFNULL = 0x0614;  // 0
// exp.:     type spec
// opt.:     DQMIF_VALUE_INLINE (for default value)
TDqmIfRecId  DQMIF_FUNC_PARAM_END         = 0x06FF;  // 0
TDqmIfRecId  DQMIF_FUNC_PARAM_VARARGS     = 0x06A0;  // 0

// 0700: Structures, objects

TDqmIfRecId  DQMIF_STRUCT_BEGIN           = 0x0700;  // str: name
TDqmIfRecId  DQMIF_STRUCT_END             = 0x07FF;  // 0
TDqmIfRecId  DQMIF_OBJ_BEGIN              = 0x0701;  // str: name
TDqmIfRecId  DQMIF_OBJ_BASE               = 0x0702;  // str: base object type name
TDqmIfRecId  DQMIF_OBJ_END                = 0x07FE;  // 0
// exp.:     DQMIF_SIZE_SPEC
TDqmIfRecId  DQMIF_FIELD_BEGIN            = 0x0710;  // str
TDqmIfRecId  DQMIF_FIELD_END              = 0x071F;  // 0
TDqmIfRecId  DQMIF_FIELD_OFFSET           = 0x0711;  // int32
// exp.:     type spec
// opt.:     DQMIF_VALUE_INLINE (for default value)
// opt.:     attributes (at the front only)

TDqmIfRecId  DQMIF_METHOD_BEGIN           = 0x0780;  // str
TDqmIfRecId  DQMIF_METHOD_END             = 0x078F;  // 0

// 0800: Linking
TDqmIfRecId  DQMIF_LINKLIB                = 0x0800;  // str, no end marker
TDqmIfRecId  DQMIF_LINKDEP                = 0x0801;  // str, module id required for final linking

// 0900: Uses
TDqmIfRecId  DQMIF_USE_BEGIN              = 0x0900;  // str
TDqmIfRecId  DQMIF_USE_ALIAS              = 0x0901;  // str
TDqmIfRecId  DQMIF_USE_ONLY               = 0x0902;  // str
TDqmIfRecId  DQMIF_USE_REEXPORT           = 0x09E0;  // 0
TDqmIfRecId  DQMIF_USE_END                = 0x09FF;  // 0

// 0A00: legacy module lifecycle
TDqmIfRecId  DQMIF_MODULE_INIT            = 0x0A00;  // str, old public linker symbol for module initialization
