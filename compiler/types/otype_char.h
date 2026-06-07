/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    otype_char.h
 * authors: nvitya
 * created: 2026-06-07
 * brief:   Character type
 */

#pragma once

#include "otype_int.h"

class OTypeChar : public OTypeInt
{
private:
  using super = OTypeInt;

public:
  OTypeChar()
  :
    super("char", 32, false)
  {
  }

  LlDiType * CreateDiType() override;
};
