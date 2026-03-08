/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_cstring.cpp
 * authors: nvitya
 * created: 2026-03-08
 * brief:   C-string type implementation
 */

#include <vector>
#include "otype_cstring.h"
#include "expressions.h"
#include "dqc.h"

using namespace std;

// OTypeCString

LlType * OTypeCString::CreateLlType()
{
  if (maxlen > 0)
  {
    // Fixed-size: [N x i8]
    return llvm::ArrayType::get(LlType::getInt8Ty(ll_ctx), maxlen);
  }
  else
  {
    // Unsized descriptor: {ptr, i64} (same layout as array slice)
    vector<LlType *> fields = {
      llvm::PointerType::get(ll_ctx, 0),
      LlType::getInt64Ty(ll_ctx)
    };
    return llvm::StructType::get(ll_ctx, fields);
  }
}

LlDiType * OTypeCString::CreateDiType()
{
  if (maxlen > 0)
  {
    // Debug info as array of i8
    LlDiType * elem_di = di_builder->createBasicType("cchar", 8, llvm::dwarf::DW_ATE_signed_char);
    llvm::Metadata * subscripts[] = {
      di_builder->getOrCreateSubrange(0, maxlen)
    };
    return di_builder->createArrayType(
        maxlen * 8, 0, elem_di,
        di_builder->getOrCreateArray(subscripts)
    );
  }
  else
  {
    // Unsized descriptor: struct {ptr, i64}
    LlDiType * ptr_di = di_builder->createPointerType(
        di_builder->createBasicType("cchar", 8, llvm::dwarf::DW_ATE_signed_char),
        TARGET_PTRSIZE * 8);
    LlDiType * len_di = di_builder->createBasicType("uint64", 64, llvm::dwarf::DW_ATE_unsigned);

    llvm::Metadata * elements[] = {
      di_builder->createMemberType(
          nullptr, "ptr", nullptr, 0, TARGET_PTRSIZE * 8, 0,
          0, llvm::DINode::FlagZero, ptr_di),
      di_builder->createMemberType(
          nullptr, "size", nullptr, 0, 64, 0,
          TARGET_PTRSIZE * 8, llvm::DINode::FlagZero, len_di)
    };

    return di_builder->createStructType(
        nullptr, name, nullptr, 0, bytesize * 8, 0,
        llvm::DINode::FlagZero, nullptr,
        di_builder->getOrCreateArray(elements)
    );
  }
}

// OValueCString

LlConst * OValueCString::CreateLlConst()
{
  if (maxlen == 0)
  {
    return nullptr;  // unsized type has no constant representation
  }

  // Create [maxlen x i8] constant, padded with zeros
  vector<llvm::Constant *> chars;
  chars.reserve(maxlen);

  LlType * i8type = LlType::getInt8Ty(ll_ctx);

  for (uint32_t i = 0; i < maxlen; ++i)
  {
    if (i < value.size())
    {
      chars.push_back(llvm::ConstantInt::get(i8type, (uint8_t)value[i]));
    }
    else
    {
      chars.push_back(llvm::ConstantInt::get(i8type, 0));
    }
  }

  // Ensure null terminator after string content
  if (value.size() < maxlen)
  {
    // Already covered: chars[value.size()] is 0
  }

  llvm::ArrayType * arrtype = llvm::ArrayType::get(i8type, maxlen);
  return llvm::ConstantArray::get(arrtype, chars);
}

bool OValueCString::CalculateConstant(OExpr * expr)
{
  auto * strlit = dynamic_cast<OCStringLit *>(expr);
  if (strlit)
  {
    value = strlit->value;
    return true;
  }

  g_compiler->ExpressionError("CString constant expression error: string literal expected");
  return false;
}
