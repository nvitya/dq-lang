/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_cstring.h
 * authors: nvitya
 * created: 2026-03-08
 * brief:   C-string type: null-terminated fixed-size char buffer
 */

#pragma once

#include <map>
#include "symbols.h"

class OCStringLit;  // forward declaration

// OValueCString: compile-time string value for global variable initialization
class OValueCString : public OValue
{
private:
  using        super = OValue;

public:
  string       value;     // the string content (without padding)
  uint32_t     maxlen;    // storage size (from the cstring[N] type)

  OValueCString(OType * atype, uint32_t amaxlen)
  :
    super(atype),
    maxlen(amaxlen)
  {
  }

  LlConst *  CreateLlConst() override;
  bool       CalculateConstant(OExpr * expr, bool emit_errors = true) override;
};

// OTypeCString: C-compatible null-terminated string type
//   maxlen > 0: fixed-size buffer cstring[N], LLVM type = [N x i8]
//   maxlen == 0: unsized (for parameters), LLVM type = {ptr, i64} (descriptor)

class OTypeCString : public OType
{
private:
  using        super = OType;

  map<uint32_t, OTypeCString *>  sized_types;  // cached sized variants

public:
  uint32_t     maxlen;

  OTypeCString(uint32_t amaxlen)
  :
    super(amaxlen > 0 ? "cstring[" + to_string(amaxlen) + "]" : "cstring", TK_STRING),
    maxlen(amaxlen)
  {
    if (amaxlen > 0)
    {
      bytesize = amaxlen;
    }
    else
    {
      bytesize = TARGET_PTRSIZE * 2;  // descriptor: ptr + size
    }
  }

  ~OTypeCString()
  {
    for (auto & [len, st] : sized_types)
    {
      delete st;
    }
  }

  OTypeCString * GetSizedType(uint32_t amaxlen)
  {
    auto it = sized_types.find(amaxlen);
    if (it != sized_types.end())
    {
      return it->second;
    }
    OTypeCString * result = new OTypeCString(amaxlen);
    sized_types[amaxlen] = result;
    return result;
  }

  OValue * CreateValue() override
  {
    return new OValueCString(this, maxlen);
  }

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
};
