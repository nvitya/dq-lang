/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_embstr.h
 * authors: nvitya
 * created: 2026-03-08
 * brief:   C-string type: null-terminated fixed-size char buffer
 */

#pragma once

#include <map>
#include <vector>
#include "symbols.h"

class OEmbStrLit;  // forward declaration

// OValueEmbStr: compile-time string value for global variable initialization
class OValueEmbStr : public OValue
{
private:
  using        super = OValue;

public:
  string       value;     // the string content (without padding)
  uint32_t     maxlen;    // storage size from embstr(N); content holds at most maxlen - 1 bytes

  OValueEmbStr(OType * atype, uint32_t amaxlen)
  :
    super(atype),
    maxlen(amaxlen)
  {
  }

  LlConst *  CreateLlConst() override;
  bool       CalculateConstant(OExpr * expr, bool emit_errors = true) override;
  bool       WriteDqmIfValue(ODqmIfWriter & writer) override;
};

enum EEmbStrMetaField
{
  ESMF_LENGTH,
  ESMF_MAXLENGTH,
  ESMF_STORAGE_SIZE,
  ESMF_PCHAR
};

enum EEmbStrMethod
{
  ESM_CLEAR,
  ESM_SET,
  ESM_APPEND,
  ESM_PREPEND,
  ESM_INSERT,
  ESM_DELETE,
  ESM_ADDFMT
};

// OTypeEmbStr: embedded, null-terminated string type
//   maxlen > 0: fixed-size buffer embstr(N), LLVM type = [N x i8]
//   maxlen == 0: unsized alias, LLVM type = pointer to a shared SDqTextInfo descriptor

class OTypeEmbStr : public OType
{
private:
  using        super = OType;

  map<uint32_t, OTypeEmbStr *>  sized_types;  // cached sized variants
  map<LlValue *, LlValue *>      descriptor_caches;  // fixed storage address -> shared descriptor

  bool IsCCharPointerType(OType * type) const;

public:
  uint32_t     maxlen;

  OTypeEmbStr(uint32_t amaxlen)
  :
    super(amaxlen > 0 ? "embstr(" + to_string(amaxlen) + ")" : "embstr", TK_EMBSTR),
    maxlen(amaxlen)
  {
    if (amaxlen > 0)
    {
      bytesize = amaxlen;
      alignsize = 1;
    }
    else
    {
      bytesize = TARGET_PTRSIZE;  // pointer to the shared descriptor
      alignsize = TARGET_PTRSIZE;
    }
  }

  ~OTypeEmbStr()
  {
    for (auto & [len, st] : sized_types)
    {
      delete st;
    }
  }

  OTypeEmbStr * GetSizedType(uint32_t amaxlen)
  {
    auto it = sized_types.find(amaxlen);
    if (it != sized_types.end())
    {
      return it->second;
    }
    OTypeEmbStr * result = new OTypeEmbStr(amaxlen);
    sized_types[amaxlen] = result;
    return result;
  }

  OValue * CreateValue() override
  {
    return new OValueEmbStr(this, maxlen);
  }

  bool CanStoreFrom(OExpr * srcexpr) const;
  bool GenerateStore(OScope * scope, LlValue * dstdaddr, OExpr * srcexpr);
  LlValue * GenerateDescriptor(OScope * scope, LlValue * embstraddr);
  void ResetDescriptorLength(OScope * scope, LlValue * embstraddr);
  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
  bool GenerateAssignment(OScope * scope, LlValue * targetaddr, OExpr * value, bool volatile_store = false) override;

  LlValue * GenerateDataPtr(OScope * scope, LlValue * embstraddr);
  LlValue * GenerateMetaField(OScope * scope, LlValue * embstraddr, EEmbStrMetaField field);
  LlValue * GenerateMethodCall(OScope * scope, LlValue * embstraddr,
                               EEmbStrMethod method, const vector<OExpr *> & args);
};
