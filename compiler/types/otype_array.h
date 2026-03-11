/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_array.h
 * authors: nvitya
 * created: 2026-03-06
 * brief:   Array types: fixed-size array and array slice (descriptor)
 */

#pragma once

#include "symbols.h"

class OValueArray : public OValue
{
private:
  using        super = OValue;

public:
  vector<OValue *> elements;

  OValueArray(OTypeArray * atype);
  ~OValueArray() override;

  LlConst *  CreateLlConst() override;
  bool       CalculateConstant(OExpr * expr) override;
};

// Fixed-size array type, e.g. int[3]
// LLVM representation: [N x T], e.g. [3 x i32]

class OTypeArray : public OType
{
private:
  using        super = OType;

public:
  OType *      elemtype;
  uint32_t     arraylength;

  OTypeArray(OType * aelemtype, uint32_t alength)
  :
    super(aelemtype->name + "[" + to_string(alength) + "]", TK_ARRAY),
    elemtype(aelemtype),
    arraylength(alength)
  {
    bytesize = aelemtype->bytesize * alength;
  }

  OValue * CreateValue() override;
  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
};

// Array slice / descriptor type, e.g. int[]
// LLVM representation: {ptr, i64}
// Used for function parameters that accept arrays of any size

class OTypeArraySlice : public OType
{
private:
  using        super = OType;

public:
  OType *      elemtype;

  OTypeArraySlice(OType * aelemtype)
  :
    super(aelemtype->name + "[]", TK_ARRAY_SLICE),
    elemtype(aelemtype)
  {
    bytesize = TARGET_PTRSIZE * 2;  // pointer + length
  }

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
};
