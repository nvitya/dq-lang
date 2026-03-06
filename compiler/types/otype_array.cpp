/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_array.cpp
 * authors: nvitya
 * created: 2026-03-06
 * brief:   Array types: fixed-size array and array slice (descriptor)
 */

#include <vector>
#include "otype_array.h"

using namespace std;

// OTypeArray -- Fixed-size array

LlType * OTypeArray::CreateLlType()
{
  return llvm::ArrayType::get(elemtype->GetLlType(), arraylength);
}

LlDiType * OTypeArray::CreateDiType()
{
  llvm::Metadata * subscripts[] = {
    di_builder->getOrCreateSubrange(0, arraylength)
  };
  return di_builder->createArrayType(
      bytesize * 8,
      0,
      elemtype->GetDiType(),
      di_builder->getOrCreateArray(subscripts)
  );
}

// OTypeArraySlice -- Array descriptor {ptr, i64}

LlType * OTypeArraySlice::CreateLlType()
{
  vector<LlType *> fields = {
    llvm::PointerType::get(ll_ctx, 0),
    LlType::getInt64Ty(ll_ctx)
  };
  return llvm::StructType::get(ll_ctx, fields);
}

LlDiType * OTypeArraySlice::CreateDiType()
{
  // For debug info, represent as a struct with pointer and length fields
  LlDiType * ptr_di = di_builder->createPointerType(elemtype->GetDiType(), TARGET_PTRSIZE * 8);
  LlDiType * len_di = di_builder->createBasicType("uint64", 64, llvm::dwarf::DW_ATE_unsigned);

  llvm::Metadata * elements[] = {
    di_builder->createMemberType(
        nullptr, "ptr", nullptr, 0, TARGET_PTRSIZE * 8, 0,
        0, llvm::DINode::FlagZero, ptr_di),
    di_builder->createMemberType(
        nullptr, "len", nullptr, 0, 64, 0,
        TARGET_PTRSIZE * 8, llvm::DINode::FlagZero, len_di)
  };

  return di_builder->createStructType(
      nullptr, name, nullptr, 0, bytesize * 8, 0,
      llvm::DINode::FlagZero, nullptr,
      di_builder->getOrCreateArray(elements)
  );
}
