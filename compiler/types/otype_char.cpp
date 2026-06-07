/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_char.cpp
 * authors: nvitya
 * created: 2026-06-07
 * brief:   Character type implementation
 */

#include "otype_char.h"

LlDiType * OTypeChar::CreateDiType()
{
  return di_builder->createBasicType(name, bitlength, llvm::dwarf::DW_ATE_UTF);
}
