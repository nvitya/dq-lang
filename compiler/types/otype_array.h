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
  bool       CalculateConstant(OExpr * expr, bool emit_errors = true) override;
  bool       WriteDqmIfValue(ODqmIfWriter & writer) override;
};

// Fixed-size array type, e.g. [3]int
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
    super("[" + to_string(alength) + "]" + aelemtype->name, TK_ARRAY),
    elemtype(aelemtype),
    arraylength(alength)
  {
    bytesize = aelemtype->bytesize * alength;
    alignsize = aelemtype->alignsize;
  }

  OValue * CreateValue() override;
  void EnsureLayout() override
  {
    elemtype->EnsureLayout();
    bytesize = elemtype->bytesize * arraylength;
    alignsize = elemtype->alignsize;
  }
  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
};

// Array slice / descriptor type, e.g. []int
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
    super("[]" + aelemtype->name, TK_ARRAY_SLICE),
    elemtype(aelemtype)
  {
    bytesize = TARGET_PTRSIZE * 2;  // pointer + length
    alignsize = TARGET_PTRSIZE;
  }

  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
};

// Owning dynamic array type, e.g. [*]int
// LLVM representation intentionally matches ORawDynArray: {ptr, uint, uint, uint}

class OTypeDynArray : public OType
{
private:
  using        super = OType;

public:
  OType *      elemtype;

  OTypeDynArray(OType * aelemtype)
  :
    super("[*]" + aelemtype->name, TK_DYN_ARRAY),
    elemtype(aelemtype)
  {
    bytesize = TARGET_PTRSIZE * 4;
    alignsize = TARGET_PTRSIZE;
  }

  void EnsureLayout() override
  {
    elemtype->EnsureLayout();
    bytesize = TARGET_PTRSIZE * 4;
    alignsize = TARGET_PTRSIZE;
  }
  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
};

LlValue * GenerateDynArrayDataPtr(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr);
LlValue * GenerateDynArrayLength(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr);
LlValue * GenerateDynArrayElementAddress(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, LlValue * index);
LlValue * GenerateDynArraySlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr,
                                OExpr * start_expr, OExpr * end_expr, OType * slicetype);
void GenerateDynArrayCreate(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr);
void GenerateDynArrayDestroy(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr);
void GenerateDynArrayClear(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr);
void GenerateDynArrayReserve(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * min_capacity);
void GenerateDynArrayCompact(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr);
void GenerateDynArraySetLength(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * new_length);
void GenerateDynArrayAppend(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * value);
void GenerateDynArrayAppendSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * values);
void GenerateDynArrayInsert(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * value);
void GenerateDynArrayInsertSlice(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * values);
void GenerateDynArrayDelete(OScope * scope, OTypeDynArray * dyntype, LlValue * dynaddr, OExpr * index, OExpr * count);
