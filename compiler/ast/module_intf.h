/*
 * Copyright (c) 2026 Viktor Nagy
 * This file is part of the DQ-Compiler project at https://github.com/nvitya/dq-comp
 *
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the project root for the full license text.
 * ---------------------------------------------------------------------------------
 * file:    module_intf.h
 * authors: nvitya
 * created: 2026-05-04
 * brief:   DQ Module Interface Class
 */

#pragma once

#include <string>
#include <format>
#include "symbols.h"

using namespace std;

class OModuleIntf
{
public:
  string           name;
  OScope *         scope_pub;

  OModuleIntf(OScope * aparent, const string aname)
  :
    name(aname)
  {
    scope_pub = new OScope(aparent, aname);
  }

  virtual ~OModuleIntf()
  {
    delete scope_pub;
  }

  bool WriteInterface(const string & filename);
  bool ReadInterface(const string & filename);
};

bool DumpModuleInterface(const string & filename);
