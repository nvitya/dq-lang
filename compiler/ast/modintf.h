/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    modintf.h
 * authors: nvitya
 * created: 2026-05-04
 * brief:   DQ Module Interface Class
 */

#pragma once

#include <string>
#include <format>
#include "symbols.h"

using namespace std;

class OModIntf
{
public:
  string           name;
  OScope *         scope_pub;

  OModIntf(OScope * aparent, const string aname)
  :
    name(aname)
  {
    scope_pub = new OScope(aparent, aname);
  }

  virtual ~OModIntf()
  {
    delete scope_pub;
  }

};

