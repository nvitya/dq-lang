/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    dqc_base.cpp
 * authors: nvitya
 * created: 2026-01-31
 * brief:
 */

#include "dqc_base.h"

ODqCompBase::ODqCompBase()
{
  scf = new OScFeederDq();
}

ODqCompBase::~ODqCompBase()
{
  delete scf;
}

const string dq_reserved_words =
   "|var|for|while|if|else|return|break|continue"
   "|and|not|or"
   "|NOT|AND|OR|XOR|IDIV|IMOD"
   "|null"
   "|function|use|implementation|initialization|finalization"
   "|struct|endstruct"
   "|"
;

bool ODqCompBase::ReservedWord(const string aname)
{
  string search_target = "|" + aname + "|";
  if (dq_reserved_words.find(search_target) != string::npos)
  {
    return true;
  }
  else
  {
    return false;
  }
}
