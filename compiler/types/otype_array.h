/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * SPDX-License-Identifier: MIT
 * See LICENSES/MIT.txt for the full license text.
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
  bool ContainsManagedStorage() const override
  {
    return elemtype && elemtype->ContainsManagedStorage();
  }
  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
};

// Array slice / descriptor type, e.g. []int
// LLVM representation: {ptr, target-native uint}
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
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;
};

// Owning dynamic array type, e.g. [*]int
// LLVM representation: nullable ODynArrMgr reference

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
    bytesize = TARGET_PTRSIZE;
    alignsize = TARGET_PTRSIZE;
  }

  void EnsureLayout() override
  {
    // The dynamic-array value is only a manager pointer. Element layout is
    // needed later by operations that allocate/copy elements, not by the
    // descriptor itself; forcing it here rejects recursive owner types.
    bytesize = TARGET_PTRSIZE;
    alignsize = TARGET_PTRSIZE;
  }
  LlType * CreateLlType() override;
  LlDiType * CreateDiType() override;
  bool ConvertFromExpr(OExpr ** rexpr, uint32_t aflags) override;
  int  GetConversionCostFromExpr(OExpr * expr, uint32_t aflags) override;

  OType *   ElementStorageType() const;
  LlValue * GetTypeInfo();

  LlValue * GenerateDataPtr(OScope * scope, LlValue * dynaddr);
  LlValue * GenerateLength(OScope * scope, LlValue * dynaddr);
  LlValue * GenerateCapacity(OScope * scope, LlValue * dynaddr);
  LlValue * GenerateRefCount(OScope * scope, LlValue * dynaddr);
  LlValue * GenerateElementAddress(OScope * scope, LlValue * dynaddr, LlValue * index);
  LlValue * GenerateSlice(OScope * scope, LlValue * dynaddr, OExpr * start_expr, OExpr * end_expr, OType * slicetype);
  void      GenerateCreate(OScope * scope, LlValue * dynaddr);
  void      GenerateDestroy(OScope * scope, LlValue * dynaddr);
  LlValue * GenerateManagerValue(OScope * scope, LlValue * dynaddr);
  void      GenerateAssignOther(OScope * scope, LlValue * dynaddr, LlValue * srcmgr);
  void      GenerateAssignData(OScope * scope, LlValue * dynaddr, LlValue * srcptr, LlValue * count);
  void      GenerateClear(OScope * scope, LlValue * dynaddr);
  void      GenerateClear(OScope * scope, LlValue * dynaddr, OExpr * free_storage);
  void      GenerateReserve(OScope * scope, LlValue * dynaddr, OExpr * min_capacity);
  void      GenerateCompact(OScope * scope, LlValue * dynaddr);
  void      GenerateSetLength(OScope * scope, LlValue * dynaddr, OExpr * new_length);
  void      GenerateSetCapacity(OScope * scope, LlValue * dynaddr, OExpr * new_capacity);
  void      GenerateAppend(OScope * scope, LlValue * dynaddr, OExpr * value);
  void      GenerateAppendSlice(OScope * scope, LlValue * dynaddr, OExpr * values);
  void      GeneratePrepend(OScope * scope, LlValue * dynaddr, OExpr * value);
  void      GeneratePrependSlice(OScope * scope, LlValue * dynaddr, OExpr * values);
  void      GenerateInsert(OScope * scope, LlValue * dynaddr, OExpr * index, OExpr * value);
  void      GenerateInsertSlice(OScope * scope, LlValue * dynaddr, OExpr * index, OExpr * values);
  void      GenerateDelete(OScope * scope, LlValue * dynaddr, OExpr * index, OExpr * count);
  LlValue * GenerateClone(OScope * scope, LlValue * dynaddr);
  LlValue * GeneratePop(OScope * scope, LlValue * dynaddr, bool first);

  bool      GenerateAssignExpr(OScope * scope, LlValue * targetaddr, OExpr * value);
  bool      GenerateAssignment(OScope * scope, LlValue * targetaddr, OExpr * value, bool volatile_store = false) override;
};
